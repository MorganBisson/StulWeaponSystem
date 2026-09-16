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

	const TArray<FGameplayAbilitySpecHandle>* BoundHandles = InputBindings.Find(InputTag);
	if (!BoundHandles)
	{
		UE_LOG(LogStulWeaponSystem, VeryVerbose, TEXT("ASC '%s' received pressed input tag '%s' with no bound ability."), *GetNameSafe(this), *InputTag.ToString());
		return;
	}

	for (const FGameplayAbilitySpecHandle Handle : *BoundHandles)
	{
		InputPressedSpecHandles.AddUnique(Handle);
		InputHeldSpecHandles.AddUnique(Handle);
	}
	UE_LOG(LogStulWeaponSystem, VeryVerbose, TEXT("ASC '%s' queued pressed input tag '%s' for %d ability spec(s)."), *GetNameSafe(this), *InputTag.ToString(), BoundHandles->Num());
}

void UStulWeaponAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag InputTag)
{
	if (!IsWeaponInputTag(InputTag))
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("ASC '%s' rejected invalid weapon input tag '%s'. Expected a child of '%s'."), *GetNameSafe(this), *InputTag.ToString(), *StulWeaponGameplayTags::InputTag_Root.GetTag().ToString());
		return;
	}

	const TArray<FGameplayAbilitySpecHandle>* BoundHandles = InputBindings.Find(InputTag);
	if (!BoundHandles)
	{
		UE_LOG(LogStulWeaponSystem, VeryVerbose, TEXT("ASC '%s' received released input tag '%s' with no bound ability."), *GetNameSafe(this), *InputTag.ToString());
		return;
	}

	for (const FGameplayAbilitySpecHandle Handle : *BoundHandles)
	{
		InputHeldSpecHandles.Remove(Handle);
		InputReleasedSpecHandles.AddUnique(Handle);
	}
	UE_LOG(LogStulWeaponSystem, VeryVerbose, TEXT("ASC '%s' queued released input tag '%s' for %d ability spec(s)."), *GetNameSafe(this), *InputTag.ToString(), BoundHandles->Num());
}

void UStulWeaponAbilitySystemComponent::ProcessAbilityInput(const bool bGamePaused)
{
	if (bGamePaused)
	{
		ClearAbilityInput();
		return;
	}

	TArray<FGameplayAbilitySpecHandle> AbilitiesToActivate;
	InputHeldSpecHandles.RemoveAll([this](const FGameplayAbilitySpecHandle Handle)
	{
		if (FindAbilitySpecFromHandle(Handle))
		{
			return false;
		}

		UE_LOG(LogStulWeaponSystem, Warning, TEXT("ASC '%s' removed stale held-input ability handle '%s'."), *GetNameSafe(this), *Handle.ToString());
		return true;
	});

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
	const TArray<FGameplayAbilitySpecHandle>* BoundHandles = InputBindings.Find(InputTag);
	if (!BoundHandles)
	{
		return false;
	}

	return BoundHandles->ContainsByPredicate([this](const FGameplayAbilitySpecHandle Handle)
	{
		return InputHeldSpecHandles.Contains(Handle);
	});
}

bool UStulWeaponAbilitySystemComponent::TryActivateAbilitiesByInputTag(const FGameplayTag InputTag)
{
	const TArray<FGameplayAbilitySpecHandle>* BoundHandles = InputBindings.Find(InputTag);
	if (!BoundHandles)
	{
		return false;
	}

	bool bActivatedAny = false;
	for (const FGameplayAbilitySpecHandle Handle : *BoundHandles)
	{
		const FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(Handle);
		if (AbilitySpec && !AbilitySpec->IsActive()) bActivatedAny |= TryActivateAbility(Handle);
	}
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
			TryActivateAbilityOnSpawn(*AbilitySpec);
		}
	}
}

void UStulWeaponAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);
	TryActivateAbilitiesOnSpawn();
}

void UStulWeaponAbilitySystemComponent::OnGiveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	Super::OnGiveAbility(AbilitySpec);

	const UStulWeaponGameplayAbility* WeaponAbility = Cast<UStulWeaponGameplayAbility>(AbilitySpec.Ability);
	if (!WeaponAbility)
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("ASC '%s' received non-weapon ability '%s'; weapon input and activation policies will not be applied."), *GetNameSafe(this), *GetNameSafe(AbilitySpec.Ability));
	}

	AddInputBindings(AbilitySpec);

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

	TryActivateAbilityOnSpawn(AbilitySpec);
}

void UStulWeaponAbilitySystemComponent::OnRemoveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	InputPressedSpecHandles.Remove(AbilitySpec.Handle);
	InputReleasedSpecHandles.Remove(AbilitySpec.Handle);
	InputHeldSpecHandles.Remove(AbilitySpec.Handle);
	RemoveInputBindings(AbilitySpec);
	Super::OnRemoveAbility(AbilitySpec);
}

