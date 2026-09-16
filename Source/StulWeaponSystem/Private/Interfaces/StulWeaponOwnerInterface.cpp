// Fill out your copyright notice in the Description page of Project Settings.


#include "Interfaces/StulWeaponOwnerInterface.h"

bool IStulWeaponOwnerInterface::GetWeaponViewData_Implementation(FStulWeaponViewData& OutViewData) const
{
	OutViewData = FStulWeaponViewData();
	return false;
}

bool IStulWeaponOwnerInterface::GetWeaponMovementData_Implementation(FStulWeaponMovementData& OutMovementData) const
{
	OutMovementData = FStulWeaponMovementData();
	return false;
}
