// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapons/StulWeaponDefinition.h"

#include "AbilitySystem/StulWeaponGameplayAbility.h"
#include "StulWeaponGameplayTags.h"
#include "Weapons/Ammunition/StulAmmoDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace StulWeaponPrimaryAssetTypes
{
	const FPrimaryAssetType StulWeaponDefinition(TEXT("StulWeaponDefinition"));
}

FPrimaryAssetId UStulWeaponDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(StulWeaponPrimaryAssetTypes::StulWeaponDefinition, GetFName());
}

void UStulWeaponDefinition::GetGameplayAssetPaths(TArray<FSoftObjectPath>& OutPaths) const
{
	OutPaths.Reset();

	if (ShotType == EStulWeaponShotType::Projectile && !ProjectileClass.IsNull())
	{
		OutPaths.AddUnique(ProjectileClass.ToSoftObjectPath());
	}
}

void UStulWeaponDefinition::GetPresentationAssetPaths(TArray<FSoftObjectPath>& OutPaths) const
{
	OutPaths.Reset();

	if (!WeaponMesh.IsNull())
	{
		OutPaths.AddUnique(WeaponMesh.ToSoftObjectPath());
	}
	if (!Presentation.MuzzleFlash.IsNull())
	{
		OutPaths.AddUnique(Presentation.MuzzleFlash.ToSoftObjectPath());
	}
	if (!Presentation.FireSound.IsNull())
	{
		OutPaths.AddUnique(Presentation.FireSound.ToSoftObjectPath());
	}
	if (ShotType == EStulWeaponShotType::Hitscan && !HitscanTracer.System.IsNull())
	{
		OutPaths.AddUnique(HitscanTracer.System.ToSoftObjectPath());
	}
	if (DefaultAmmo)
	{
		TArray<FSoftObjectPath> AmmoPresentationAssetPaths;
		DefaultAmmo->GetPresentationAssetPaths(AmmoPresentationAssetPaths);
		for (const FSoftObjectPath& AssetPath : AmmoPresentationAssetPaths)
		{
			OutPaths.AddUnique(AssetPath);
		}
	}
}

void UStulWeaponDefinition::GetDisplayAssetPaths(TArray<FSoftObjectPath>& OutPaths) const
{
	OutPaths.Reset();

	if (!Display.Icon.IsNull())
	{
		OutPaths.AddUnique(Display.Icon.ToSoftObjectPath());
	}
}

