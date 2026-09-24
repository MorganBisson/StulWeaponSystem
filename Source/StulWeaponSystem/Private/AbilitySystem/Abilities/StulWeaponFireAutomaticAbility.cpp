#include "AbilitySystem/Abilities/StulWeaponFireAutomaticAbility.h"

#include "StulWeaponGameplayTags.h"
#include "Weapons/StulWeapon.h"

UStulWeaponFireAutomaticAbility::UStulWeaponFireAutomaticAbility()
{
	ActivationPolicy = EStulWeaponAbilityActivationPolicy::WhileInputActive;
}

bool UStulWeaponFireAutomaticAbility::ResolveFireSessionType(const AStulWeapon& Weapon, EStulFireSessionType& OutSessionType) const
{
	OutSessionType = EStulFireSessionType::Automatic;
	return Weapon.GetCurrentFireMode() == StulWeaponGameplayTags::FireMode_Automatic;
}
