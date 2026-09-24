#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/StulWeaponFireAbility.h"
#include "StulWeaponFireSingleAbility.generated.h"

/** One input activation produces exactly one logical shot. */
UCLASS(Blueprintable)
class STULWEAPONSYSTEM_API UStulWeaponFireSingleAbility : public UStulWeaponFireAbility
{
	GENERATED_BODY()

public:
	UStulWeaponFireSingleAbility();

protected:
	virtual bool ResolveFireSessionType(const AStulWeapon& Weapon, EStulFireSessionType& OutSessionType) const override;
};