#if WITH_EDITOR
EDataValidationResult UStulWeaponDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	auto AddError = [&Context, &Result](const FText& Error)
	{
		Context.AddError(Error);
		Result = EDataValidationResult::Invalid;
	};

	if (MuzzleSocketName.IsNone())
	{
		AddError(NSLOCTEXT("StulWeaponValidation", "MissingMuzzleSocket", "MuzzleSocketName must not be None."));
	}
	if (AimData.AimSocketName.IsNone())
	{
		AddError(NSLOCTEXT("StulWeaponValidation", "MissingAimSocket", "AimData.AimSocketName must not be None."));
	}
	if (!DefaultAmmo)
	{
		AddError(NSLOCTEXT("StulWeaponValidation", "MissingDefaultAmmo", "A DefaultAmmo definition must be configured."));
	}
	else if (!DefaultAmmo->ImpactProfile)
	{
		AddError(NSLOCTEXT("StulWeaponValidation", "MissingImpactProfile", "The DefaultAmmo definition must reference an ImpactProfile."));
	}
	if (AimData.Magnification < 1.0f)
	{
		AddError(NSLOCTEXT("StulWeaponValidation", "InvalidAimMagnification", "AimData.Magnification must be at least one."));
	}
	if (BaseFireInterval <= 0.0f)
	{
		AddError(NSLOCTEXT("StulWeaponValidation", "InvalidFireInterval", "BaseFireInterval must be greater than zero."));
	}
	if (BaseBurstInterval < BaseFireInterval)
	{
		AddError(NSLOCTEXT("StulWeaponValidation", "InvalidBurstInterval", "BaseBurstInterval must be greater than or equal to BaseFireInterval."));
	}
	if (ShotType == EStulWeaponShotType::Hitscan && MaxRange <= 0.0f)
	{
		AddError(NSLOCTEXT("StulWeaponValidation", "InvalidHitscanRange", "Hitscan weapons must have a MaxRange greater than zero."));
	}
	if (ShotType == EStulWeaponShotType::Hitscan && !HitscanTracer.System.IsNull() && HitscanTracer.Speed <= 0.0f)
	{
		AddError(NSLOCTEXT("StulWeaponValidation", "InvalidTracerSpeed", "Configured hitscan tracers must have a Speed greater than zero."));
	}
	if (ShotType == EStulWeaponShotType::Hitscan && !HitscanTracer.System.IsNull() && HitscanTracer.Length <= 0.0f)
	{
		AddError(NSLOCTEXT("StulWeaponValidation", "InvalidTracerLength", "Configured hitscan tracers must have a Length greater than zero."));
	}
	if (ShotType == EStulWeaponShotType::Hitscan && !HitscanTracer.System.IsNull() && HitscanTracer.Width <= 0.0f)
	{
		AddError(NSLOCTEXT("StulWeaponValidation", "InvalidTracerWidth", "Configured hitscan tracers must have a Width greater than zero."));
	}
	for (const TPair<int32, FStulWeaponShotPattern>& PatternPair : ShotPatterns)
	{
		if (PatternPair.Key <= 0)
		{
			AddError(NSLOCTEXT("StulWeaponValidation", "InvalidShotPatternCount", "ShotPatterns keys must be greater than zero."));
			continue;
		}
		if (PatternPair.Value.ShotPoints.Num() != PatternPair.Key) AddError(FText::Format(NSLOCTEXT("StulWeaponValidation", "MismatchedShotPatternSize", "Shot pattern {0} must contain exactly {0} points."), FText::AsNumber(PatternPair.Key)));
	}

	if (AvailableFireModes.IsEmpty())
	{
		AddError(NSLOCTEXT("StulWeaponValidation", "MissingFireModes", "At least one fire mode must be configured."));
	}
	else if (!DefaultFireModeTag.IsValid() || !AvailableFireModes.HasTagExact(DefaultFireModeTag))
	{
		AddError(NSLOCTEXT("StulWeaponValidation", "InvalidDefaultFireMode", "DefaultFireModeTag must be one of AvailableFireModes."));
	}

	for (const FGameplayTag FireMode : AvailableFireModes)
	{
		if (!StulWeaponGameplayTags::IsSupportedFireMode(FireMode))
		{
			AddError(NSLOCTEXT("StulWeaponValidation", "UnsupportedFireMode", "AvailableFireModes may currently contain only Single, Burst or Automatic."));
			break;
		}
	}

	if (BaseMagazineCapacity < 0)
	{
		AddError(NSLOCTEXT("StulWeaponValidation", "InvalidMagazineCapacity", "BaseMagazineCapacity cannot be negative."));
	}
	if (BaseReloadDuration <= 0.0f)
	{
		AddError(NSLOCTEXT("StulWeaponValidation", "InvalidReloadDuration", "BaseReloadDuration must be greater than zero."));
	}
	if (ReloadType == EStulWeaponReloadType::Custom && BaseAmmoToReload <= 0)
	{
		AddError(NSLOCTEXT("StulWeaponValidation", "InvalidIncrementalReloadAmount", "Custom reloads require BaseAmmoToReload to be greater than zero."));
	}
	if (BaseAimDuration < 0.0f)
	{
		AddError(NSLOCTEXT("StulWeaponValidation", "InvalidAimDuration", "BaseAimDuration cannot be negative."));
	}
	if (BaseFireModeChangeDuration < 0.0f)
	{
		AddError(NSLOCTEXT("StulWeaponValidation", "InvalidFireModeChangeDuration", "BaseFireModeChangeDuration cannot be negative."));
	}

	if (ShotType == EStulWeaponShotType::Projectile && ProjectileClass.IsNull())
	{
		AddError(NSLOCTEXT("StulWeaponValidation", "MissingProjectileClass", "Projectile weapons require a ProjectileClass."));
	}
	if (ShotType == EStulWeaponShotType::Projectile && BaseProjectileSpeed <= 0.0f)
	{
		AddError(NSLOCTEXT("StulWeaponValidation", "InvalidProjectileSpeed", "Projectile weapons require BaseProjectileSpeed to be greater than zero."));
	}
	if (ShotType == EStulWeaponShotType::Projectile && AimTraceDistance <= 0.0f)
	{
		AddError(NSLOCTEXT("StulWeaponValidation", "InvalidProjectileAimDistance", "Projectile weapons require AimTraceDistance to be greater than zero."));
	}
	if (ShotType == EStulWeaponShotType::Projectile && ProjectileMaxLifeSeconds <= 0.0f)
	{
		AddError(NSLOCTEXT("StulWeaponValidation", "InvalidProjectileLifetime", "Projectile weapons require ProjectileMaxLifeSeconds to be greater than zero."));
	}

	TSet<UClass*> MappedAbilityClasses;
	for (const FStulWeaponAbilityMapping& Mapping : BaseAbilities)
	{
		if (!Mapping.AbilityClass)
		{
			AddError(NSLOCTEXT("StulWeaponValidation", "InvalidAbilityMapping", "Every ability mapping requires an AbilityClass."));
			break;
		}
		if (Mapping.AbilityLevel <= 0)
		{
			AddError(NSLOCTEXT("StulWeaponValidation", "InvalidAbilityLevel", "Every ability mapping requires an AbilityLevel greater than zero."));
			break;
		}
		if (MappedAbilityClasses.Contains(Mapping.AbilityClass.Get()))
		{
			AddError(NSLOCTEXT("StulWeaponValidation", "DuplicateAbilityClass", "The same weapon ability class cannot be granted more than once."));
			break;
		}
		MappedAbilityClasses.Add(Mapping.AbilityClass.Get());

		const UStulWeaponGameplayAbility* WeaponAbilityCDO = Mapping.AbilityClass.GetDefaultObject();
		if (!WeaponAbilityCDO
			|| WeaponAbilityCDO->GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::InstancedPerActor)
		{
			AddError(NSLOCTEXT(
				"StulWeaponValidation",
				"InvalidAbilityInstancingPolicy",
				"Weapon abilities must use the Instanced Per Actor Gameplay Ability policy."));
			break;
		}

		const EStulWeaponAbilityActivationPolicy ActivationPolicy = WeaponAbilityCDO->GetActivationPolicy();
		const bool bUsesInput = ActivationPolicy == EStulWeaponAbilityActivationPolicy::OnInputTriggered
			|| ActivationPolicy == EStulWeaponAbilityActivationPolicy::WhileInputActive;
		if (bUsesInput && !Mapping.InputTag.IsValid())
		{
			AddError(NSLOCTEXT("StulWeaponValidation", "MissingAbilityInputTag", "Input-triggered weapon abilities require an InputTag."));
			break;
		}
		if (!bUsesInput && Mapping.InputTag.IsValid())
		{
			AddError(NSLOCTEXT("StulWeaponValidation", "UnusedAbilityInputTag", "Manual and OnSpawn weapon abilities must not declare an InputTag."));
			break;
		}

		if (Mapping.InputTag.IsValid()
			&& (Mapping.InputTag == StulWeaponGameplayTags::InputTag_Root
				|| !Mapping.InputTag.MatchesTag(StulWeaponGameplayTags::InputTag_Root)))
		{
			AddError(NSLOCTEXT("StulWeaponValidation", "InvalidAbilityInputTag", "Weapon ability input tags must be children of Stul.Weapon.Input."));
			break;
		}
	}

	return Result;
}
#endif
