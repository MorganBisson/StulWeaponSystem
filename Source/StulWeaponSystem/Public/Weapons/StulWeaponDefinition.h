// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Weapons/Presentation/StulWeaponPresentationTypes.h"
#include "Weapons/StulWeaponTypes.h"
#include "StulWeaponDefinition.generated.h"

class UAnimSequence;
class UCurveTable;
class UCurveVector;
class USkeletalMesh;
class AStulWeaponProjectile;
class FDataValidationContext;
class UStulAmmoDefinition;

/** Immutable authoring data shared by every runtime instance of a weapon. */
UCLASS(BlueprintType, Const)
class STULWEAPONSYSTEM_API UStulWeaponDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	/** Collects gameplay assets required on every network role, including dedicated servers. */
	void GetGameplayAssetPaths(TArray<FSoftObjectPath>& OutPaths) const;

	/** Collects presentation assets that dedicated servers never need to load. */
	void GetPresentationAssetPaths(TArray<FSoftObjectPath>& OutPaths) const;

	/** Collects assets that are only required by menus or inventory UI. */
	void GetDisplayAssetPaths(TArray<FSoftObjectPath>& OutPaths) const;

	/* ------------------------------ Abilities ------------------------------ */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Initialization|Abilities")
	TArray<FStulWeaponAbilityMapping> BaseAbilities;

	/* ------------------------------- Display ------------------------------- */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display", meta = (ShowOnlyInnerProperties))
	FStulWeaponDisplayData Display;

	/* ---------------------------- Shooting setup --------------------------- */

	/** Describes how shots from this weapon travel to their targets. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|General")
	EStulWeaponShotType ShotType = EStulWeaponShotType::Hitscan;

	/** Channel used to resolve the point under the crosshair before applying pattern and spread. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|General")
	TEnumAsByte<ECollisionChannel> AimTraceChannel = ECC_Visibility;

	/** Ammunition payload used by default, independently of the weapon's hitscan or projectile delivery method. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Ammunition")
	TObjectPtr<UStulAmmoDefinition> DefaultAmmo;

	/* -------------------------------- Visual ------------------------------- */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	TSoftObjectPtr<USkeletalMesh> WeaponMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	FName MuzzleSocketName = TEXT("Muzzle");

	/** Optional audiovisual effects consumed by the generic weapon Gameplay Cues. Dedicated servers never load these assets. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual|Effects", meta = (ShowOnlyInnerProperties, DisplayName = "Effects"))
	FStulWeaponPresentationData Presentation;

	/** Optional client-side tracer used only by hitscan weapons. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual|Effects|Hitscan Tracer", meta = (ShowOnlyInnerProperties, EditCondition = "ShotType == EStulWeaponShotType::Hitscan", EditConditionHides))
	FStulWeaponTracerData HitscanTracer;

	/** Base aiming configuration. Future sights may override the resolved value at runtime. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Aim", meta = (ShowOnlyInnerProperties))
	FStulWeaponAimData AimData;

	/* ----------------------------- Fire modes ------------------------------ */

	/** Set of fire modes supported by this weapon. Cycle order is defined by the runtime weapon. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Fire Modes", meta = (Categories = "Stul.Weapon.FireMode"))
	FGameplayTagContainer AvailableFireModes;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Fire Modes", meta = (Categories = "Stul.Weapon.FireMode"))
	FGameplayTag DefaultFireModeTag;

	/** Delay before a requested fire mode becomes active. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Fire Modes", meta = (ClampMin = "0.0", Units = "s"))
	float BaseFireModeChangeDuration = 0.25f;

	/* ----------------------------- Base stats ------------------------------ */

	/** Reserved for the later modifier/scaling roadmap; it is not consumed by runtime gameplay yet. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Deferred|Scaling")
	TSoftObjectPtr<UCurveTable> MultipliersCurveTable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|General", meta = (ClampMin = "0.0"))
	float BaseDamage = 18.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|General", meta = (ClampMin = "1"))
	int32 BaseShotsPerFire = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|General", meta = (ClampMin = "0"))
	int32 BaseFireCost = 1;

	/** Time in seconds between two consecutive shots. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Timing", meta = (ClampMin = "0.001", Units = "s"))
	float BaseFireInterval = 0.1f;

	/** Delay from the final shot of one burst to the first shot of the next. Runtime never allows it below FireInterval. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float BaseBurstInterval = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|General", meta = (ClampMin = "1"))
	int32 BaseBurstShotCount = 3;

	/* -------------------------------- Spread ------------------------------- */

	/** Base cone currently applied to every shot. Dynamic spread is intentionally deferred. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Spread", meta = (ClampMin = "0.0", Units = "deg"))
	float BaseMinSpread = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Deferred|Dynamic Spread", meta = (ClampMin = "0.0", Units = "deg"))
	float BaseAirMinSpread = 7.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Deferred|Dynamic Spread", meta = (ClampMin = "0.0", Units = "deg"))
	float BaseMaxSpread = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Deferred|Dynamic Spread", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BaseCrouchSpreadMultiplier = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Deferred|Dynamic Spread", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BaseStandSpreadMultiplier = 0.45f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Deferred|Dynamic Spread", meta = (ClampMin = "1.0"))
	float BaseAirSpreadMultiplier = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Deferred|Dynamic Spread", meta = (ClampMin = "0.0", Units = "deg"))
	float BaseSpreadIncreasePerShot = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Deferred|Dynamic Spread", meta = (ClampMin = "0.0", Units = "deg"))
	float BaseMaxSpreadOvershoot = 4.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Deferred|Dynamic Spread", meta = (ClampMin = "0.0"))
	float BaseSpreadInterpSpeed = 5.0f;

	/* ------------------------------- Reload -------------------------------- */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Reload")
	EStulWeaponReloadType ReloadType = EStulWeaponReloadType::Full;

	/** Number of rounds restored after each duration in the incremental Custom reload mode. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Reload", meta = (ClampMin = "1", EditCondition = "ReloadType == EStulWeaponReloadType::Custom", EditConditionHides))
	int32 BaseAmmoToReload = 1;

	/** Full reload duration, or duration between two inserts in Custom mode, before modifiers are applied. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Reload", meta = (ClampMin = "0.001", Units = "s"))
	float BaseReloadDuration = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Magazine", meta = (ClampMin = "0"))
	int32 BaseMagazineCapacity = 30;

	/** Aim transition duration in seconds before modifiers are applied. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Aim", meta = (ClampMin = "0.0", Units = "s"))
	float BaseAimDuration = 0.2f;

	/* ------------------------------- Patterns ------------------------------ */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Patterns", meta = (ClampMin = "0.0"))
	float BasePatternScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Patterns")
	TMap<int32, FStulWeaponShotPattern> ShotPatterns;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Patterns")
	bool bUseMuzzleOffset = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Patterns", meta = (ClampMin = "0.0", EditCondition = "bUseMuzzleOffset"))
	float MuzzleOffsetScale = 1.0f;

	/* ----------------------------- Penetration ----------------------------- */

	/** Penetration authoring is retained for the roadmap and is not executed by the current shot pipeline. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Deferred|Penetration", meta = (ClampMin = "0"))
	int32 BasePenetrationCount = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Deferred|Penetration", meta = (ClampMin = "0.0"))
	float BasePenetrationStrength = 0.0f;

	/* ------------------------------ Ballistics ----------------------------- */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Hitscan", meta = (ClampMin = "0.0", Units = "cm", EditCondition = "ShotType == EStulWeaponShotType::Hitscan", EditConditionHides))
	float MaxRange = 5000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Hitscan", meta = (EditCondition = "ShotType == EStulWeaponShotType::Hitscan", EditConditionHides))
	TEnumAsByte<ECollisionChannel> HitscanTraceChannel = ECC_Visibility;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Projectile", meta = (EditCondition = "ShotType == EStulWeaponShotType::Projectile", EditConditionHides))
	TSoftClassPtr<AStulWeaponProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Projectile", meta = (ClampMin = "0.0", Units = "cm/s", EditCondition = "ShotType == EStulWeaponShotType::Projectile", EditConditionHides))
	float BaseProjectileSpeed = 20000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Projectile", meta = (ClampMin = "0.0", EditCondition = "ShotType == EStulWeaponShotType::Projectile", EditConditionHides))
	float ProjectileGravityScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Projectile", meta = (ClampMin = "0.0", Units = "cm", EditCondition = "ShotType == EStulWeaponShotType::Projectile", EditConditionHides))
	float AimTraceDistance = 10000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Projectile", meta = (ClampMin = "0.001", Units = "s", EditCondition = "ShotType == EStulWeaponShotType::Projectile", EditConditionHides))
	float ProjectileMaxLifeSeconds = 5.0f;

	/* ------------------------------- Recoil -------------------------------- */

	/** Recoil authoring is retained for the roadmap and is not consumed by runtime presentation yet. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Deferred|Recoil")
	TSoftObjectPtr<UCurveVector> RecoilCurve;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Deferred|Recoil", meta = (ClampMin = "0.0"))
	float RecoilRecoverySpeed = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Deferred|Recoil", meta = (ClampMin = "0.0"))
	float RecoilSpeed = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Deferred|Recoil", meta = (ClampMin = "0.0"))
	float RecoilKickMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Deferred|Recoil", meta = (ClampMin = "0.0", Units = "s"))
	float TimeBeforeRecoveryStarts = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Deferred|Recoil")
	float RecoilKick = 10.0f;

	/* ------------------------------ Animation ------------------------------ */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual|Animation")
	TSoftObjectPtr<UAnimSequence> FireAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual|Attachment")
	FTransform GripAttachTransform;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual|Attachment")
	FTransform HolsterAttachTransform;
};
