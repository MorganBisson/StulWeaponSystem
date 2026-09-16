#include "Weapons/Shooting/StulWeaponShootingLibrary.h"

#include "Engine/Engine.h"
#include "Engine/World.h"

bool UStulWeaponShootingLibrary::FindAimTarget(const UObject* WorldContextObject, const FVector& ViewOrigin, const FVector& ViewDirection, const float MaxRange, const ECollisionChannel TraceChannel, const TArray<AActor*>& ActorsToIgnore, FVector& OutTargetLocation, FHitResult& OutHitResult)
{
	OutHitResult = FHitResult();
	const FVector SafeDirection = ViewDirection.GetSafeNormal();
	OutTargetLocation = ViewOrigin + SafeDirection * FMath::Max(0.0f, MaxRange);

	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World || SafeDirection.IsNearlyZero() || MaxRange <= 0.0f)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(StulWeaponAimTrace), true);
	QueryParams.AddIgnoredActors(ActorsToIgnore);
	const bool bBlockingHit = World->LineTraceSingleByChannel(OutHitResult, ViewOrigin, OutTargetLocation, TraceChannel, QueryParams);
	if (bBlockingHit)
	{
		OutTargetLocation = OutHitResult.ImpactPoint;
	}
	return bBlockingHit;
}

FVector UStulWeaponShootingLibrary::CalculateShotDirection(const FStulWeaponShotRequest& Request)
{
	FVector Direction = (Request.TargetLocation - Request.ShotOrigin).GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return FVector::ForwardVector;
	}

	if (!Request.PatternOffset.IsNearlyZero() && Request.PatternScale > 0.0f)
	{
		FRotator PatternRotation = Direction.Rotation();
		PatternRotation.Pitch += Request.PatternOffset.X * Request.PatternScale;
		PatternRotation.Yaw += Request.PatternOffset.Y * Request.PatternScale;
		Direction = PatternRotation.Vector();
	}

	if (Request.SpreadHalfAngleDegrees > 0.0f)
	{
		FRandomStream RandomStream(Request.RandomSeed);
		Direction = RandomStream.VRandCone(Direction, FMath::DegreesToRadians(Request.SpreadHalfAngleDegrees));
	}

	return Direction.GetSafeNormal();
}

FVector UStulWeaponShootingLibrary::CalculateMuzzleOffset(const FTransform& MuzzleTransform, const FVector2D& PatternOffset, const float OffsetScale)
{
	if (OffsetScale <= 0.0f || PatternOffset.IsNearlyZero())
	{
		return MuzzleTransform.GetLocation();
	}

	return MuzzleTransform.GetLocation()
		+ MuzzleTransform.GetUnitAxis(EAxis::Z) * PatternOffset.X * OffsetScale
		+ MuzzleTransform.GetUnitAxis(EAxis::Y) * PatternOffset.Y * OffsetScale;
}

bool UStulWeaponShootingLibrary::PerformHitscanTrace(const UObject* WorldContextObject, const FVector& TraceOrigin, const FVector& ShotDirection, const float MaxRange, const ECollisionChannel TraceChannel, const TArray<AActor*>& ActorsToIgnore, FHitResult& OutHitResult, FVector& OutEndLocation)
{
	OutHitResult = FHitResult();
	const FVector SafeDirection = ShotDirection.GetSafeNormal();
	OutEndLocation = TraceOrigin + SafeDirection * FMath::Max(0.0f, MaxRange);

	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World || SafeDirection.IsNearlyZero() || MaxRange <= 0.0f)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(StulWeaponHitscanTrace), true);
	QueryParams.AddIgnoredActors(ActorsToIgnore);
	QueryParams.bReturnPhysicalMaterial = true;
	const bool bBlockingHit = World->LineTraceSingleByChannel(OutHitResult, TraceOrigin, OutEndLocation, TraceChannel, QueryParams);
	if (bBlockingHit)
	{
		OutEndLocation = OutHitResult.ImpactPoint;
	}
	return bBlockingHit;
}
