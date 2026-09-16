#include "AbilitySystem/Abilities/StulWeaponFireAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Effects/StulWeaponAmmoCostEffect.h"
#include "AbilitySystem/StulWeaponAttributeSet.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameplayPrediction.h"
#include "Projectiles/StulWeaponProjectile.h"
#include "StulWeaponGameplayTags.h"
#include "StulWeaponSystem.h"
#include "TimerManager.h"
#include "Weapons/Shooting/StulWeaponShootingLibrary.h"
#include "Weapons/Shooting/StulWeaponShootingTypes.h"
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
	ActivationBlockedTags.AddTag(StulWeaponGameplayTags::State_Reloading);
	ActivationBlockedTags.AddTag(StulWeaponGameplayTags::State_ChangingFireMode);
}

/*********************************************************************************************/
/************************************ Gameplay Ability ***************************************/
/*********************************************************************************************/
void UStulWeaponFireAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AStulWeapon* Weapon = GetStulWeapon();
	if (!Weapon || !Weapon->GetWeaponDefinition())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveFireMode = Weapon->GetCurrentFireMode();
	bInputReleased = false;
	NextLocalShotSequence = 0;
	NextExpectedServerShotSequence = 0;
	ShotsRemainingInBurst = 0;
	AcceptedShotsInServerBurst = 0;
	bFiringStarted = false;

	if (ActiveFireMode == StulWeaponGameplayTags::FireMode_Burst || ActiveFireMode == StulWeaponGameplayTags::FireMode_Automatic)
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

	bFiringStarted = true;
	Weapon->NotifyFiringStarted();
	SendWeaponEvent(StulWeaponGameplayTags::Event_Fire_Start);
	BindServerTargetData();
	if (!IsActive())
	{
		return;
	}

	if (ActorInfo && ActorInfo->IsLocallyControlled())
	{
		BeginLocalFiring();
	}
}

void UStulWeaponFireAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const bool bReplicateEndAbility, const bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LocalShotTimer);
	}

	UnbindServerTargetData();
	if (WaitInputReleaseTask)
	{
		WaitInputReleaseTask->EndTask();
		WaitInputReleaseTask = nullptr;
	}

	if (bFiringStarted)
	{
		if (AStulWeapon* Weapon = GetStulWeapon())
		{
			Weapon->NotifyFiringEnded();
		}
		SendWeaponEvent(StulWeaponGameplayTags::Event_Fire_End);
	}

	bFiringStarted = false;
	ActiveFireMode = FGameplayTag();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UStulWeaponFireAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const AStulWeapon* Weapon = ActorInfo ? Cast<AStulWeapon>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UStulWeaponDefinition* Definition = Weapon ? Weapon->GetWeaponDefinition() : nullptr;
	if (!Definition)
	{
		return false;
	}
	if (Definition->ShotType == EStulWeaponShotType::Hitscan)
	{
		if (Definition->MaxRange <= 0.0f)
		{
			return false;
		}
	}
	else if (Definition->ShotType == EStulWeaponShotType::Projectile)
	{
		if (!Definition->ProjectileClass.Get() || Definition->BaseProjectileSpeed <= 0.0f || Definition->AimTraceDistance <= 0.0f || Definition->ProjectileMaxLifeSeconds <= 0.0f)
		{
			return false;
		}
	}
	else
	{
		return false;
	}

	const UWorld* World = GetWorld();
	return !ActorInfo || !ActorInfo->IsLocallyControlled() || (World && World->GetTimeSeconds() + KINDA_SMALL_NUMBER >= EarliestNextLocalShotTime);
}

bool UStulWeaponFireAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	const AStulWeapon* Weapon = ActorInfo ? Cast<AStulWeapon>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UStulWeaponAttributeSet* Attributes = Weapon ? Weapon->GetWeaponAttributeSet() : nullptr;
	if (Attributes && Attributes->GetCurrentAmmo() + KINDA_SMALL_NUMBER >= FMath::Max(0.0f, Attributes->GetFireCost()))
	{
		return true;
	}

	const FGameplayTag& CostTag = UAbilitySystemGlobals::Get().ActivateFailCostTag;
	if (OptionalRelevantTags && CostTag.IsValid())
	{
		OptionalRelevantTags->AddTag(CostTag);
	}
	return false;
}

void UStulWeaponFireAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	const UStulWeaponAttributeSet* Attributes = GetStulWeaponAttributeSet();
	if (!Attributes)
	{
		return;
	}

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
/************************************** Local Firing *****************************************/
/*********************************************************************************************/
void UStulWeaponFireAbility::BeginLocalFiring()
{
	if (ActiveFireMode == StulWeaponGameplayTags::FireMode_Single)
	{
		FinishFiring(!SubmitLocalShot());
		return;
	}

	if (ActiveFireMode == StulWeaponGameplayTags::FireMode_Automatic)
	{
		if (!SubmitLocalShot())
		{
			FinishFiring(true);
			return;
		}
		ScheduleNextLocalShot(GetFireInterval());
		return;
	}

	if (ActiveFireMode == StulWeaponGameplayTags::FireMode_Burst)
	{
		ShotsRemainingInBurst = GetBurstSize();
		if (!SubmitLocalShot())
		{
			FinishFiring(true);
			return;
		}
		--ShotsRemainingInBurst;
		ScheduleNextLocalShot(ShotsRemainingInBurst > 0 ? GetFireInterval() : GetBurstInterval());
		return;
	}

	UE_LOG(LogStulWeaponSystem, Warning, TEXT("Fire ability '%s' rejected unsupported fire mode '%s'."), *GetNameSafe(this), *ActiveFireMode.ToString());
	FinishFiring(true);
}

bool UStulWeaponFireAbility::SubmitLocalShot()
{
	FStulWeaponViewData ViewData;
	if (!GetWeaponViewData(ViewData))
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Fire ability '%s' could not obtain weapon view data."), *GetNameSafe(this));
		return false;
	}

	const FVector ViewDirection = ViewData.ViewRotation.Vector().GetSafeNormal();
	if (ViewDirection.IsNearlyZero())
	{
		return false;
	}
	FGameplayAbilityTargetDataHandle TargetData;
	FGameplayAbilityTargetData_StulWeaponShot* ShotData = new FGameplayAbilityTargetData_StulWeaponShot();
	ShotData->ViewOrigin = ViewData.ViewLocation;
	ShotData->ViewDirection = ViewDirection;
	ShotData->ShotSequence = NextLocalShotSequence;
	TargetData.Add(ShotData);

	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	if (IsPredictingClient() && AbilitySystem)
	{
		FScopedPredictionWindow PredictionWindow(AbilitySystem, true);
		if (!CommitAbilityCost(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
		{
			return false;
		}
		AbilitySystem->CallServerSetReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey(), TargetData, FGameplayTag(), AbilitySystem->ScopedPredictionKey);
		ExecuteShot(ViewData.ViewLocation, ViewDirection, NextLocalShotSequence, false);
	}
	else
	{
		if (!CommitAbilityCost(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
		{
			return false;
		}
		const bool bAuthoritative = CurrentActorInfo && CurrentActorInfo->IsNetAuthority();
		ExecuteShot(ViewData.ViewLocation, ViewDirection, NextLocalShotSequence, bAuthoritative);
	}

	const float DelayAfterShot = GetDelayAfterCurrentLocalShot();
	if (const UWorld* World = GetWorld())
	{
		EarliestNextLocalShotTime = World->GetTimeSeconds() + DelayAfterShot;
	}
	++NextLocalShotSequence;
	return true;
}

void UStulWeaponFireAbility::ScheduleNextLocalShot(const float Delay)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(LocalShotTimer, this, &ThisClass::HandleLocalShotTimer, FMath::Max(Delay, KINDA_SMALL_NUMBER), false);
	}
}

