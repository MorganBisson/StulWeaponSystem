#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "StulWeaponImpactCue.generated.h"

/** Plays the surface response resolved from the ammunition captured by a weapon shot. */
UCLASS()
class STULWEAPONSYSTEM_API UStulWeaponImpactCue : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
};
