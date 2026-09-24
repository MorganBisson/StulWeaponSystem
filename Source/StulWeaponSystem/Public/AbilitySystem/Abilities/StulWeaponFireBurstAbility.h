#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/StulWeaponFireAbility.h"
#include "StulWeaponFireBurstAbility.generated.h"

/** One input activation produces one complete bounded burst. */
UCLASS(Blueprintable)
class STULWEAPONSYSTEM_API UStulWeaponFireBurstAbility : public UStulWeaponFireAbility
{
	GENERATED_BODY()

public:
	UStulWeaponFireBurstAbility();

protected:
	virtual bool ResolveFireSessionType(const AStulWeapon& Weapon, EStulFireSessionType& OutSessionType) const override;
};
