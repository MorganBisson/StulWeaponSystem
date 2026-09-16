#include "Projectiles/ProjectileVisualComponent.h"

#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"

UProjectileVisualComponent::UProjectileVisualComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

/*********************************************************************************************/
/************************************ Initialization *****************************************/
/*********************************************************************************************/
void UProjectileVisualComponent::InitializeVisual(const FVector& CosmeticOrigin)
{
	SetComponentTickEnabled(false);
	SetRelativeLocation(FVector::ZeroVector);
	bConvergenceActive = false;

	const AActor* ProjectileOwner = GetOwner();
	if (!ProjectileOwner || ProjectileOwner->GetNetMode() == NM_DedicatedServer || ConvergenceDistance <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	ConvergenceStartLocation = ProjectileOwner->GetActorLocation();
	InitialWorldOffset = CosmeticOrigin - ConvergenceStartLocation;
	if (InitialWorldOffset.IsNearlyZero())
	{
		return;
	}

	SetWorldLocation(CosmeticOrigin, false, nullptr, ETeleportType::TeleportPhysics);
	LastDebugVisualLocation = CosmeticOrigin;
	bConvergenceActive = true;
	SetComponentTickEnabled(true);
}

/*********************************************************************************************/
/************************************** Component ********************************************/
/*********************************************************************************************/
void UProjectileVisualComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const AActor* ProjectileOwner = GetOwner();
	if (!bConvergenceActive || !ProjectileOwner)
	{
		SetComponentTickEnabled(false);
		return;
	}

	const FVector GameplayLocation = ProjectileOwner->GetActorLocation();
	const float DistanceAlpha = FMath::Clamp(FVector::Distance(ConvergenceStartLocation, GameplayLocation) / ConvergenceDistance, 0.0f, 1.0f);
	const float SmoothedAlpha = FMath::SmoothStep(0.0f, 1.0f, DistanceAlpha);
	SetWorldLocation(GameplayLocation + InitialWorldOffset * (1.0f - SmoothedAlpha), false, nullptr, ETeleportType::TeleportPhysics);

#if ENABLE_DRAW_DEBUG
	if (bDrawDebugConvergence && GetWorld())
	{
		const FVector VisualLocation = GetComponentLocation();
		DrawDebugLine(GetWorld(), LastDebugVisualLocation, VisualLocation, FColor::Purple, false, FMath::Max(0.0f, DebugDrawDuration), 0, FMath::Max(0.0f, DebugLineThickness));
		DrawDebugLine(GetWorld(), VisualLocation, GameplayLocation, FColor::Cyan, false, FMath::Max(0.0f, DebugDrawDuration), 0, FMath::Max(0.0f, DebugLineThickness));
		LastDebugVisualLocation = VisualLocation;
	}
#endif

	if (DistanceAlpha >= 1.0f)
	{
		SetRelativeLocation(FVector::ZeroVector);
		bConvergenceActive = false;
		SetComponentTickEnabled(false);
	}
}
