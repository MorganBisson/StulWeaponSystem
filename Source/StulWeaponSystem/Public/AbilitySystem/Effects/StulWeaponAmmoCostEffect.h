#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "StulWeaponAmmoCostEffect.generated.h"

/** Instant predicted cost that subtracts the SetByCaller ammo magnitude from CurrentAmmo. */
UCLASS()
class STULWEAPONSYSTEM_API UStulWeaponAmmoCostEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UStulWeaponAmmoCostEffect();
};