void UStulWeaponFireAbility::HandleLocalShotTimer()
{
	if (ActiveFireMode == StulWeaponGameplayTags::FireMode_Automatic)
	{
		if (bInputReleased || !SubmitLocalShot())
		{
			FinishFiring(false);
			return;
		}
		ScheduleNextLocalShot(GetFireInterval());
		return;
	}

	if (ActiveFireMode != StulWeaponGameplayTags::FireMode_Burst)
	{
		FinishFiring(true);
		return;
	}

	if (ShotsRemainingInBurst <= 0)
	{
		if (bInputReleased)
		{
			FinishFiring(false);
			return;
		}
		ShotsRemainingInBurst = GetBurstSize();
	}

	if (!SubmitLocalShot())
	{
		FinishFiring(false);
		return;
	}

	--ShotsRemainingInBurst;
	if (ShotsRemainingInBurst <= 0 && bInputReleased)
	{
		FinishFiring(false);
		return;
	}
	const float NextDelay = ShotsRemainingInBurst > 0 ? GetFireInterval() : GetBurstInterval();
	ScheduleNextLocalShot(NextDelay);
}

void UStulWeaponFireAbility::FinishFiring(const bool bWasCancelled)
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}

float UStulWeaponFireAbility::GetDelayAfterCurrentLocalShot() const
{
	if (ActiveFireMode == StulWeaponGameplayTags::FireMode_Burst && ShotsRemainingInBurst <= 1)
	{
		return GetBurstInterval();
	}
	return GetFireInterval();
}

void UStulWeaponFireAbility::HandleInputReleased(const float TimeHeld)
{
	bInputReleased = true;
	if (ActiveFireMode == StulWeaponGameplayTags::FireMode_Automatic || (ActiveFireMode == StulWeaponGameplayTags::FireMode_Burst && ShotsRemainingInBurst <= 0 && AcceptedShotsInServerBurst == 0))
	{
		FinishFiring(false);
	}
}

/*********************************************************************************************/
/************************************** Target Data ******************************************/
/*********************************************************************************************/
void UStulWeaponFireAbility::BindServerTargetData()
{
	if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority() || CurrentActorInfo->IsLocallyControlled())
	{
		return;
	}

	if (UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo())
	{
		AbilitySystem->AbilityTargetDataSetDelegate(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey()).AddUObject(this, &ThisClass::HandleReplicatedTargetData);
		bServerTargetDataBound = true;
		AbilitySystem->CallReplicatedTargetDataDelegatesIfSet(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
	}
}

void UStulWeaponFireAbility::UnbindServerTargetData()
{
	if (!bServerTargetDataBound)
	{
		return;
	}

	if (UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo())
	{
		AbilitySystem->AbilityTargetDataSetDelegate(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey()).RemoveAll(this);
		AbilitySystem->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
	}
	bServerTargetDataBound = false;
}

