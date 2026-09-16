// Fill out your copyright notice in the Description page of Project Settings.


#include "Test/StulWeaponTestController.h"

#include "Components/StulWeaponManagerComponent.h"
#include "GameFramework/Pawn.h"

void AStulWeaponTestController::PostProcessInput(const float DeltaTime, const bool bGamePaused)
{
	Super::PostProcessInput(DeltaTime, bGamePaused);

	if (!StulWeaponManager.IsValid()) CacheWeaponManager(GetPawn());
	
	if (StulWeaponManager.IsValid())
	{
		StulWeaponManager->ProcessAbilityInput(bGamePaused);
	}
}

void AStulWeaponTestController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	CacheWeaponManager(InPawn);
}

void AStulWeaponTestController::AcknowledgePossession(APawn* InPawn)
{
	Super::AcknowledgePossession(InPawn);
	CacheWeaponManager(InPawn);
}

void AStulWeaponTestController::OnUnPossess()
{
	StulWeaponManager.Reset();
	Super::OnUnPossess();
}

void AStulWeaponTestController::CacheWeaponManager(APawn* InPawn)
{
	StulWeaponManager = InPawn ? InPawn->FindComponentByClass<UStulWeaponManagerComponent>() : nullptr;
}
