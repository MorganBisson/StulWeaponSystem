#include "AbilitySystem/StulWeaponGameplayAbility.h"

#include "AbilitySystem/StulWeaponAttributeSet.h"
#include "Interfaces/StulWeaponOwnerInterface.h"
#include "Weapons/StulWeapon.h"

UStulWeaponGameplayAbility::UStulWeaponGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

AStulWeapon* UStulWeaponGameplayAbility::GetStulWeapon() const
{
	if (const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo())
	{
		if (AStulWeapon* Weapon = Cast<AStulWeapon>(ActorInfo->AvatarActor.Get()))
		{
			return Weapon;
		}
	}

	return Cast<AStulWeapon>(GetCurrentSourceObject());
}

UStulWeaponAttributeSet* UStulWeaponGameplayAbility::GetStulWeaponAttributeSet() const
{
	const AStulWeapon* Weapon = GetStulWeapon();
	return Weapon ? Weapon->GetWeaponAttributeSet() : nullptr;
}

AActor* UStulWeaponGameplayAbility::GetWeaponOwnerActor() const
{
	const AStulWeapon* Weapon = GetStulWeapon();
	return Weapon ? Weapon->GetOwner() : nullptr;
}

bool UStulWeaponGameplayAbility::GetWeaponViewData(FStulWeaponViewData& OutViewData) const
{
	OutViewData = FStulWeaponViewData();

	AActor* OwnerActor = GetWeaponOwnerActor();
	if (!IsValid(OwnerActor))
	{
		return false;
	}

	if (OwnerActor->Implements<UStulWeaponOwnerInterface>())
	{
		return IStulWeaponOwnerInterface::Execute_GetWeaponViewData(OwnerActor, OutViewData);
	}

	OwnerActor->GetActorEyesViewPoint(OutViewData.ViewLocation, OutViewData.ViewRotation);
	return true;
}

bool UStulWeaponGameplayAbility::GetWeaponMovementData(FStulWeaponMovementData& OutMovementData) const
{
	OutMovementData = FStulWeaponMovementData();

	AActor* OwnerActor = GetWeaponOwnerActor();
	return IsValid(OwnerActor)
		&& OwnerActor->Implements<UStulWeaponOwnerInterface>()
		&& IStulWeaponOwnerInterface::Execute_GetWeaponMovementData(OwnerActor, OutMovementData);
}

bool UStulWeaponGameplayAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	const AStulWeapon* Weapon = ActorInfo ? Cast<AStulWeapon>(ActorInfo->AvatarActor.Get()) : nullptr;
	return IsValid(Weapon)
		&& Weapon->IsInitialized()
		&& Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}
