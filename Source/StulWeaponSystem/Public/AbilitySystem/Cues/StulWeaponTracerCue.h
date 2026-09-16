// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "StulWeaponTracerCue.generated.h"

/** Plays one pooled cosmetic Niagara tracer for a hitscan trajectory. */
UCLASS()
class STULWEAPONSYSTEM_API UStulWeaponTracerCue : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
};
