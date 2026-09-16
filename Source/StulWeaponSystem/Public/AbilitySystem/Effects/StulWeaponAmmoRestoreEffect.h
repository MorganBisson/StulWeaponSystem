// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "StulWeaponAmmoRestoreEffect.generated.h"

/** Instant predicted effect that adds the SetByCaller restore magnitude to CurrentAmmo. */
UCLASS()
class STULWEAPONSYSTEM_API UStulWeaponAmmoRestoreEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UStulWeaponAmmoRestoreEffect();
};
