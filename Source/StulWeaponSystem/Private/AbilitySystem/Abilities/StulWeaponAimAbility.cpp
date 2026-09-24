// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/StulWeaponAimAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "StulWeaponGameplayTags.h"
#include "Weapons/StulWeapon.h"

UStulWeaponAimAbility::UStulWeaponAimAbility()
{
	ActivationPolicy = EStulWeaponAbilityActivationPolicy::OnInputTriggered;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	SetAssetTags(FGameplayTagContainer(StulWeaponGameplayTags::Ability_Aim));
	ActivationOwnedTags.AddTag(StulWeaponGameplayTags::State_Aiming);
	ActivationBlockedTags.AddTag(StulWeaponGameplayTags::State_Aiming);
	ActivationBlockedTags.AddTag(StulWeaponGameplayTags::State_Reloading);
}

/*********************************************************************************************/
/************************************ Gameplay Ability ***************************************/
/*********************************************************************************************/

void UStulWeaponAimAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AStulWeapon* Weapon = GetStulWeapon();
	if (!Weapon)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	WaitInputReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, false);
	if (!WaitInputReleaseTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	WaitInputReleaseTask->OnRelease.AddDynamic(this, &ThisClass::HandleInputReleased);
	WaitInputReleaseTask->ReadyForActivation();
	if (ActorInfo && ActorInfo->IsLocallyControlled()) Weapon->SetAimPresentationActive(true);
	K2_AddGameplayCue(StulWeaponGameplayTags::GameplayCue_Aim, FGameplayEffectContextHandle(), true);
}

void UStulWeaponAimAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const bool bReplicateEndAbility, const bool bWasCancelled)
{
	if (ActorInfo && ActorInfo->IsLocallyControlled())
	{
		if (AStulWeapon* Weapon = Cast<AStulWeapon>(ActorInfo->AvatarActor.Get())) Weapon->SetAimPresentationActive(false);
	}

	if (WaitInputReleaseTask)
	{
		WaitInputReleaseTask->EndTask();
		WaitInputReleaseTask = nullptr;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

/*********************************************************************************************/
/**************************************** Input **********************************************/
/*********************************************************************************************/

void UStulWeaponAimAbility::HandleInputReleased(const float /*TimeHeld*/)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
