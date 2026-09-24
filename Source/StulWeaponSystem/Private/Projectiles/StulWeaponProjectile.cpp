#include "Projectiles/StulWeaponProjectile.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StulWeaponFireComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Projectiles/ProjectileVisualComponent.h"
#include "StulWeaponSystem.h"
#include "Weapons/Ammunition/StulAmmoDefinition.h"
#include "Weapons/StulWeapon.h"

AStulWeaponProjectile::AStulWeaponProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(30.0f);

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	SetRootComponent(CollisionComponent);
	CollisionComponent->InitSphereRadius(5.0f);
	CollisionComponent->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetCanEverAffectNavigation(false);
	CollisionComponent->bReturnMaterialOnMove = true;
	CollisionComponent->CanCharacterStepUpOn = ECB_No;

	VisualComponent = CreateDefaultSubobject<UProjectileVisualComponent>(TEXT("VisualComponent"));
	VisualComponent->SetupAttachment(CollisionComponent);

	ProjectileMovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovementComponent"));
	ProjectileMovementComponent->SetUpdatedComponent(CollisionComponent);
	ProjectileMovementComponent->bInitialVelocityInLocalSpace = false;
	ProjectileMovementComponent->bRotationFollowsVelocity = true;
	ProjectileMovementComponent->bShouldBounce = false;
	ProjectileMovementComponent->ProjectileGravityScale = 0.0f;
}

/*********************************************************************************************/
/**************************************** Actor **********************************************/
/*********************************************************************************************/
void AStulWeaponProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (InstanceMode == EInstanceMode::Uninitialized && !HasAuthority())
	{
		InstanceMode = EInstanceMode::Simulated;
		bInitialized = true;
	}
	if (!InitData.IsValid())
	{
		UE_LOG(LogStulWeaponSystem, Error, TEXT("Projectile '%s' began play with invalid initialization data."), *GetNameSafe(this));
		if (HasAuthority())
		{
			Destroy();
		}
		return;
	}

	CollisionComponent->SetCollisionEnabled(InstanceMode == EInstanceMode::Authoritative || InstanceMode == EInstanceMode::Predicted ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	ProjectileMovementComponent->InitialSpeed = InitData.Speed;
	ProjectileMovementComponent->ProjectileGravityScale = FMath::Max(0.0f, InitData.GravityScale);
	ProjectileMovementComponent->Velocity = InitData.Direction.GetSafeNormal() * InitData.Speed;
	SetActorRotation(InitData.Direction.Rotation());
	FVector CosmeticOrigin = InitData.CosmeticOrigin;
	AStulWeaponProjectile* PredictedProjectile = nullptr;
	AStulWeapon* SourceWeapon = Cast<AStulWeapon>(GetOwner());
	if (InstanceMode == EInstanceMode::Simulated && HasLocalNetOwner())
	{
		if (SourceWeapon)
		{
			PredictedProjectile = SourceWeapon->FindPredictedProjectile(InitData.ProjectileId);
			if (PredictedProjectile && PredictedProjectile->GetVisualComponent()) CosmeticOrigin = PredictedProjectile->GetVisualComponent()->GetComponentLocation();
			else if (InitData.ProjectileId.IsValid()) CosmeticOrigin = GetActorLocation();
			else CosmeticOrigin = SourceWeapon->GetMuzzleTransform().GetLocation();
		}
	}
	else if (InstanceMode == EInstanceMode::Simulated)
	{
		CosmeticOrigin = ResolveSimulatedCosmeticOrigin(SourceWeapon, InitData.CosmeticOrigin);
	}
	VisualComponent->InitializeVisual(CosmeticOrigin);
	ProjectileMovementComponent->OnProjectileStop.AddDynamic(this, &ThisClass::HandleProjectileStop);
	if (PredictedProjectile)
	{
		PredictedProjectile->ConfirmPrediction();
		SetActorHiddenInGame(true);
	}
	else K2_OnProjectileInitialized(InitData);

	if (InstanceMode == EInstanceMode::Authoritative)
	{
		SetLifeSpan(InitData.MaxLifeSeconds);
	}
	else if (InstanceMode == EInstanceMode::Predicted)
	{
		SetLifeSpan(FMath::Min(InitData.MaxLifeSeconds, FMath::Max(0.1f, PredictedConfirmationTimeout)));
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawDebugTrajectory)
	{
		LastDebugLocation = GetActorLocation();
		SetActorTickEnabled(true);
	}
#endif
}

void AStulWeaponProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsPredictedProjectile())
	{
		if (AStulWeapon* SourceWeapon = Cast<AStulWeapon>(GetOwner())) SourceWeapon->UnregisterPredictedProjectile(InitData.ProjectileId, this);
	}
	if (UPrimitiveComponent* OwnerCollision = IgnoringOwnerCollisionComponent.Get()) OwnerCollision->IgnoreActorWhenMoving(this, false);
	IgnoringOwnerCollisionComponent.Reset();

	Super::EndPlay(EndPlayReason);
}

