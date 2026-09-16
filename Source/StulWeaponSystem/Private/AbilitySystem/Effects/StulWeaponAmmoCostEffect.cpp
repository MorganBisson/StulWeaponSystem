#include "AbilitySystem/Effects/StulWeaponAmmoCostEffect.h"

#include "StulWeaponGameplayTags.h"
#include "AbilitySystem/StulWeaponAttributeSet.h"

UStulWeaponAmmoCostEffect::UStulWeaponAmmoCostEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat AmmoCostMagnitude;
	AmmoCostMagnitude.DataTag = StulWeaponGameplayTags::SetByCaller_Ammo;

	FGameplayModifierInfo& AmmoModifier = Modifiers.AddDefaulted_GetRef();
	AmmoModifier.Attribute = UStulWeaponAttributeSet::GetCurrentAmmoAttribute();
	AmmoModifier.ModifierOp = EGameplayModOp::Additive;
	AmmoModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(AmmoCostMagnitude);
}