void UStulWeaponAbilitySystemComponent::AbilitySpecInputPressed(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputPressed(Spec);

	if (!Spec.IsActive())
	{
		return;
	}

	const TArray<UGameplayAbility*> AbilityInstances = Spec.GetAbilityInstances();
	if (AbilityInstances.IsEmpty())
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Active ability '%s' has no instance for replicated InputPressed event. Weapon abilities must be Instanced Per Actor."), *GetNameSafe(Spec.Ability));
		return;
	}

	const UGameplayAbility* AbilityInstance = AbilityInstances.Last();
	if (!AbilityInstance)
	{
		UE_LOG(LogStulWeaponSystem, Error, TEXT("Ability '%s' returned a null instance for replicated InputPressed event."), *GetNameSafe(Spec.Ability));
		return;
	}

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

	const TArray<UGameplayAbility*> AbilityInstances = Spec.GetAbilityInstances();
	if (AbilityInstances.IsEmpty())
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Active ability '%s' has no instance for replicated InputReleased event. Weapon abilities must be Instanced Per Actor."), *GetNameSafe(Spec.Ability));
		return;
	}

	const UGameplayAbility* AbilityInstance = AbilityInstances.Last();
	if (!AbilityInstance)
	{
		UE_LOG(LogStulWeaponSystem, Error, TEXT("Ability '%s' returned a null instance for replicated InputReleased event."), *GetNameSafe(Spec.Ability));
		return;
	}

	const FPredictionKey PredictionKey = AbilityInstance->GetCurrentActivationInfo().GetActivationPredictionKey();
	InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, Spec.Handle, PredictionKey);
}

void UStulWeaponAbilitySystemComponent::TryActivateAbilityOnSpawn(const FGameplayAbilitySpec& AbilitySpec)
{
	const UStulWeaponGameplayAbility* WeaponAbility = Cast<UStulWeaponGameplayAbility>(AbilitySpec.Ability);
	const AActor* TempAvatarActor = AbilityActorInfo.IsValid() ? AbilityActorInfo->AvatarActor.Get() : nullptr;
	if (!WeaponAbility || WeaponAbility->GetActivationPolicy() != EStulWeaponAbilityActivationPolicy::OnSpawn || AbilitySpec.IsActive() || !TempAvatarActor || TempAvatarActor->GetTearOff() || TempAvatarActor->IsActorBeingDestroyed() || TempAvatarActor->GetLifeSpan() > 0.0f)
	{
		return;
	}

	const EGameplayAbilityNetExecutionPolicy::Type NetExecutionPolicy = WeaponAbility->GetNetExecutionPolicy();
	const bool bLocallyControlled = AbilityActorInfo->IsLocallyControlled();
	const bool bNetAuthority = AbilityActorInfo->IsNetAuthority();
	const bool bLocalExecution = NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::LocalOnly || NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	const bool bServerExecution = NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::ServerOnly || NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	if ((bLocallyControlled && bLocalExecution) || (bNetAuthority && bServerExecution))
	{
		if (!TryActivateAbility(AbilitySpec.Handle))
		{
			UE_LOG(LogStulWeaponSystem, VeryVerbose, TEXT("ASC '%s' could not activate OnSpawn ability '%s' (activation requirements may not be met)."), *GetNameSafe(this), *GetNameSafe(WeaponAbility));
		}
	}
}

void UStulWeaponAbilitySystemComponent::AddInputBindings(const FGameplayAbilitySpec& AbilitySpec)
{
	for (const FGameplayTag& Tag : AbilitySpec.GetDynamicSpecSourceTags())
	{
		if (IsWeaponInputTag(Tag))
		{
			InputBindings.FindOrAdd(Tag).AddUnique(AbilitySpec.Handle);
		}
		else if (Tag == StulWeaponGameplayTags::InputTag_Root)
		{
			UE_LOG(LogStulWeaponSystem, Warning, TEXT("Ability '%s' uses root tag '%s' as an input binding; use a child tag instead."), *GetNameSafe(AbilitySpec.Ability), *Tag.ToString());
		}
	}
}

void UStulWeaponAbilitySystemComponent::RemoveInputBindings(const FGameplayAbilitySpec& AbilitySpec)
{
	for (const FGameplayTag& Tag : AbilitySpec.GetDynamicSpecSourceTags())
	{
		if (TArray<FGameplayAbilitySpecHandle>* BoundHandles = InputBindings.Find(Tag))
		{
			BoundHandles->Remove(AbilitySpec.Handle);
			if (BoundHandles->IsEmpty())
			{
				InputBindings.Remove(Tag);
			}
		}
	}
}
