// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/StulWeaponGameplayAbility.h"
#include "StulWeaponAimAbility.generated.h"

class UAbilityTask_WaitInputRelease;

/** Predicted hold-to-aim ability. Camera and animation interpolation remain project-owned. */
UCLASS(Blueprintable)
class STULWEAPONSYSTEM_API UStulWeaponAimAbility : public UStulWeaponGameplayAbility
{
	GENERATED_BODY()

public:
	UStulWeaponAimAbility();

protected:
	/************************ Gameplay Ability ************************/
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	/************************ Input ************************/
	UFUNCTION()
	void HandleInputReleased(float TimeHeld);

	void SendAimEvent(FGameplayTag EventTag) const;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitInputRelease> WaitInputReleaseTask;

	bool bAimStarted = false;
};
