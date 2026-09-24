#include "AbilitySystem/StulWeaponGameplayAbility.h"

#include "AbilitySystem/StulWeaponAttributeSet.h"
#include "Components/StulWeaponManagerComponent.h"
#include "GameFramework/Actor.h"
#include "GameplayAbilitySpec.h"
#include "Interfaces/StulWeaponOwnerInterface.h"
#include "Weapons/StulWeapon.h"

UStulWeaponGameplayAbility::UStulWeaponGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UStulWeaponGameplayAbility::TryActivateAbilityOnSpawn(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) const
{
	if (!ActorInfo || ActivationPolicy != EStulWeaponAbilityActivationPolicy::OnSpawn || Spec.IsActive()) return;

	UAbilitySystemComponent* AbilitySystem = ActorInfo->AbilitySystemComponent.Get();
	const AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	if (!AbilitySystem || !AvatarActor || AvatarActor->GetTearOff() || AvatarActor->IsActorBeingDestroyed() || AvatarActor->GetLifeSpan() > 0.0f) return;

	const bool bLocalExecution = NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::LocalOnly || NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	const bool bServerExecution = NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::ServerOnly || NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	if ((ActorInfo->IsLocallyControlled() && bLocalExecution) || (ActorInfo->IsNetAuthority() && bServerExecution)) AbilitySystem->TryActivateAbility(Spec.Handle);
}

void UStulWeaponGameplayAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);
	TryActivateAbilityOnSpawn(ActorInfo, Spec);
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

	return nullptr;
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
	if (!IsValid(Weapon) || !Weapon->IsInitialized()) return false;

	if (ActivationPolicy != EStulWeaponAbilityActivationPolicy::OnSpawn)
	{
		const AActor* WeaponOwner = Weapon->GetOwner();
		const UStulWeaponManagerComponent* WeaponManager = WeaponOwner ? WeaponOwner->FindComponentByClass<UStulWeaponManagerComponent>() : nullptr;
		if (!WeaponManager || WeaponManager->GetEquippedWeapon() != Weapon) return false;
	}

	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}
