#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Weapons/StulWeaponTypes.h"
#include "StulWeaponGameplayAbility.generated.h"

class AStulWeapon;
class UStulWeaponAttributeSet;
struct FGameplayAbilitySpec;

/** Common base class for abilities executed by a Stul weapon. */
UCLASS(Abstract, Blueprintable)
class STULWEAPONSYSTEM_API UStulWeaponGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UStulWeaponGameplayAbility();

	EStulWeaponAbilityActivationPolicy GetActivationPolicy() const { return ActivationPolicy; }
	void TryActivateAbilityOnSpawn(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) const;

	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Ability")
	AStulWeapon* GetStulWeapon() const;

	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Ability")
	UStulWeaponAttributeSet* GetStulWeaponAttributeSet() const;

	/** Returns the gameplay actor that owns the weapon, if one is assigned. */
	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Ability")
	AActor* GetWeaponOwnerActor() const;

	/** Queries the owner interface, then falls back to the owner's eye viewpoint. */
	UFUNCTION(BlueprintCallable, Category = "Stul Weapon System|Ability")
	bool GetWeaponViewData(FStulWeaponViewData& OutViewData) const;

	/** Queries movement state through the project-facing owner interface. */
	UFUNCTION(BlueprintCallable, Category = "Stul Weapon System|Ability")
	bool GetWeaponMovementData(FStulWeaponMovementData& OutMovementData) const;

protected:
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Activation")
	EStulWeaponAbilityActivationPolicy ActivationPolicy = EStulWeaponAbilityActivationPolicy::OnInputTriggered;
};
