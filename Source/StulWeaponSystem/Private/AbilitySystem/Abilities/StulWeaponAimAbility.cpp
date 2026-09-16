// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/StulWeaponAimAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "StulWeaponGameplayTags.h"
#include "Weapons/StulWeapon.h"
#include "Weapons/StulWeaponDefinition.h"

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
	bAimStarted = true;
	if (!IsActive())
	{
		return;
	}
	SendAimEvent(StulWeaponGameplayTags::Event_Aim_Start);
}

void UStulWeaponAimAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const bool bReplicateEndAbility, const bool bWasCancelled)
{
	if (WaitInputReleaseTask)
	{
		WaitInputReleaseTask->EndTask();
		WaitInputReleaseTask = nullptr;
	}

	if (bAimStarted)
	{
		SendAimEvent(StulWeaponGameplayTags::Event_Aim_End);
	}
	bAimStarted = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

/*********************************************************************************************/
/**************************************** Input **********************************************/
/*********************************************************************************************/

void UStulWeaponAimAbility::HandleInputReleased(const float /*TimeHeld*/)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UStulWeaponAimAbility::SendAimEvent(const FGameplayTag EventTag) const
{
	AStulWeapon* Weapon = GetStulWeapon();
	if (!Weapon || !EventTag.IsValid())
	{
		return;
	}

	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = Weapon->GetOwner();
	Payload.Target = Weapon;
	Payload.OptionalObject = Weapon->GetWeaponDefinition();
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Weapon, EventTag, Payload);
}
