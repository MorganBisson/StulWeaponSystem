// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Weapons/StulWeaponTypes.h"
#include "StulWeaponOwnerInterface.generated.h"

UINTERFACE(BlueprintType)
class STULWEAPONSYSTEM_API UStulWeaponOwnerInterface : public UInterface
{
	GENERATED_BODY()
};

/** Project-facing bridge that keeps the weapon system independent from FPS, TPS and AI classes. */
class STULWEAPONSYSTEM_API IStulWeaponOwnerInterface
{
	GENERATED_BODY()

public:
	/** Returns the viewpoint used to select the point the weapon should aim at. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Stul Weapon System|Owner")
	bool GetWeaponViewData(FStulWeaponViewData& OutViewData) const;
	virtual bool GetWeaponViewData_Implementation(FStulWeaponViewData& OutViewData) const;

	/** Returns normalized movement information used to calculate weapon spread. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Stul Weapon System|Owner")
	bool GetWeaponMovementData(FStulWeaponMovementData& OutMovementData) const;
	virtual bool GetWeaponMovementData_Implementation(FStulWeaponMovementData& OutMovementData) const;
};
