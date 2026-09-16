// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/StulWeaponGameplayAbility.h"
#include "StulWeaponChangeFireModeAbility.generated.h"

class UAbilityTask_WaitDelay;

/** Cancels active fire, waits for the configured transition and commits the next supported fire mode. */
UCLASS(Blueprintable)
class STULWEAPONSYSTEM_API UStulWeaponChangeFireModeAbility : public UStulWeaponGameplayAbility
{
	GENERATED_BODY()

public:
	UStulWeaponChangeFireModeAbility();

protected:
	/************************ Gameplay Ability ************************/
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	/************************ Fire Mode ************************/
	bool StartChangeDelay();
	bool CommitPendingFireMode();
	UFUNCTION()
	void HandleChangeDelayFinished();

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitDelay> ChangeDelayTask;

	FGameplayTag PendingFireMode;
	bool bFireModeCommitted = false;
};
