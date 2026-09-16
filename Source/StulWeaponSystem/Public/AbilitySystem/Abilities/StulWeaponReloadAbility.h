// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/StulWeaponGameplayAbility.h"
#include "StulWeaponReloadAbility.generated.h"

class UAbilityTask_WaitDelay;
class UGameplayEffect;

/** Predicted reload ability supporting one-shot full reloads and interruptible incremental reloads. */
UCLASS(Blueprintable)
class STULWEAPONSYSTEM_API UStulWeaponReloadAbility : public UStulWeaponGameplayAbility
{
	GENERATED_BODY()

public:
	UStulWeaponReloadAbility();

	/** Interrupts a Custom reload immediately when ammo is available, or after the current insertion when empty. */
	bool RequestInterruptForFire(bool& bOutWaitForCurrentCycle);

protected:
	/************************ Gameplay Ability ************************/
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reload")
	TSubclassOf<UGameplayEffect> AmmoRestoreEffectClass;

private:
	/************************ Reload ************************/
	bool StartReloadDelay();
	UFUNCTION()
	void HandleReloadDelayFinished();
	void CompleteReload();
	int32 CalculateAmmoToReload() const;
	bool ApplyAmmoRestore(int32 AmmoAmount);
	void SendReloadEvent(FGameplayTag EventTag, float Magnitude) const;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitDelay> ReloadDelayTask;

	int32 AmmoRestoredDuringAbility = 0;
	float ReloadStartAmmo = 0.0f;
	bool bReloadStarted = false;
	bool bInterruptAfterCurrentCycle = false;
};
