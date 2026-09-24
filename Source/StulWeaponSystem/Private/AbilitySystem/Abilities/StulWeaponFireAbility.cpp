#include "AbilitySystem/Abilities/StulWeaponFireAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Effects/StulWeaponAmmoCostEffect.h"
#include "AbilitySystem/StulWeaponAttributeSet.h"
#include "Components/StulWeaponFireComponent.h"
#include "Projectiles/StulWeaponProjectile.h"
#include "StulWeaponGameplayTags.h"
#include "StulWeaponSystem.h"
#include "Weapons/StulWeapon.h"
#include "Weapons/StulWeaponDefinition.h"

UStulWeaponFireAbility::UStulWeaponFireAbility()
{
	ActivationPolicy = EStulWeaponAbilityActivationPolicy::OnInputTriggered;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	CostGameplayEffectClass = UStulWeaponAmmoCostEffect::StaticClass();
	SetAssetTags(FGameplayTagContainer(StulWeaponGameplayTags::Ability_Fire));
	ActivationOwnedTags.AddTag(StulWeaponGameplayTags::State_Firing);
	ActivationBlockedTags.AddTag(StulWeaponGameplayTags::State_Firing);
	ActivationBlockedTags.AddTag(StulWeaponGameplayTags::State_ChangingFireMode);
	CancelAbilitiesWithTag.AddTag(StulWeaponGameplayTags::Ability_Reload);
}

/*********************************************************************************************/
/************************************ Gameplay Ability ***************************************/
/*********************************************************************************************/
void UStulWeaponFireAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AStulWeapon* Weapon = GetStulWeapon();
	if (!Weapon || !ResolveFireSessionType(*Weapon, ActiveSessionType) || !StartFireSession(ActiveSessionType))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ActiveSessionType == EStulFireSessionType::Automatic)
	{
		WaitInputReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, false);
		if (!WaitInputReleaseTask)
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
		WaitInputReleaseTask->OnRelease.AddDynamic(this, &ThisClass::HandleInputReleased);
		WaitInputReleaseTask->ReadyForActivation();
	}
}

void UStulWeaponFireAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const bool bReplicateEndAbility, const bool bWasCancelled)
{
	if (WaitInputReleaseTask)
	{
		WaitInputReleaseTask->EndTask();
		WaitInputReleaseTask = nullptr;
	}

	if (bAuthorizationRegistered)
	{
		if (AStulWeapon* Weapon = GetStulWeapon())
		{
			if (UStulWeaponFireComponent* FireComponent = Weapon->GetFireComponent()) FireComponent->UnregisterAbilityAuthorization(this, bWasCancelled);
		}
		bAuthorizationRegistered = false;
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UStulWeaponFireAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) return false;

	const AStulWeapon* Weapon = ActorInfo ? Cast<AStulWeapon>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UStulWeaponDefinition* Definition = Weapon ? Weapon->GetWeaponDefinition() : nullptr;
	EStulFireSessionType SessionType;
	if (!Weapon || !Definition || !Weapon->CanPredictWeaponFire() || !ResolveFireSessionType(*Weapon, SessionType)) return false;
	if (const UStulWeaponFireComponent* FireComponent = Weapon->GetFireComponent())
	{
		const UStulWeaponAttributeSet* Attributes = Weapon->GetWeaponAttributeSet();
		const int32 ShotCost = FMath::Max(0, FMath::RoundToInt(Attributes ? Attributes->GetFireCost() : 0.0f));
		if (FireComponent->ResolveExecutionRole() == EStulFireExecutionRole::PredictiveProducer && FireComponent->GetDisplayedAmmo() < ShotCost) return false;
	}
	if (Weapon->IsReloading() && Definition->ReloadType != EStulWeaponReloadType::Custom) return false;
	if (Definition->ShotType == EStulWeaponShotType::Hitscan) return Definition->MaxRange > 0.0f;
	if (Definition->ShotType == EStulWeaponShotType::Projectile) return Definition->ProjectileClass.Get() && Definition->BaseProjectileSpeed > 0.0f && Definition->AimTraceDistance > 0.0f && Definition->ProjectileMaxLifeSeconds > 0.0f;
	return false;
}

