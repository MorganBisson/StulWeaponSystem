#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Weapons/Shooting/StulWeaponShootingTypes.h"
#include "StulWeaponShootingLibrary.generated.h"

/** Stateless helpers shared by C++ and Blueprint weapon firing implementations. */
UCLASS()
class STULWEAPONSYSTEM_API UStulWeaponShootingLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Traces from the view to select the point that the weapon should aim toward. */
	UFUNCTION(BlueprintCallable, Category = "Stul Weapon System|Shooting", meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "ActorsToIgnore"))
	static bool FindAimTarget(const UObject* WorldContextObject, const FVector& ViewOrigin, const FVector& ViewDirection, float MaxRange, ECollisionChannel TraceChannel, const TArray<AActor*>& ActorsToIgnore, FVector& OutTargetLocation, FHitResult& OutHitResult);

	/** Produces a repeatable direction from the same request and seed on client and server. */
	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Shooting")
	static FVector CalculateShotDirection(const FStulWeaponShotRequest& Request);

	/** Offsets a muzzle in its local up/right plane using a pattern expressed in authored units. */
	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Shooting")
	static FVector CalculateMuzzleOffset(const FTransform& MuzzleTransform, const FVector2D& PatternOffset, float OffsetScale);

	/** Performs one authoritative hitscan trace. */
	UFUNCTION(BlueprintCallable, Category = "Stul Weapon System|Shooting", meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "ActorsToIgnore"))
	static bool PerformHitscanTrace(const UObject* WorldContextObject, const FVector& TraceOrigin, const FVector& ShotDirection, float MaxRange, ECollisionChannel TraceChannel, const TArray<AActor*>& ActorsToIgnore, FHitResult& OutHitResult, FVector& OutEndLocation);
};
