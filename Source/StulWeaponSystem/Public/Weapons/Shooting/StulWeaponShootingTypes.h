#pragma once

#include "CoreMinimal.h"
#include "Weapons/Shooting/StulWeaponFireSessionTypes.h"
#include "StulWeaponShootingTypes.generated.h"

/** Immutable input used to calculate one projectile or hitscan direction. */
USTRUCT(BlueprintType)
struct STULWEAPONSYSTEM_API FStulWeaponShotRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Shot")
	FVector ShotOrigin = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Shot")
	FVector TargetLocation = FVector::ZeroVector;

	/** X is pitch and Y is yaw, both in degrees. */
	UPROPERTY(BlueprintReadWrite, Category = "Shot")
	FVector2D PatternOffset = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Shot", meta = (ClampMin = "0.0"))
	float PatternScale = 1.0f;

	/** Angular radius of the random cone in degrees. */
	UPROPERTY(BlueprintReadWrite, Category = "Shot", meta = (ClampMin = "0.0", Units = "deg"))
	float SpreadHalfAngleDegrees = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Shot")
	int32 RandomSeed = 0;
};

/** Result shared by authoritative gameplay and local weapon presentation. */
USTRUCT(BlueprintType)
struct STULWEAPONSYSTEM_API FStulWeaponShotResult
{
	GENERATED_BODY()

	/** Authoritative trace origin, normally the validated player viewpoint. */
	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	FVector_NetQuantize10 TraceOrigin = FVector::ZeroVector;

	/** Cosmetic origin used by muzzle flashes and tracers. */
	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	FVector_NetQuantize10 MuzzleLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	FVector_NetQuantizeNormal Direction = FVector::ForwardVector;

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	FVector_NetQuantize10 EndLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	FHitResult HitResult;

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	int32 ProjectileIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	bool bBlockingHit = false;

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	bool bAuthoritative = false;
};

/** Results produced by one logical weapon shot and its shared sequence number. */
USTRUCT(BlueprintType)
struct STULWEAPONSYSTEM_API FStulWeaponShotExecution
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	FStulShotId ShotId;

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	int32 ShotSequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	TArray<FStulWeaponShotResult> Results;

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	bool bAuthoritative = false;
};
