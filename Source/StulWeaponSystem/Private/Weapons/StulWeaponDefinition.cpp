// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapons/StulWeaponDefinition.h"

#include "AbilitySystem/StulWeaponGameplayAbility.h"
#include "StulWeaponGameplayTags.h"

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
	if (!Presentation.HitscanTracer.IsNull())
	{
		OutPaths.AddUnique(Presentation.HitscanTracer.ToSoftObjectPath());
	}
	if (!Presentation.DefaultImpactEffect.IsNull())
	{
		OutPaths.AddUnique(Presentation.DefaultImpactEffect.ToSoftObjectPath());
	}
	if (!Presentation.DefaultImpactSound.IsNull())
	{
		OutPaths.AddUnique(Presentation.DefaultImpactSound.ToSoftObjectPath());
	}
	for (const TPair<TEnumAsByte<EPhysicalSurface>, TSoftObjectPtr<UNiagaraSystem>>& Pair : Presentation.ImpactEffectsBySurface)
	{
		if (!Pair.Value.IsNull())
		{
			OutPaths.AddUnique(Pair.Value.ToSoftObjectPath());
		}
	}
	for (const TPair<TEnumAsByte<EPhysicalSurface>, TSoftObjectPtr<USoundBase>>& Pair : Presentation.ImpactSoundsBySurface)
	{
		if (!Pair.Value.IsNull())
		{
			OutPaths.AddUnique(Pair.Value.ToSoftObjectPath());
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
	for (const TPair<int32, FStulWeaponShotPattern>& PatternPair : ShotPatterns)
	{
		if (PatternPair.Key <= 0)
		{
			AddError(NSLOCTEXT("StulWeaponValidation", "InvalidShotPatternCount", "ShotPatterns keys must be greater than zero."));
			break;
		}
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
		if (!FireMode.IsValid()
			|| FireMode == StulWeaponGameplayTags::FireMode_Root
			|| !FireMode.MatchesTag(StulWeaponGameplayTags::FireMode_Root))
		{
			AddError(NSLOCTEXT("StulWeaponValidation", "InvalidFireMode", "Fire modes must be children of Stul.Weapon.FireMode."));
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

	for (const FStulWeaponAbilityMapping& Mapping : BaseAbilities)
	{
		if (!Mapping.AbilityClass)
		{
			AddError(NSLOCTEXT("StulWeaponValidation", "InvalidAbilityMapping", "Every ability mapping requires an AbilityClass."));
			break;
		}

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
