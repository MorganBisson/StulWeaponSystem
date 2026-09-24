// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/StulWeaponReloadAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Effects/StulWeaponAmmoRestoreEffect.h"
#include "AbilitySystem/StulWeaponAttributeSet.h"
#include "StulWeaponGameplayTags.h"
#include "StulWeaponSystem.h"
#include "Weapons/StulWeapon.h"
#include "Weapons/StulWeaponDefinition.h"

UStulWeaponReloadAbility::UStulWeaponReloadAbility()
{
	ActivationPolicy = EStulWeaponAbilityActivationPolicy::OnInputTriggered;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	AmmoRestoreEffectClass = UStulWeaponAmmoRestoreEffect::StaticClass();
	SetAssetTags(FGameplayTagContainer(StulWeaponGameplayTags::Ability_Reload));
	ActivationOwnedTags.AddTag(StulWeaponGameplayTags::State_Reloading);
	ActivationBlockedTags.AddTag(StulWeaponGameplayTags::State_Reloading);
	ActivationBlockedTags.AddTag(StulWeaponGameplayTags::State_Firing);
	ActivationBlockedTags.AddTag(StulWeaponGameplayTags::State_ChangingFireMode);
	CancelAbilitiesWithTag.AddTag(StulWeaponGameplayTags::Ability_Aim);
}

/*********************************************************************************************/
/************************************ Gameplay Ability ***************************************/
/*********************************************************************************************/

bool UStulWeaponReloadAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const AStulWeapon* Weapon = ActorInfo ? Cast<AStulWeapon>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UStulWeaponAttributeSet* Attributes = Weapon ? Weapon->GetWeaponAttributeSet() : nullptr;
	const UStulWeaponDefinition* Definition = Weapon ? Weapon->GetWeaponDefinition() : nullptr;
	if (!Attributes || !Definition || !AmmoRestoreEffectClass || Attributes->GetReloadDuration() <= 0.0f)
	{
		return false;
	}

	if (Definition->ReloadType == EStulWeaponReloadType::Custom && Definition->BaseAmmoToReload <= 0)
	{
		return false;
	}

	return Attributes->GetCurrentAmmo() + KINDA_SMALL_NUMBER < Attributes->GetMaxAmmo();
}

void UStulWeaponReloadAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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

	AmmoRestoredDuringAbility = 0;
	ReloadStartAmmo = GetStulWeaponAttributeSet() ? GetStulWeaponAttributeSet()->GetCurrentAmmo() : 0.0f;
	if (!StartReloadDelay())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bReloadStarted = true;
	K2_AddGameplayCue(StulWeaponGameplayTags::GameplayCue_Reload, FGameplayEffectContextHandle(), true);
}

void UStulWeaponReloadAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const bool bReplicateEndAbility, const bool bWasCancelled)
{
	if (ReloadDelayTask)
	{
		ReloadDelayTask->EndTask();
		ReloadDelayTask = nullptr;
	}

	if (bReloadStarted && ActorInfo && ActorInfo->IsNetAuthority())
	{
		ExecuteReloadPresentationCue(bWasCancelled ? StulWeaponGameplayTags::GameplayCue_ReloadCancelled : StulWeaponGameplayTags::GameplayCue_ReloadCompleted, static_cast<float>(AmmoRestoredDuringAbility));
	}
	bReloadStarted = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	AmmoRestoredDuringAbility = 0;
	ReloadStartAmmo = 0.0f;
}

/*********************************************************************************************/
/**************************************** Reload *********************************************/
/*********************************************************************************************/

bool UStulWeaponReloadAbility::StartReloadDelay()
{
	const UStulWeaponAttributeSet* Attributes = GetStulWeaponAttributeSet();
	if (!Attributes || Attributes->GetReloadDuration() <= 0.0f)
	{
		return false;
	}

	ReloadDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, Attributes->GetReloadDuration());
	if (!ReloadDelayTask)
	{
		return false;
	}

	ReloadDelayTask->OnFinish.AddDynamic(this, &ThisClass::HandleReloadDelayFinished);
	ReloadDelayTask->ReadyForActivation();
	return true;
}

void UStulWeaponReloadAbility::HandleReloadDelayFinished()
{
	ReloadDelayTask = nullptr;
	const int32 AmmoToReload = CalculateAmmoToReload();
	if (AmmoToReload <= 0)
	{
		CompleteReload();
		return;
	}

	if (!ApplyAmmoRestore(AmmoToReload))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	AmmoRestoredDuringAbility += AmmoToReload;
	ExecuteReloadPresentationCue(StulWeaponGameplayTags::GameplayCue_ReloadCommit, static_cast<float>(AmmoToReload));
	if (!IsActive())
	{
		return;
	}

	const AStulWeapon* Weapon = GetStulWeapon();
	const UStulWeaponDefinition* Definition = Weapon ? Weapon->GetWeaponDefinition() : nullptr;
	const bool bHasAmmoLeftToReload = CalculateAmmoToReload() > 0;
	const bool bShouldContinueIncrementalReload = Definition && Definition->ReloadType == EStulWeaponReloadType::Custom && bHasAmmoLeftToReload;
	if (bShouldContinueIncrementalReload)
	{
		if (StartReloadDelay())
		{
			return;
		}

		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	CompleteReload();
}

void UStulWeaponReloadAbility::CompleteReload()
{
	if (!CurrentActorInfo) return;
	const bool bIsAuthority = CurrentActorInfo->IsNetAuthority();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bIsAuthority, false);
}

