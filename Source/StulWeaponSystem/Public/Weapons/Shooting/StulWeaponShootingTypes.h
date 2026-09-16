#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "StulWeaponShootingTypes.generated.h"

/** Compact client aim data sent to the server for one weapon shot. */
USTRUCT()
struct STULWEAPONSYSTEM_API FGameplayAbilityTargetData_StulWeaponShot : public FGameplayAbilityTargetData
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantize10 ViewOrigin = FVector::ZeroVector;

	UPROPERTY()
	FVector_NetQuantizeNormal ViewDirection = FVector::ForwardVector;

	UPROPERTY()
	uint16 ShotSequence = 0;

	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
	virtual bool HasOrigin() const override { return true; }
	virtual FTransform GetOrigin() const override { return FTransform(ViewDirection.Rotation(), ViewOrigin); }
	virtual bool HasEndPoint() const override { return true; }
	virtual FVector GetEndPoint() const override { return ViewOrigin + ViewDirection; }
	virtual FString ToString() const override { return TEXT("FGameplayAbilityTargetData_StulWeaponShot"); }
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FGameplayAbilityTargetData_StulWeaponShot> : public TStructOpsTypeTraitsBase2<FGameplayAbilityTargetData_StulWeaponShot>
{
	enum { WithNetSerializer = true };
};

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
