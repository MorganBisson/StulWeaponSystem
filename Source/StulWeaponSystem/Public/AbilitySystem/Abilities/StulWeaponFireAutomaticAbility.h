#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/StulWeaponFireAbility.h"
#include "StulWeaponFireAutomaticAbility.generated.h"

/** Produces shots while the fire input remains active. */
UCLASS(Blueprintable)
class STULWEAPONSYSTEM_API UStulWeaponFireAutomaticAbility : public UStulWeaponFireAbility
{
	GENERATED_BODY()

public:
	UStulWeaponFireAutomaticAbility();

protected:
	virtual bool ResolveFireSessionType(const AStulWeapon& Weapon, EStulFireSessionType& OutSessionType) const override;
};
