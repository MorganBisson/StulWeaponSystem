// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "StulWeaponTestController.generated.h"

class UStulWeaponManagerComponent;
/**
 * 
 */
UCLASS()
class STULWEAPONSYSTEM_API AStulWeaponTestController : public APlayerController
{
	GENERATED_BODY()
	
	
public:
	virtual void PostProcessInput(const float DeltaTime, const bool bGamePaused) override;
	virtual void AcknowledgePossession(APawn* InPawn) override;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

private: 
	void CacheWeaponManager(APawn* InPawn);

	TWeakObjectPtr<UStulWeaponManagerComponent> StulWeaponManager; 
};
