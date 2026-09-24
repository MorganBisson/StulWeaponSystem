#include "AbilitySystem/StulWeaponAbilitySystemComponent.h"

#include "AbilitySystem/StulWeaponGameplayAbility.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "GameplayAbilitySpec.h"
#include "StulWeaponSystem.h"
#include "StulWeaponGameplayTags.h"

namespace
{
	bool IsWeaponInputTag(const FGameplayTag Tag)
	{
		return Tag.IsValid()
			&& Tag != StulWeaponGameplayTags::InputTag_Root
			&& Tag.MatchesTag(StulWeaponGameplayTags::InputTag_Root);
	}
}

void UStulWeaponAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag InputTag)
{
	if (!IsWeaponInputTag(InputTag))
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("ASC '%s' rejected invalid weapon input tag '%s'. Expected a child of '%s'."), *GetNameSafe(this), *InputTag.ToString(), *StulWeaponGameplayTags::InputTag_Root.GetTag().ToString());
		return;
	}

	int32 BindingCount = 0;
	for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
	{
		if (AbilitySpec.Ability && AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			InputPressedSpecHandles.AddUnique(AbilitySpec.Handle);
			InputHeldSpecHandles.AddUnique(AbilitySpec.Handle);
			++BindingCount;
		}
	}
	UE_LOG(LogStulWeaponSystem, VeryVerbose, TEXT("ASC '%s' queued pressed input tag '%s' for %d ability spec(s)."), *GetNameSafe(this), *InputTag.ToString(), BindingCount);
}

void UStulWeaponAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag InputTag)
{
	if (!IsWeaponInputTag(InputTag))
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("ASC '%s' rejected invalid weapon input tag '%s'. Expected a child of '%s'."), *GetNameSafe(this), *InputTag.ToString(), *StulWeaponGameplayTags::InputTag_Root.GetTag().ToString());
		return;
	}

	int32 BindingCount = 0;
	for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
	{
		if (AbilitySpec.Ability && AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			InputHeldSpecHandles.Remove(AbilitySpec.Handle);
			InputReleasedSpecHandles.AddUnique(AbilitySpec.Handle);
			++BindingCount;
		}
	}
	UE_LOG(LogStulWeaponSystem, VeryVerbose, TEXT("ASC '%s' queued released input tag '%s' for %d ability spec(s)."), *GetNameSafe(this), *InputTag.ToString(), BindingCount);
}