void UStulWeaponFireAbility::HandleReplicatedTargetData(const FGameplayAbilityTargetDataHandle& TargetData, const FGameplayTag ActivationTag)
{
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	if (AbilitySystem)
	{
		AbilitySystem->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
	}

	FVector ViewOrigin;
	FVector ViewDirection;
	uint16 ShotSequence = 0;
	if (!ValidateTargetData(TargetData, ViewOrigin, ViewDirection, ShotSequence) || !ValidateAuthoritativeCadence(ShotSequence) || !CommitAbilityCost(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Fire ability '%s' rejected replicated shot %u from '%s'."), *GetNameSafe(this), ShotSequence, *GetNameSafe(GetWeaponOwnerActor()));
		FinishFiring(true);
		return;
	}

	ExecuteShot(ViewOrigin, ViewDirection, ShotSequence, true);
	if (ActiveFireMode == StulWeaponGameplayTags::FireMode_Burst && bInputReleased && AcceptedShotsInServerBurst == 0)
	{
		FinishFiring(false);
		return;
	}
	if (ActiveFireMode == StulWeaponGameplayTags::FireMode_Single)
	{
		FinishFiring(false);
	}
}

bool UStulWeaponFireAbility::ValidateTargetData(const FGameplayAbilityTargetDataHandle& TargetData, FVector& OutViewOrigin, FVector& OutViewDirection, uint16& OutShotSequence) const
{
	if (TargetData.Num() != 1)
	{
		return false;
	}

	const FGameplayAbilityTargetData* RawData = TargetData.Get(0);
	if (!RawData || RawData->GetScriptStruct() != FGameplayAbilityTargetData_StulWeaponShot::StaticStruct())
	{
		return false;
	}

	const FGameplayAbilityTargetData_StulWeaponShot* ShotData = static_cast<const FGameplayAbilityTargetData_StulWeaponShot*>(RawData);
	OutViewOrigin = ShotData->ViewOrigin;
	OutViewDirection = ShotData->ViewDirection.GetSafeNormal();
	OutShotSequence = ShotData->ShotSequence;
	if (OutViewOrigin.ContainsNaN() || OutViewDirection.ContainsNaN() || OutViewDirection.IsNearlyZero())
	{
		return false;
	}

	FStulWeaponViewData ServerViewData;
	if (!GetWeaponViewData(ServerViewData))
	{
		return false;
	}

	const bool bOriginValid = FVector::DistSquared(OutViewOrigin, ServerViewData.ViewLocation) <= FMath::Square(MaxClientViewOriginError);
	const float MinimumAimDot = FMath::Cos(FMath::DegreesToRadians(MaxClientAimErrorDegrees));
	const bool bDirectionValid = FVector::DotProduct(OutViewDirection, ServerViewData.ViewRotation.Vector().GetSafeNormal()) >= MinimumAimDot;
	return bOriginValid && bDirectionValid;
}

bool UStulWeaponFireAbility::ValidateAuthoritativeCadence(const uint16 ShotSequence)
{
	if (ShotSequence != NextExpectedServerShotSequence)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const double Now = World->GetTimeSeconds();
	if (Now + KINDA_SMALL_NUMBER < EarliestNextAuthoritativeShotTime)
	{
		return false;
	}

	++NextExpectedServerShotSequence;
	float RequiredDelay = GetFireInterval();
	if (ActiveFireMode == StulWeaponGameplayTags::FireMode_Burst)
	{
		++AcceptedShotsInServerBurst;
		if (AcceptedShotsInServerBurst >= GetBurstSize())
		{
			AcceptedShotsInServerBurst = 0;
			RequiredDelay = GetBurstInterval();
		}
	}
	EarliestNextAuthoritativeShotTime = Now + RequiredDelay * (1.0f - FMath::Clamp(FireIntervalTolerance, 0.0f, 1.0f));
	return true;
}

/*********************************************************************************************/
/************************************* Shot Execution ****************************************/
/*********************************************************************************************/
void UStulWeaponFireAbility::ExecuteShot(const FVector& ViewOrigin, const FVector& ViewDirection, const uint16 ShotSequence, const bool bAuthoritative)
{
	AStulWeapon* Weapon = GetStulWeapon();
	const UStulWeaponDefinition* Definition = Weapon ? Weapon->GetWeaponDefinition() : nullptr;
	const UStulWeaponAttributeSet* Attributes = GetStulWeaponAttributeSet();
	if (!Weapon || !Definition || !Attributes)
	{
		return;
	}

	TArray<AActor*> IgnoredActors;
	IgnoredActors.Reserve(2);
	IgnoredActors.Add(Weapon);
	if (AActor* OwnerActor = GetWeaponOwnerActor())
	{
		IgnoredActors.Add(OwnerActor);
	}

	const float AimDistance = Definition->ShotType == EStulWeaponShotType::Projectile ? Definition->AimTraceDistance : Definition->MaxRange;
	FVector AimTarget;
	FHitResult AimHit;
	UStulWeaponShootingLibrary::FindAimTarget(this, ViewOrigin, ViewDirection, AimDistance, Definition->AimTraceChannel, IgnoredActors, AimTarget, AimHit);
	const FTransform MuzzleTransform = Weapon->GetMuzzleTransform();
	const int32 ProjectileCount = FMath::Max(1, FMath::FloorToInt(Attributes->GetShotsPerFire()));
	const FStulWeaponShotPattern* ShotPattern = Definition->ShotPatterns.Find(ProjectileCount);

	for (int32 ProjectileIndex = 0; ProjectileIndex < ProjectileCount; ++ProjectileIndex)
	{
		const FVector2D PatternOffset = ShotPattern && ShotPattern->ShotPoints.IsValidIndex(ProjectileIndex) ? ShotPattern->ShotPoints[ProjectileIndex] : FVector2D::ZeroVector;
		const FVector MuzzleLocation = Definition->bUseMuzzleOffset ? UStulWeaponShootingLibrary::CalculateMuzzleOffset(MuzzleTransform, PatternOffset, Definition->MuzzleOffsetScale) : MuzzleTransform.GetLocation();

		FStulWeaponShotRequest ShotRequest;
		ShotRequest.ShotOrigin = ViewOrigin;
		ShotRequest.TargetLocation = AimTarget;
		ShotRequest.PatternOffset = PatternOffset;
		ShotRequest.PatternScale = Attributes->GetPatternScale();
		ShotRequest.SpreadHalfAngleDegrees = Attributes->GetMinSpread();
		ShotRequest.RandomSeed = MakeShotSeed(ShotSequence, ProjectileIndex);

		FStulWeaponShotResult ShotResult;
		ShotResult.TraceOrigin = ViewOrigin;
		ShotResult.MuzzleLocation = MuzzleLocation;
		ShotResult.Direction = UStulWeaponShootingLibrary::CalculateShotDirection(ShotRequest);
		ShotResult.ProjectileIndex = ProjectileIndex;
		ShotResult.bAuthoritative = bAuthoritative;
		ShotResult.EndLocation = ViewOrigin + ShotResult.Direction * AimDistance;

		if (Definition->ShotType == EStulWeaponShotType::Hitscan)
		{
			ExecuteHitscanShot(*Definition, ViewOrigin, IgnoredActors, ShotResult);
		}
		else if (Definition->ShotType == EStulWeaponShotType::Projectile && bAuthoritative && !SpawnAuthoritativeProjectile(*Definition, *Weapon, ViewOrigin, ShotSequence, ShotResult))
		{
			UE_LOG(LogStulWeaponSystem, Error, TEXT("Fire ability '%s' failed to spawn projectile %d for weapon '%s'."), *GetNameSafe(this), ProjectileIndex, *GetNameSafe(Weapon));
			continue;
		}

		DrawShotDebug(ViewOrigin, AimTarget, AimHit, ShotResult);
		NotifyShotExecuted(*Weapon, ShotResult);
		if (Definition->ShotType == EStulWeaponShotType::Hitscan && ShotResult.bBlockingHit)
		{
			Weapon->ExecuteImpactGameplayCue(ShotResult.HitResult, Attributes->GetDamage(), Weapon);
			if (bAuthoritative)
			{
				HandleAuthoritativeHitscanImpact(*Weapon, ShotResult);
			}
		}
	}
}

void UStulWeaponFireAbility::ExecuteHitscanShot(const UStulWeaponDefinition& Definition, const FVector& ViewOrigin, const TArray<AActor*>& IgnoredActors, FStulWeaponShotResult& ShotResult)
{
	ShotResult.bBlockingHit = UStulWeaponShootingLibrary::PerformHitscanTrace(this, ViewOrigin, ShotResult.Direction, Definition.MaxRange, Definition.HitscanTraceChannel, IgnoredActors, ShotResult.HitResult, ShotResult.EndLocation);
}

bool UStulWeaponFireAbility::SpawnAuthoritativeProjectile(const UStulWeaponDefinition& Definition, AStulWeapon& Weapon, const FVector& ViewOrigin, const uint16 ShotSequence, FStulWeaponShotResult& ShotResult) const
{
	if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority())
	{
		return false;
	}

	UWorld* World = GetWorld();
	const TSubclassOf<AStulWeaponProjectile> ProjectileClass = Definition.ProjectileClass.Get();
	if (!World || !ProjectileClass)
	{
		return false;
	}

	const FTransform SpawnTransform(ShotResult.Direction.Rotation(), ViewOrigin);
	AStulWeaponProjectile* Projectile = World->SpawnActorDeferred<AStulWeaponProjectile>(ProjectileClass, SpawnTransform, &Weapon, Cast<APawn>(GetWeaponOwnerActor()), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Projectile)
	{
		return false;
	}

	FStulWeaponProjectileInitData ProjectileInitData;
	ProjectileInitData.CosmeticOrigin = ShotResult.MuzzleLocation;
	ProjectileInitData.Direction = ShotResult.Direction;
	ProjectileInitData.Speed = Definition.BaseProjectileSpeed;
	ProjectileInitData.GravityScale = Definition.ProjectileGravityScale;
	ProjectileInitData.MaxLifeSeconds = Definition.ProjectileMaxLifeSeconds;
	ProjectileInitData.ShotSequence = ShotSequence;
	ProjectileInitData.ProjectileIndex = ShotResult.ProjectileIndex;

	const UStulWeaponAttributeSet* Attributes = GetStulWeaponAttributeSet();
	if (!Projectile->InitializeProjectile(ProjectileInitData, Attributes ? Attributes->GetDamage() : 0.0f))
	{
		Projectile->Destroy();
		return false;
	}

	Projectile->FinishSpawning(SpawnTransform);
	return true;
}

