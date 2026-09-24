#include "AbilitySystem/Abilities/StulWeaponFireBurstAbility.h"

#include "StulWeaponGameplayTags.h"
#include "Weapons/StulWeapon.h"

UStulWeaponFireBurstAbility::UStulWeaponFireBurstAbility()
{
	ActivationPolicy = EStulWeaponAbilityActivationPolicy::OnInputTriggered;
}

bool UStulWeaponFireBurstAbility::ResolveFireSessionType(const AStulWeapon& Weapon, EStulFireSessionType& OutSessionType) const
{
	OutSessionType = EStulFireSessionType::Burst;
	return Weapon.GetCurrentFireMode() == StulWeaponGameplayTags::FireMode_Burst;
}
