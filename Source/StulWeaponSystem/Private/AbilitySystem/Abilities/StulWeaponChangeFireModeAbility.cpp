// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/StulWeaponChangeFireModeAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystem/StulWeaponAbilitySystemComponent.h"
#include "AbilitySystem/StulWeaponAttributeSet.h"
#include "StulWeaponGameplayTags.h"
#include "StulWeaponSystem.h"
#include "Weapons/StulWeapon.h"

UStulWeaponChangeFireModeAbility::UStulWeaponChangeFireModeAbility()
{
	ActivationPolicy = EStulWeaponAbilityActivationPolicy::OnInputTriggered;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	SetAssetTags(FGameplayTagContainer(StulWeaponGameplayTags::Ability_ChangeFireMode));
	ActivationOwnedTags.AddTag(StulWeaponGameplayTags::State_ChangingFireMode);
	ActivationBlockedTags.AddTag(StulWeaponGameplayTags::State_ChangingFireMode);
	ActivationBlockedTags.AddTag(StulWeaponGameplayTags::State_Reloading);
	CancelAbilitiesWithTag.AddTag(StulWeaponGameplayTags::Ability_Fire);
}

/*********************************************************************************************/
/************************************ Gameplay Ability ***************************************/
/*********************************************************************************************/

bool UStulWeaponChangeFireModeAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const AStulWeapon* Weapon = ActorInfo ? Cast<AStulWeapon>(ActorInfo->AvatarActor.Get()) : nullptr;
	FGameplayTag NextFireMode;
	return Weapon && Weapon->GetNextFireMode(NextFireMode);
}

void UStulWeaponChangeFireModeAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AStulWeapon* Weapon = GetStulWeapon();
	if (!Weapon || !Weapon->GetNextFireMode(PendingFireMode) || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bFireModeCommitted = false;
	K2_AddGameplayCue(StulWeaponGameplayTags::GameplayCue_ChangeFireMode, FGameplayEffectContextHandle(), true);
	if (!StartChangeDelay())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

void UStulWeaponChangeFireModeAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const bool bReplicateEndAbility, const bool bWasCancelled)
{
	if (ChangeDelayTask)
	{
		ChangeDelayTask->EndTask();
		ChangeDelayTask = nullptr;
	}

	if (!bWasCancelled && !bFireModeCommitted) bFireModeCommitted = CommitPendingFireMode();
	if (bWasCancelled && bFireModeCommitted && ActorInfo && !ActorInfo->IsNetAuthority())
	{
		if (AStulWeapon* Weapon = GetStulWeapon()) Weapon->RestoreAuthoritativeFireMode();
	}

	UStulWeaponAbilitySystemComponent* AbilitySystem = Cast<UStulWeaponAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
	const bool bResumeFire = !bWasCancelled && bFireModeCommitted && ActorInfo && ActorInfo->IsLocallyControlled() && AbilitySystem && AbilitySystem->IsAbilityInputTagHeld(StulWeaponGameplayTags::Input_Fire);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	PendingFireMode = FGameplayTag();
	bFireModeCommitted = false;
	if (bResumeFire) AbilitySystem->TryActivateAbilitiesByInputTag(StulWeaponGameplayTags::Input_Fire);
}

/*********************************************************************************************/
/*************************************** Fire Mode *******************************************/
/*********************************************************************************************/

bool UStulWeaponChangeFireModeAbility::StartChangeDelay()
{
	const UStulWeaponAttributeSet* Attributes = GetStulWeaponAttributeSet();
	if (!Attributes)
	{
		return false;
	}

	const float ChangeDuration = FMath::Max(0.0f, Attributes->GetFireModeChangeDuration());
	if (ChangeDuration <= KINDA_SMALL_NUMBER)
	{
		HandleChangeDelayFinished();
		return true;
	}

	ChangeDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, ChangeDuration);
	if (!ChangeDelayTask)
	{
		return false;
	}

	ChangeDelayTask->OnFinish.AddDynamic(this, &ThisClass::HandleChangeDelayFinished);
	ChangeDelayTask->ReadyForActivation();
	return true;
}

bool UStulWeaponChangeFireModeAbility::CommitPendingFireMode()
{
	AStulWeapon* Weapon = GetStulWeapon();
	if (!Weapon || !PendingFireMode.IsValid())
	{
		return false;
	}

	const bool bCommitted = Weapon->CommitFireModeChange(PendingFireMode);
	if (!bCommitted) UE_LOG(LogStulWeaponSystem, Warning, TEXT("Fire mode ability '%s' failed to commit mode '%s' for weapon '%s'."), *GetNameSafe(this), *PendingFireMode.ToString(), *GetNameSafe(Weapon));
	return bCommitted;
}

void UStulWeaponChangeFireModeAbility::HandleChangeDelayFinished()
{
	ChangeDelayTask = nullptr;
	bFireModeCommitted = CommitPendingFireMode();
	if (!bFireModeCommitted)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority()) return;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