void UStulWeaponFireAbility::HandleAuthoritativeHitscanImpact(AStulWeapon& Weapon, const FStulWeaponShotResult& ShotResult)
{
	K2_OnAuthoritativeHit(ShotResult);
	AActor* HitActor = ShotResult.HitResult.GetActor();
	if (!HitActor)
	{
		return;
	}

	if (UAbilitySystemComponent* HitAbilitySystem = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor))
	{
		FGameplayEventData HitEventData;
		HitEventData.EventTag = StulWeaponGameplayTags::Event_Hit;
		HitEventData.Instigator = GetWeaponOwnerActor();
		HitEventData.Target = HitActor;
		HitEventData.OptionalObject = &Weapon;
		HitEventData.EventMagnitude = GetStulWeaponAttributeSet() ? GetStulWeaponAttributeSet()->GetDamage() : 0.0f;
		HitEventData.TargetData.Add(new FGameplayAbilityTargetData_SingleTargetHit(ShotResult.HitResult));
		HitAbilitySystem->HandleGameplayEvent(StulWeaponGameplayTags::Event_Hit, &HitEventData);
	}
}

void UStulWeaponFireAbility::NotifyShotExecuted(AStulWeapon& Weapon, const FStulWeaponShotResult& ShotResult)
{
	Weapon.NotifyShotExecuted(ShotResult);
	if (ShotResult.ProjectileIndex == 0)
	{
		Weapon.ExecuteFireGameplayCue(ShotResult);
	}
	if (const UStulWeaponDefinition* Definition = Weapon.GetWeaponDefinition(); Definition && Definition->ShotType == EStulWeaponShotType::Hitscan)
	{
		Weapon.ExecuteTracerGameplayCue(ShotResult);
	}
	K2_OnShotExecuted(ShotResult);
	SendWeaponEvent(StulWeaponGameplayTags::Event_Fire_Shot, &ShotResult);
}

