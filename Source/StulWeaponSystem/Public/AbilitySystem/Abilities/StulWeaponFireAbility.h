#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/StulWeaponGameplayAbility.h"
#include "Weapons/Shooting/StulWeaponFireSessionTypes.h"
#include "StulWeaponFireAbility.generated.h"

class AStulWeapon;
class UAbilityTask_WaitInputRelease;

/** Common GAS lifecycle and per-shot cost authorization for weapon fire abilities. */
UCLASS(Blueprintable)
class STULWEAPONSYSTEM_API UStulWeaponFireAbility : public UStulWeaponGameplayAbility
{
	GENERATED_BODY()

public:
	UStulWeaponFireAbility();

protected:
	/************************ Gameplay Ability ************************/
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo) const override;

	/************************ Fire Session ************************/
	/** Base compatibility maps the weapon's current mode. Concrete subclasses return one fixed type. */
	virtual bool ResolveFireSessionType(const AStulWeapon& Weapon, EStulFireSessionType& OutSessionType) const;
	bool StartFireSession(EStulFireSessionType SessionType);
	bool AuthorizeShot(const FStulShotId& ShotId);
	void HandleFireSessionEnded(bool bWasCancelled);

	UFUNCTION()
	void HandleInputReleased(float TimeHeld);

private:
	/************************ Runtime State ************************/
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitInputRelease> WaitInputReleaseTask;

	EStulFireSessionType ActiveSessionType = EStulFireSessionType::Single;
	bool bAuthorizationRegistered = false;
};