void UStulWeaponAbilitySystemComponent::ProcessAbilityInput(const bool bGamePaused)
{
	if (bGamePaused)
	{
		ClearAbilityInput();
		return;
	}

	TArray<FGameplayAbilitySpecHandle> AbilitiesToActivate;
	for (const FGameplayAbilitySpecHandle Handle : InputHeldSpecHandles)
	{
		if (const FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(Handle))
		{
			const UStulWeaponGameplayAbility* WeaponAbility = Cast<UStulWeaponGameplayAbility>(AbilitySpec->Ability);
			if (WeaponAbility && WeaponAbility->GetActivationPolicy() == EStulWeaponAbilityActivationPolicy::WhileInputActive && !AbilitySpec->IsActive())
			{
				AbilitiesToActivate.AddUnique(Handle);
			}
		}
	}

	for (const FGameplayAbilitySpecHandle Handle : InputPressedSpecHandles)
	{
		if (FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(Handle))
		{
			AbilitySpecInputPressed(*AbilitySpec);

			const UStulWeaponGameplayAbility* WeaponAbility = Cast<UStulWeaponGameplayAbility>(AbilitySpec->Ability);
			if (WeaponAbility && WeaponAbility->GetActivationPolicy() == EStulWeaponAbilityActivationPolicy::OnInputTriggered && !AbilitySpec->IsActive())
			{
				AbilitiesToActivate.AddUnique(Handle);
			}
		}
		else
		{
			UE_LOG(LogStulWeaponSystem, Warning, TEXT("ASC '%s' could not process pressed input for stale ability handle '%s'."), *GetNameSafe(this), *Handle.ToString());
		}
	}

	for (const FGameplayAbilitySpecHandle Handle : AbilitiesToActivate)
	{
		if (!TryActivateAbility(Handle))
		{
			const FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(Handle);
			const UGameplayAbility* Ability = AbilitySpec ? AbilitySpec->Ability.Get() : nullptr;
			const AActor* TempOwnerActor = AbilityActorInfo.IsValid() ? AbilityActorInfo->OwnerActor.Get() : nullptr;
			const AActor* TempAvatarActor = AbilityActorInfo.IsValid() ? AbilityActorInfo->AvatarActor.Get() : nullptr;
			const APlayerController* PlayerController = AbilityActorInfo.IsValid() ? AbilityActorInfo->PlayerController.Get() : nullptr;
			const bool bLocallyControlled = AbilityActorInfo.IsValid() && AbilityActorInfo->IsLocallyControlled();
			const bool bNetAuthority = AbilityActorInfo.IsValid() && AbilityActorInfo->IsNetAuthority();
			UE_LOG(LogStulWeaponSystem, VeryVerbose, TEXT("ASC '%s' failed to activate ability '%s' for input (local: %s, authority: %s, owner: '%s', avatar: '%s', player controller: '%s')."), *GetNameSafe(this), *GetNameSafe(Ability), bLocallyControlled ? TEXT("true") : TEXT("false"), bNetAuthority ? TEXT("true") : TEXT("false"), *GetNameSafe(TempOwnerActor), *GetNameSafe(TempAvatarActor), *GetNameSafe(PlayerController));
		}
	}

	for (const FGameplayAbilitySpecHandle Handle : InputReleasedSpecHandles)
	{
		if (FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(Handle))
		{
			AbilitySpecInputReleased(*AbilitySpec);
		}
		else
		{
			UE_LOG(LogStulWeaponSystem, Warning, TEXT("ASC '%s' could not process released input for stale ability handle '%s'."), *GetNameSafe(this), *Handle.ToString());
		}
	}

	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
}

void UStulWeaponAbilitySystemComponent::ClearAbilityInput()
{
	TArray<FGameplayAbilitySpecHandle> HandlesToRelease = InputHeldSpecHandles;
	for (const FGameplayAbilitySpecHandle Handle : InputPressedSpecHandles)
	{
		HandlesToRelease.AddUnique(Handle);
	}
	for (const FGameplayAbilitySpecHandle Handle : InputReleasedSpecHandles)
	{
		HandlesToRelease.AddUnique(Handle);
	}

	for (const FGameplayAbilitySpecHandle Handle : HandlesToRelease)
	{
		if (FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(Handle))
		{
			if (AbilitySpec->InputPressed)
			{
				AbilitySpecInputReleased(*AbilitySpec);
			}
			else
			{
				AbilitySpec->InputPressed = false;
			}
		}
	}

	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
	InputHeldSpecHandles.Reset();
}

bool UStulWeaponAbilitySystemComponent::IsAbilityInputTagHeld(const FGameplayTag InputTag) const
{
	for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
	{
		if (AbilitySpec.Ability && AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag) && InputHeldSpecHandles.Contains(AbilitySpec.Handle)) return true;
	}
	return false;
}

bool UStulWeaponAbilitySystemComponent::TryActivateAbilitiesByInputTag(const FGameplayTag InputTag)
{
	bool bActivatedAny = false;
	TArray<FGameplayAbilitySpecHandle> HandlesToActivate;
	for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
	{
		if (AbilitySpec.Ability && !AbilitySpec.IsActive() && AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag)) HandlesToActivate.Add(AbilitySpec.Handle);
	}
	for (const FGameplayAbilitySpecHandle Handle : HandlesToActivate) bActivatedAny |= TryActivateAbility(Handle);
	return bActivatedAny;
}

