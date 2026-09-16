#include "Projectiles/StulWeaponProjectile.h"

#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Projectiles/ProjectileVisualComponent.h"
#include "StulWeaponGameplayTags.h"
#include "StulWeaponSystem.h"
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

	if (!InitData.IsValid())
	{
		UE_LOG(LogStulWeaponSystem, Error, TEXT("Projectile '%s' began play with invalid initialization data."), *GetNameSafe(this));
		if (HasAuthority())
		{
			Destroy();
		}
		return;
	}

	CollisionComponent->SetCollisionEnabled(HasAuthority() ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	ProjectileMovementComponent->InitialSpeed = InitData.Speed;
	ProjectileMovementComponent->ProjectileGravityScale = FMath::Max(0.0f, InitData.GravityScale);
	ProjectileMovementComponent->Velocity = InitData.Direction.GetSafeNormal() * InitData.Speed;
	SetActorRotation(InitData.Direction.Rotation());
	VisualComponent->InitializeVisual(InitData.CosmeticOrigin);
	ProjectileMovementComponent->OnProjectileStop.AddDynamic(this, &ThisClass::HandleProjectileStop);
	K2_OnProjectileInitialized(InitData);

	if (HasAuthority())
	{
		SetLifeSpan(InitData.MaxLifeSeconds);
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawDebugTrajectory)
	{
		LastDebugLocation = GetActorLocation();
		SetActorTickEnabled(true);
	}
#endif
}

void AStulWeaponProjectile::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

#if ENABLE_DRAW_DEBUG
	if (bDrawDebugTrajectory)
	{
		const FVector CurrentLocation = GetActorLocation();
		const FColor TrajectoryColor = HasAuthority() ? FColor::Orange : FColor::Green;
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
bool AStulWeaponProjectile::InitializeProjectile(const FStulWeaponProjectileInitData& InInitData, const float InDamage)
{
	if (!HasAuthority() || bInitialized || !InInitData.IsValid())
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Projectile '%s' rejected an invalid or repeated initialization request."), *GetNameSafe(this));
		return false;
	}

	InitData = InInitData;
	Damage = FMath::Max(0.0f, InDamage);
	bInitialized = true;

	CollisionComponent->ClearMoveIgnoreActors();
	CollisionComponent->IgnoreActorWhenMoving(this, true);
	if (AActor* SourceWeapon = GetOwner())
	{
		CollisionComponent->IgnoreActorWhenMoving(SourceWeapon, true);
		if (AActor* WeaponOwner = SourceWeapon->GetOwner())
		{
			CollisionComponent->IgnoreActorWhenMoving(WeaponOwner, true);
		}
	}
	return true;
}

/*********************************************************************************************/
/**************************************** Impact *********************************************/
/*********************************************************************************************/
void AStulWeaponProjectile::HandleProjectileStop(const FHitResult& ImpactResult)
{
	if (HasAuthority())
	{
		HandleAuthoritativeImpact(ImpactResult);
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

	AActor* HitActor = ImpactResult.GetActor();
	AStulWeapon* SourceWeapon = Cast<AStulWeapon>(GetOwner());
	if (SourceWeapon)
	{
		SourceWeapon->ExecuteImpactGameplayCue(ImpactResult, Damage, this);
	}
	if (HitActor)
	{
		if (UAbilitySystemComponent* HitAbilitySystem = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor))
		{
			FGameplayEventData HitEventData;
			HitEventData.EventTag = StulWeaponGameplayTags::Event_Hit;
			HitEventData.Instigator = GetInstigator();
			HitEventData.Target = HitActor;
			HitEventData.OptionalObject = SourceWeapon;
			HitEventData.EventMagnitude = Damage;
			HitEventData.TargetData.Add(new FGameplayAbilityTargetData_SingleTargetHit(ImpactResult));
			HitAbilitySystem->HandleGameplayEvent(StulWeaponGameplayTags::Event_Hit, &HitEventData);
		}
	}

	Destroy();
}