int32 UStulWeaponReloadAbility::CalculateAmmoToReload() const
{
	const AStulWeapon* Weapon = GetStulWeapon();
	const UStulWeaponDefinition* Definition = Weapon ? Weapon->GetWeaponDefinition() : nullptr;
	const UStulWeaponAttributeSet* Attributes = GetStulWeaponAttributeSet();
	if (!Definition || !Attributes)
	{
		return 0;
	}

	const bool bAuthority = CurrentActorInfo && CurrentActorInfo->IsNetAuthority();
	const float EffectiveCurrentAmmo = bAuthority ? Attributes->GetCurrentAmmo() : FMath::Min(Attributes->GetMaxAmmo(), ReloadStartAmmo + static_cast<float>(AmmoRestoredDuringAbility));
	const int32 MissingAmmo = FMath::Max(0, FMath::CeilToInt(Attributes->GetMaxAmmo() - EffectiveCurrentAmmo - KINDA_SMALL_NUMBER));
	return Definition->ReloadType == EStulWeaponReloadType::Full ? MissingAmmo : FMath::Min(FMath::Max(1, Definition->BaseAmmoToReload), MissingAmmo);
}

bool UStulWeaponReloadAbility::ApplyAmmoRestore(const int32 AmmoAmount)
{
	UStulWeaponAttributeSet* Attributes = GetStulWeaponAttributeSet();
	if (AmmoAmount <= 0 || !AmmoRestoreEffectClass || !Attributes)
	{
		return false;
	}
	if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority()) return true;

	const float PreviousAmmo = Attributes->GetCurrentAmmo();

	FGameplayEffectSpecHandle RestoreSpec = MakeOutgoingGameplayEffectSpec(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, AmmoRestoreEffectClass, GetAbilityLevel());
	if (!RestoreSpec.IsValid())
	{
		UE_LOG(LogStulWeaponSystem, Error, TEXT("Reload ability '%s' could not create its ammo restore Gameplay Effect spec."), *GetNameSafe(this));
		return false;
	}

	RestoreSpec.Data->SetSetByCallerMagnitude(StulWeaponGameplayTags::SetByCaller_AmmoRestore, static_cast<float>(AmmoAmount));
	ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, RestoreSpec);
	const bool bAmmoRestored = Attributes->GetCurrentAmmo() > PreviousAmmo + KINDA_SMALL_NUMBER;
	if (!bAmmoRestored) UE_LOG(LogStulWeaponSystem, Error, TEXT("Reload ability '%s' failed to restore ammo on authority (before: %.2f, after: %.2f, max: %.2f, requested: %d, effect: '%s')."), *GetNameSafe(this), PreviousAmmo, Attributes->GetCurrentAmmo(), Attributes->GetMaxAmmo(), AmmoAmount, *GetNameSafe(AmmoRestoreEffectClass.Get()));
	if (bAmmoRestored)
	{
		if (UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo()) AbilitySystem->ForceReplication();
		UE_LOG(LogStulWeaponSystem, Verbose, TEXT("Reload ability '%s' restored %d ammo on authority (before: %.2f, after: %.2f, max: %.2f)."), *GetNameSafe(this), AmmoAmount, PreviousAmmo, Attributes->GetCurrentAmmo(), Attributes->GetMaxAmmo());
	}
	return bAmmoRestored;
}

void UStulWeaponReloadAbility::ExecuteReloadPresentationCue(const FGameplayTag GameplayCueTag, const float Magnitude)
{
	// Delayed reload commits have independent client/server clocks. Only authority emits one-shot cues;
	// the persistent Reload cue remains predicted so the owner still starts presentation immediately.
	if (!GameplayCueTag.IsValid() || !CurrentActorInfo || !CurrentActorInfo->IsNetAuthority()) return;

	FGameplayCueParameters Parameters;
	Parameters.RawMagnitude = Magnitude;
	if (AStulWeapon* Weapon = GetStulWeapon())
	{
		Parameters.Instigator = Weapon->GetOwner();
		Parameters.EffectCauser = Weapon;
		Parameters.SourceObject = Weapon->GetWeaponDefinition();
	}
	K2_ExecuteGameplayCueWithParams(GameplayCueTag, Parameters);
}