void UStulWeaponAbilitySystemComponent::TryActivateAbilitiesOnSpawn()
{
	TArray<FGameplayAbilitySpecHandle> OnSpawnAbilityHandles;
	for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		const UStulWeaponGameplayAbility* WeaponAbility = Cast<UStulWeaponGameplayAbility>(AbilitySpec.Ability);
		if (WeaponAbility && WeaponAbility->GetActivationPolicy() == EStulWeaponAbilityActivationPolicy::OnSpawn)
		{
			OnSpawnAbilityHandles.Add(AbilitySpec.Handle);
		}
	}

	for (const FGameplayAbilitySpecHandle Handle : OnSpawnAbilityHandles)
	{
		if (const FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(Handle))
		{
			if (const UStulWeaponGameplayAbility* WeaponAbility = Cast<UStulWeaponGameplayAbility>(AbilitySpec->Ability)) WeaponAbility->TryActivateAbilityOnSpawn(AbilityActorInfo.Get(), *AbilitySpec);
		}
	}
}

void UStulWeaponAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	const AActor* PreviousAvatar = AbilityActorInfo.IsValid() ? AbilityActorInfo->AvatarActor.Get() : nullptr;
	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);
	if (InAvatarActor && InAvatarActor != PreviousAvatar) TryActivateAbilitiesOnSpawn();
}

void UStulWeaponAbilitySystemComponent::OnGiveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	Super::OnGiveAbility(AbilitySpec);

	const UStulWeaponGameplayAbility* WeaponAbility = Cast<UStulWeaponGameplayAbility>(AbilitySpec.Ability);
	if (!WeaponAbility)
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("ASC '%s' received non-weapon ability '%s'; weapon input and activation policies will not be applied."), *GetNameSafe(this), *GetNameSafe(AbilitySpec.Ability));
	}

	if (WeaponAbility && (WeaponAbility->GetActivationPolicy() == EStulWeaponAbilityActivationPolicy::OnInputTriggered || WeaponAbility->GetActivationPolicy() == EStulWeaponAbilityActivationPolicy::WhileInputActive))
	{
		bool bHasInputBinding = false;
		for (const FGameplayTag& Tag : AbilitySpec.GetDynamicSpecSourceTags())
		{
			bHasInputBinding |= IsWeaponInputTag(Tag);
		}

		if (!bHasInputBinding)
		{
			UE_LOG(LogStulWeaponSystem, Warning, TEXT("Weapon ability '%s' uses an input activation policy but spec '%s' has no valid weapon input tag."), *GetNameSafe(WeaponAbility), *AbilitySpec.Handle.ToString());
		}
	}
}

void UStulWeaponAbilitySystemComponent::OnRemoveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	InputPressedSpecHandles.Remove(AbilitySpec.Handle);
	InputReleasedSpecHandles.Remove(AbilitySpec.Handle);
	InputHeldSpecHandles.Remove(AbilitySpec.Handle);
	Super::OnRemoveAbility(AbilitySpec);
}

void UStulWeaponAbilitySystemComponent::AbilitySpecInputPressed(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputPressed(Spec);

	if (!Spec.IsActive())
	{
		return;
	}

	const UGameplayAbility* AbilityInstance = Spec.GetPrimaryInstance();
	if (!ensureMsgf(AbilityInstance, TEXT("Active weapon ability spec '%s' has no primary instance."), *Spec.Handle.ToString())) return;

	const FPredictionKey PredictionKey = AbilityInstance->GetCurrentActivationInfo().GetActivationPredictionKey();
	InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, Spec.Handle, PredictionKey);
}

void UStulWeaponAbilitySystemComponent::AbilitySpecInputReleased(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputReleased(Spec);

	if (!Spec.IsActive())
	{
		return;
	}

	const UGameplayAbility* AbilityInstance = Spec.GetPrimaryInstance();
	if (!ensureMsgf(AbilityInstance, TEXT("Active weapon ability spec '%s' has no primary instance."), *Spec.Handle.ToString())) return;

	const FPredictionKey PredictionKey = AbilityInstance->GetCurrentActivationInfo().GetActivationPredictionKey();
	InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, Spec.Handle, PredictionKey);
}
