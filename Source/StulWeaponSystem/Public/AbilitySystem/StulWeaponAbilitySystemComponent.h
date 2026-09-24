#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "StulWeaponAbilitySystemComponent.generated.h"

/** Weapon ASC that routes semantic input tags to replicated ability specs. */
UCLASS()
class STULWEAPONSYSTEM_API UStulWeaponAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	/** Queues specs bound to a semantic input tag as pressed and held. */
	void AbilityInputTagPressed(FGameplayTag InputTag);

	/** Queues specs bound to a semantic input tag as released. */
	void AbilityInputTagReleased(FGameplayTag InputTag);

	/** Processes queued input once after the owning project has gathered input for the frame. */
	void ProcessAbilityInput(bool bGamePaused);

	/** Releases active inputs and clears every input queue. */
	void ClearAbilityInput();

	/** Returns whether at least one ability mapped to this input is currently held. */
	bool IsAbilityInputTagHeld(FGameplayTag InputTag) const;

	/** Attempts to activate every inactive ability mapped to this input tag. */
	bool TryActivateAbilitiesByInputTag(FGameplayTag InputTag);

	void TryActivateAbilitiesOnSpawn();
	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;

protected:
	virtual void OnGiveAbility(FGameplayAbilitySpec& AbilitySpec) override;
	virtual void OnRemoveAbility(FGameplayAbilitySpec& AbilitySpec) override;
	virtual void AbilitySpecInputPressed(FGameplayAbilitySpec& Spec) override;
	virtual void AbilitySpecInputReleased(FGameplayAbilitySpec& Spec) override;

private:
	TArray<FGameplayAbilitySpecHandle> InputPressedSpecHandles;
	TArray<FGameplayAbilitySpecHandle> InputReleasedSpecHandles;
	TArray<FGameplayAbilitySpecHandle> InputHeldSpecHandles;
};