void UStulWeaponFireAbility::DrawShotDebug(const FVector& ViewOrigin, const FVector& AimTarget, const FHitResult& AimHit, const FStulWeaponShotResult& ShotResult) const
{
#if ENABLE_DRAW_DEBUG
	if (!bDrawDebugTraces)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Duration = FMath::Max(0.0f, DebugDrawDuration);
	const float Radius = FMath::Max(0.0f, DebugImpactRadius);
	const float Thickness = FMath::Max(0.0f, DebugLineThickness);
	if (ShotResult.ProjectileIndex == 0)
	{
		DrawDebugLine(World, ViewOrigin, AimTarget, FColor::Cyan, false, Duration, 0, Thickness);
		DrawDebugSphere(World, ViewOrigin, Radius, 8, FColor::Blue, false, Duration, 0, Thickness);
		if (AimHit.bBlockingHit)
		{
			DrawDebugSphere(World, AimHit.ImpactPoint, Radius, 12, FColor::Yellow, false, Duration, 0, Thickness);
		}
	}

	const FColor GameplayTraceColor = ShotResult.bAuthoritative ? FColor::Orange : FColor::Green;
	DrawDebugLine(World, ShotResult.TraceOrigin, ShotResult.EndLocation, GameplayTraceColor, false, Duration, 0, Thickness);
	DrawDebugLine(World, ShotResult.MuzzleLocation, ShotResult.EndLocation, FColor::Purple, false, Duration, 0, Thickness);
	DrawDebugSphere(World, ShotResult.MuzzleLocation, Radius, 8, FColor::Magenta, false, Duration, 0, Thickness);
	if (ShotResult.bBlockingHit)
	{
		DrawDebugSphere(World, ShotResult.HitResult.ImpactPoint, Radius, 12, FColor::Red, false, Duration, 0, Thickness);
	}
#endif
}