void AStulWeaponProjectile::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

#if ENABLE_DRAW_DEBUG
	if (bDrawDebugTrajectory)
	{
		const FVector CurrentLocation = GetActorLocation();
		const FColor TrajectoryColor = IsAuthoritativeProjectile() ? FColor::Orange : FColor::Green;
		DrawDebugLine(GetWorld(), LastDebugLocation, CurrentLocation, TrajectoryColor, false, FMath::Max(0.0f, DebugDrawDuration), 0, FMath::Max(0.0f, DebugLineThickness));
		LastDebugLocation = CurrentLocation;
	}
#endif
}

void AStulWeaponProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AStulWeaponProjectile, InitData, COND_InitialOnly);
}

/*********************************************************************************************/
/************************************ Initialization *****************************************/
/*********************************************************************************************/
bool AStulWeaponProjectile::InitializeProjectile(const FStulWeaponProjectileInitData& InInitData, const float InDamage, UStulAmmoDefinition* InAmmoDefinition)
{
	if (!HasAuthority() || bInitialized || !InInitData.IsValid())
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Projectile '%s' rejected an invalid or repeated initialization request."), *GetNameSafe(this));
		return false;
	}

	InitData = InInitData;
	Damage = FMath::Max(0.0f, InDamage);
	AmmoDefinition = InAmmoDefinition;
	InstanceMode = EInstanceMode::Authoritative;
	bInitialized = true;
	ConfigureCollisionIgnores();
	return true;
}

bool AStulWeaponProjectile::InitializePredictedProjectile(const FStulWeaponProjectileInitData& InInitData)
{
	if (bInitialized || !InInitData.IsValid())
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Projectile '%s' rejected an invalid or repeated predicted initialization request."), *GetNameSafe(this));
		return false;
	}

	SetReplicates(false);
	SetReplicateMovement(false);
	InitData = InInitData;
	InstanceMode = EInstanceMode::Predicted;
	bInitialized = true;
	ConfigureCollisionIgnores();
	return true;
}

void AStulWeaponProjectile::ConfirmPrediction()
{
	if (!IsPredictedProjectile() || bPredictionConfirmed) return;
	bPredictionConfirmed = true;
	SetLifeSpan(FMath::Max(0.1f, InitData.MaxLifeSeconds));
}

FVector AStulWeaponProjectile::ResolveSimulatedCosmeticOrigin(const AStulWeapon* SourceWeapon, const FVector& ReplicatedCosmeticOrigin)
{
	return SourceWeapon ? SourceWeapon->GetMuzzleTransform().GetLocation() : ReplicatedCosmeticOrigin;
}

void AStulWeaponProjectile::ConfigureCollisionIgnores()
{
	CollisionComponent->ClearMoveIgnoreActors();
	CollisionComponent->IgnoreActorWhenMoving(this, true);
	if (AActor* SourceWeapon = GetOwner())
	{
		CollisionComponent->IgnoreActorWhenMoving(SourceWeapon, true);
		if (AActor* WeaponOwner = SourceWeapon->GetOwner())
		{
			CollisionComponent->IgnoreActorWhenMoving(WeaponOwner, true);
			if (UPrimitiveComponent* OwnerCollision = Cast<UPrimitiveComponent>(WeaponOwner->GetRootComponent()))
			{
				OwnerCollision->IgnoreActorWhenMoving(this, true);
				IgnoringOwnerCollisionComponent = OwnerCollision;
			}
		}
	}
}

/*********************************************************************************************/
/**************************************** Impact *********************************************/
/*********************************************************************************************/
void AStulWeaponProjectile::HandleProjectileStop(const FHitResult& ImpactResult)
{
	if (IsAuthoritativeProjectile())
	{
		HandleAuthoritativeImpact(ImpactResult);
	}
	else if (IsPredictedProjectile())
	{
		Destroy();
	}
}

void AStulWeaponProjectile::HandleAuthoritativeImpact(const FHitResult& ImpactResult)
{
#if ENABLE_DRAW_DEBUG
	if (bDrawDebugTrajectory && GetWorld())
	{
		DrawDebugSphere(GetWorld(), ImpactResult.ImpactPoint, FMath::Max(0.0f, DebugImpactRadius), 12, FColor::Red, false, FMath::Max(0.0f, DebugDrawDuration), 0, FMath::Max(0.0f, DebugLineThickness));
	}
#endif

	AStulWeapon* SourceWeapon = Cast<AStulWeapon>(GetOwner());
	if (SourceWeapon)
	{
		SourceWeapon->HandleAuthoritativeHit(ImpactResult, Damage, this, AmmoDefinition, false);
		if (UStulWeaponFireComponent* FireComponent = SourceWeapon->GetFireComponent()) FireComponent->PresentAuthoritativeProjectileImpact(ImpactResult, Damage, this);
	}

	Destroy();
}
