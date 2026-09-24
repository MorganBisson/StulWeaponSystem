#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "StulWeaponFireCue.generated.h"

/** Plays the muzzle flash and firing sound resolved from the runtime weapon presentation. */
UCLASS()
class STULWEAPONSYSTEM_API UStulWeaponFireCue : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
};
