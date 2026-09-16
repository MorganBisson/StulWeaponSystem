// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StulWeaponTypes.generated.h"

class UStulWeaponGameplayAbility;
class UTexture2D;

/** Defines when a weapon ability should be activated. */
UENUM(BlueprintType)
enum class EStulWeaponAbilityActivationPolicy : uint8
{
	/** Attempts activation once for every new input press. */
	OnInputTriggered,

	/** Reattempts activation while the input remains held. */
	WhileInputActive,

	/** Attempts activation when the ability is granted and the weapon ASC is ready. */
	OnSpawn,

	/** Never activates automatically; gameplay code or an event must activate it explicitly. */
	Manual
};

/** Describes how a shot travels from the weapon to its target. */
UENUM(BlueprintType)
enum class EStulWeaponShotType : uint8
{
	Hitscan,
	Projectile
};

/** Describes whether a reload fills the magazine or inserts a fixed amount of ammunition. */
UENUM(BlueprintType)
enum class EStulWeaponReloadType : uint8
{
	/** Restores every missing round after one reload duration. */
	Full,

	/** Restores BaseAmmoToReload rounds per reload duration until full or interrupted. */
	Custom
};

/** Project-independent aiming configuration resolved from the weapon and future customizations. */
USTRUCT(BlueprintType)
struct STULWEAPONSYSTEM_API FStulWeaponAimData
{
	GENERATED_BODY()

	/** Optical magnification requested from the owning project's camera system. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aim", meta = (ClampMin = "1.0"))
	float Magnification = 1.0f;

	/** Socket used by the owning project's alignment and IK systems. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aim")
	FName AimSocketName = TEXT("AimSocket");
};

/** Presentation data that is useful to any game, independently of its UI framework. */
USTRUCT(BlueprintType)
struct STULWEAPONSYSTEM_API FStulWeaponDisplayData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	FText Name;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	TSoftObjectPtr<UTexture2D> Icon;
};

/** A deterministic offset pattern used by weapons that emit several shots at once. */
USTRUCT(BlueprintType)
struct STULWEAPONSYSTEM_API FStulWeaponShotPattern
{
	GENERATED_BODY()

	/** X is the pitch offset and Y is the yaw offset, both in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern")
	TArray<FVector2D> ShotPoints;
};

/** Grants a weapon ability and optionally associates a semantic input tag with it. */
USTRUCT(BlueprintType)
struct STULWEAPONSYSTEM_API FStulWeaponAbilityMapping
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	TSubclassOf<UStulWeaponGameplayAbility> AbilityClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability", meta = (Categories = "Stul.Weapon.Input"))
	FGameplayTag InputTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability", meta = (ClampMin = "1"))
	int32 AbilityLevel = 1;
};

/** Minimal state required to restore a weapon without serializing its actor. */
USTRUCT(BlueprintType)
struct STULWEAPONSYSTEM_API FStulWeaponInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Weapon", meta = (AllowedTypes = "StulWeaponDefinition"))
	FPrimaryAssetId WeaponDefinitionId;

	/** Item level captured when the weapon is generated; reserved for future weapon and mod scaling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Weapon", meta = (ClampMin = "1"))
	int32 ItemLevel = 1;

	/** A negative value means that a newly created weapon should start with a full magazine. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Weapon")
	int32 CurrentAmmo = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Weapon", meta = (Categories = "Stul.Weapon.FireMode"))
	FGameplayTag CurrentFireModeTag;

	/** Stable generation seed used to reconstruct procedural weapon and mod rolls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Weapon")
	int32 GenerationSeed = 0;

	bool IsValid() const
	{
		return WeaponDefinitionId.IsValid() && ItemLevel > 0;
	}
};

/** View information supplied by the owning project, whether it uses FPS, TPS or AI aiming. */
USTRUCT(BlueprintType)
struct STULWEAPONSYSTEM_API FStulWeaponViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "View")
	FVector ViewLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "View")
	FRotator ViewRotation = FRotator::ZeroRotator;
};

/** Movement information used by generic weapon spread calculations. */
USTRUCT(BlueprintType)
struct STULWEAPONSYSTEM_API FStulWeaponMovementData
{
	GENERATED_BODY()

	/** Normalized movement amount: zero at rest and one at the reference maximum speed. */
	UPROPERTY(BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MovementAlpha = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Movement")
	bool bIsInAir = false;

	UPROPERTY(BlueprintReadWrite, Category = "Movement")
	bool bIsCrouching = false;
};