void UStulWeaponFireAbility::SendWeaponEvent(const FGameplayTag EventTag, const FStulWeaponShotResult* ShotResult) const
{
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	AStulWeapon* Weapon = GetStulWeapon();
	if (!AbilitySystem || !Weapon)
	{
		return;
	}

	FGameplayEventData EventData;
	EventData.EventTag = EventTag;
	EventData.Instigator = GetWeaponOwnerActor();
	EventData.OptionalObject = Weapon;
	if (ShotResult)
	{
		EventData.Target = ShotResult->HitResult.GetActor();
		EventData.EventMagnitude = GetStulWeaponAttributeSet() ? GetStulWeaponAttributeSet()->GetDamage() : 0.0f;
		EventData.TargetData.Add(new FGameplayAbilityTargetData_SingleTargetHit(ShotResult->HitResult));
	}
	AbilitySystem->HandleGameplayEvent(EventTag, &EventData);
}

int32 UStulWeaponFireAbility::MakeShotSeed(const uint16 ShotSequence, const int32 ProjectileIndex) const
{
	const int32 PredictionSeed = static_cast<int32>(CurrentActivationInfo.GetActivationPredictionKey().Current);
	return static_cast<int32>(HashCombineFast(GetTypeHash(PredictionSeed), HashCombineFast(GetTypeHash(ShotSequence), GetTypeHash(ProjectileIndex))));
}

float UStulWeaponFireAbility::GetFireInterval() const
{
	const UStulWeaponAttributeSet* Attributes = GetStulWeaponAttributeSet();
	return FMath::Max(Attributes ? Attributes->GetFireInterval() : 0.0f, KINDA_SMALL_NUMBER);
}

float UStulWeaponFireAbility::GetBurstInterval() const
{
	const UStulWeaponAttributeSet* Attributes = GetStulWeaponAttributeSet();
	return FMath::Max(Attributes ? Attributes->GetBurstInterval() : 0.0f, GetFireInterval());
}

int32 UStulWeaponFireAbility::GetBurstSize() const
{
	const UStulWeaponAttributeSet* Attributes = GetStulWeaponAttributeSet();
	return FMath::Max(1, FMath::FloorToInt(Attributes ? Attributes->GetBurstShotCount() : 1.0f));
}