bool UStulWeaponFireAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	const AStulWeapon* Weapon = ActorInfo ? Cast<AStulWeapon>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UStulWeaponAttributeSet* Attributes = Weapon ? Weapon->GetWeaponAttributeSet() : nullptr;
	if (Attributes && Attributes->GetCurrentAmmo() + KINDA_SMALL_NUMBER >= FMath::Max(0.0f, Attributes->GetFireCost())) return true;

	const FGameplayTag& CostTag = UAbilitySystemGlobals::Get().ActivateFailCostTag;
	if (OptionalRelevantTags && CostTag.IsValid()) OptionalRelevantTags->AddTag(CostTag);
	return false;
}

void UStulWeaponFireAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	const UStulWeaponAttributeSet* Attributes = GetStulWeaponAttributeSet();
	if (!Attributes) return;

	FGameplayEffectSpecHandle CostSpec = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, CostGameplayEffectClass, GetAbilityLevel(Handle, ActorInfo));
	if (!CostSpec.IsValid())
	{
		UE_LOG(LogStulWeaponSystem, Error, TEXT("Fire ability '%s' could not create its ammo cost Gameplay Effect spec."), *GetNameSafe(this));
		return;
	}

	CostSpec.Data->SetSetByCallerMagnitude(StulWeaponGameplayTags::SetByCaller_Ammo, -FMath::Max(0.0f, Attributes->GetFireCost()));
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, CostSpec);
}

/*********************************************************************************************/
/************************************** Fire Session *****************************************/
/*********************************************************************************************/
bool UStulWeaponFireAbility::ResolveFireSessionType(const AStulWeapon& Weapon, EStulFireSessionType& OutSessionType) const
{
	const FGameplayTag FireMode = Weapon.GetCurrentFireMode();
	if (FireMode == StulWeaponGameplayTags::FireMode_Single) OutSessionType = EStulFireSessionType::Single;
	else if (FireMode == StulWeaponGameplayTags::FireMode_Burst) OutSessionType = EStulFireSessionType::Burst;
	else if (FireMode == StulWeaponGameplayTags::FireMode_Automatic) OutSessionType = EStulFireSessionType::Automatic;
	else return false;
	return true;
}

bool UStulWeaponFireAbility::StartFireSession(const EStulFireSessionType SessionType)
{
	AStulWeapon* Weapon = GetStulWeapon();
	UStulWeaponFireComponent* FireComponent = Weapon ? Weapon->GetFireComponent() : nullptr;
	if (!FireComponent) return false;

	bAuthorizationRegistered = FireComponent->RegisterAbilityAuthorization(
		SessionType,
		CurrentSpecHandle,
		this,
		FStulAuthorizeWeaponShot::CreateUObject(this, &ThisClass::AuthorizeShot),
		FStulFireSessionEnded::CreateUObject(this, &ThisClass::HandleFireSessionEnded));
	if (!bAuthorizationRegistered) return false;

	if (SessionType == EStulFireSessionType::Single) return FireComponent->StartSingle();
	if (SessionType == EStulFireSessionType::Burst) return FireComponent->StartBurst();
	return FireComponent->StartAutomatic();
}

bool UStulWeaponFireAbility::AuthorizeShot(const FStulShotId& ShotId)
{
	AStulWeapon* Weapon = GetStulWeapon();
	UStulWeaponFireComponent* FireComponent = Weapon ? Weapon->GetFireComponent() : nullptr;
	const UStulWeaponAttributeSet* Attributes = GetStulWeaponAttributeSet();
	if (!Weapon || !FireComponent || !Attributes || !ShotId.IsValid()) return false;

	const EStulFireExecutionRole ExecutionRole = FireComponent->ResolveExecutionRole();
	if (ExecutionRole == EStulFireExecutionRole::PredictiveProducer && ActiveSessionType != EStulFireSessionType::Single)
	{
		return FireComponent->GetDisplayedAmmo() >= FMath::Max(0, FMath::RoundToInt(Attributes->GetFireCost()));
	}
	return CommitAbilityCost(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo);
}

void UStulWeaponFireAbility::HandleFireSessionEnded(const bool bWasCancelled)
{
	if (!IsActive()) return;
	const AStulWeapon* Weapon = GetStulWeapon();
	const UStulWeaponFireComponent* FireComponent = Weapon ? Weapon->GetFireComponent() : nullptr;
	const bool bReplicateEndAbility = !FireComponent || FireComponent->ResolveExecutionRole() != EStulFireExecutionRole::PredictiveProducer;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UStulWeaponFireAbility::HandleInputReleased(const float TimeHeld)
{
	if (AStulWeapon* Weapon = GetStulWeapon())
	{
		if (UStulWeaponFireComponent* FireComponent = Weapon->GetFireComponent()) FireComponent->RequestStopAutomatic();
	}
}
