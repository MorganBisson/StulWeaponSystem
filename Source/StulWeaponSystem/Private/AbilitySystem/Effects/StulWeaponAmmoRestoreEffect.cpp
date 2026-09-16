// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Effects/StulWeaponAmmoRestoreEffect.h"

#include "AbilitySystem/StulWeaponAttributeSet.h"
#include "StulWeaponGameplayTags.h"

UStulWeaponAmmoRestoreEffect::UStulWeaponAmmoRestoreEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat AmmoRestoreMagnitude;
	AmmoRestoreMagnitude.DataTag = StulWeaponGameplayTags::SetByCaller_AmmoRestore;

	FGameplayModifierInfo& AmmoModifier = Modifiers.AddDefaulted_GetRef();
	AmmoModifier.Attribute = UStulWeaponAttributeSet::GetCurrentAmmoAttribute();
	AmmoModifier.ModifierOp = EGameplayModOp::Additive;
	AmmoModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(AmmoRestoreMagnitude);
}
