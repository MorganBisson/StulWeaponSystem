#include "AbilitySystem/Abilities/StulWeaponFireSingleAbility.h"

#include "StulWeaponGameplayTags.h"
#include "Weapons/StulWeapon.h"

UStulWeaponFireSingleAbility::UStulWeaponFireSingleAbility()
{
	ActivationPolicy = EStulWeaponAbilityActivationPolicy::OnInputTriggered;
}

bool UStulWeaponFireSingleAbility::ResolveFireSessionType(const AStulWeapon& Weapon, EStulFireSessionType& OutSessionType) const
{
	OutSessionType = EStulFireSessionType::Single;
	return Weapon.GetCurrentFireMode() == StulWeaponGameplayTags::FireMode_Single;
}
