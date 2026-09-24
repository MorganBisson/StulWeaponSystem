#include "Weapons/StulWeapon.h"

#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/StulWeaponAbilitySystemComponent.h"
#include "AbilitySystem/StulWeaponAttributeSet.h"
#include "AbilitySystem/StulWeaponGameplayAbility.h"
#include "Components/StulWeaponFireComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffectTypes.h"
#include "Net/UnrealNetwork.h"
#include "Projectiles/StulWeaponProjectile.h"
#include "StulWeaponGameplayTags.h"
#include "StulWeaponSystem.h"
#include "Weapons/Ammunition/StulAmmoDefinition.h"
#include "Weapons/StulWeaponDefinition.h"

/*********************************************************************************************/
/****************************************** Actor ********************************************/
/*********************************************************************************************/

AStulWeapon::AStulWeapon()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	SetRootComponent(WeaponMesh);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	AbilitySystemComponent = CreateDefaultSubobject<UStulWeaponAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	WeaponAttributeSet = CreateDefaultSubobject<UStulWeaponAttributeSet>(TEXT("WeaponAttributeSet"));
	AbilitySystemComponent->AddAttributeSetSubobject(WeaponAttributeSet.Get());

	FireComponent = CreateDefaultSubobject<UStulWeaponFireComponent>(TEXT("FireComponent"));
}

UAbilitySystemComponent* AStulWeapon::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UStulAmmoDefinition* AStulWeapon::GetCurrentAmmoDefinition() const
{
	return WeaponDefinition ? WeaponDefinition->DefaultAmmo.Get() : nullptr;
}

void AStulWeapon::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelPendingLoads();
	ClearPredictedProjectiles();
	ClearGrantedAbilities();
	UninitializeAbilitySystem();

	Super::EndPlay(EndPlayReason);
}

void AStulWeapon::HandleGameplayCue(UObject* Self, const FGameplayTag GameplayCueTag, const EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters)
{
	const bool bStateActivated = EventType == EGameplayCueEvent::OnActive || EventType == EGameplayCueEvent::WhileActive;
	if (GameplayCueTag == StulWeaponGameplayTags::GameplayCue_Aim || GameplayCueTag == StulWeaponGameplayTags::GameplayCue_Reload || GameplayCueTag == StulWeaponGameplayTags::GameplayCue_ChangeFireMode)
	{
		if (bStateActivated || EventType == EGameplayCueEvent::Removed) SetPresentationCueActive(GameplayCueTag, bStateActivated);
		return;
	}

	if (EventType == EGameplayCueEvent::Executed)
	{
		if (GameplayCueTag == StulWeaponGameplayTags::GameplayCue_ReloadCommit)
		{
			OnReloadCommit.Broadcast(this, FMath::Max(0, FMath::RoundToInt(Parameters.RawMagnitude)));
			return;
		}
		if (GameplayCueTag == StulWeaponGameplayTags::GameplayCue_ReloadCompleted)
		{
			OnReloadCompleted.Broadcast(this);
			return;
		}
		if (GameplayCueTag == StulWeaponGameplayTags::GameplayCue_ReloadCancelled)
		{
			OnReloadCancelled.Broadcast(this);
			return;
		}
	}

	IGameplayCueInterface::HandleGameplayCue(Self, GameplayCueTag, EventType, Parameters);
}

void AStulWeapon::SetPresentationCueActive(const FGameplayTag GameplayCueTag, const bool bActive)
{
	const bool bWasActive = ActivePresentationCues.HasTagExact(GameplayCueTag);
	if (bWasActive == bActive) return;

	if (bActive) ActivePresentationCues.AddTag(GameplayCueTag);
	else ActivePresentationCues.RemoveTag(GameplayCueTag);

	if (GameplayCueTag == StulWeaponGameplayTags::GameplayCue_Aim)
	{
		SetAimPresentationActive(bActive);
	}
	else if (GameplayCueTag == StulWeaponGameplayTags::GameplayCue_Reload)
	{
		if (bActive) OnReloadStarted.Broadcast(this);
	}
	else if (GameplayCueTag == StulWeaponGameplayTags::GameplayCue_ChangeFireMode)
	{
		OnFireModeChangeStateChanged.Broadcast(this, bActive);
	}
}

bool AStulWeapon::HasPresentationOrGameplayState(const FGameplayTag GameplayStateTag, const FGameplayTag PresentationCueTag) const
{
	return ActivePresentationCues.HasTagExact(PresentationCueTag) || (AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(GameplayStateTag));
}

void AStulWeapon::SetOwner(AActor* NewOwner)
{
	if (GetOwner() == NewOwner) return;

	UninitializeAbilitySystem();
	Super::SetOwner(NewOwner);
	if (bInitializationStarted && IsValid(NewOwner)) InitializeAbilityActorInfo();
}

void AStulWeapon::OnRep_Owner()
{
	Super::OnRep_Owner();
	AbilitySystemComponent->ClearAbilityInput();

	if (IsValid(GetOwner()))
	{
		InitializeAbilityActorInfo();
	}
	else
	{
		UninitializeAbilitySystem();
		bInitialized = false;
	}
}

void AStulWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AStulWeapon, WeaponDefinitionId);
	DOREPLIFETIME_CONDITION_NOTIFY(AStulWeapon, CurrentFireModeTag, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(AStulWeapon, WeaponBallisticSeed, COND_OwnerOnly, REPNOTIFY_Always);
}

/*********************************************************************************************/
/************************************* Initialization ****************************************/
/*********************************************************************************************/

bool AStulWeapon::InitializeFromDefinition(UStulWeaponDefinition* InDefinition)
{
	if (!BeginInitializationRequest())
	{
		return false;
	}

	FStulWeaponInstanceData NewInstanceData;
	if (!IsValid(InDefinition))
	{
		FailInitialization(NSLOCTEXT("StulWeaponSystem", "InvalidWeaponDefinition", "The weapon definition is invalid."));
		return false;
	}

	NewInstanceData.WeaponDefinitionId = InDefinition->GetPrimaryAssetId();
	return InitializeFromResolvedDefinition(InDefinition, NewInstanceData);
}

bool AStulWeapon::BeginInitializationRequest()
{
	if (!HasAuthority())
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Initialization rejected for weapon '%s': initialization must run on the server."), *GetNameSafe(this));
		return false;
	}

	if (bInitializationStarted)
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Initialization rejected for weapon '%s': a weapon actor can only be initialized once."), *GetNameSafe(this));
		return false;
	}

	if (!AbilitySystemComponent)
	{
		FailInitialization(NSLOCTEXT(
			"StulWeaponSystem",
			"MissingAbilitySystemComponent",
			"The weapon Ability System Component is missing."));
		return false;
	}

	if (!IsValid(GetOwner()))
	{
		FailInitialization(NSLOCTEXT(
			"StulWeaponSystem",
			"MissingWeaponOwner",
			"The weapon must have a valid Owner before initialization."));
		return false;
	}

	bInitializationStarted = true;
	WeaponBallisticSeed = HashCombineFast(GetTypeHash(GetUniqueID()), GetTypeHash(static_cast<uint64>(FPlatformTime::Cycles64())));
	if (WeaponBallisticSeed == 0) WeaponBallisticSeed = 1;
	InitializeAbilityActorInfo();
	return true;
}

bool AStulWeapon::InitializeFromResolvedDefinition(UStulWeaponDefinition* InDefinition, const FStulWeaponInstanceData& InstanceData)
{
	if (!IsValid(InDefinition))
	{
		FailInitialization(NSLOCTEXT("StulWeaponSystem", "InvalidWeaponDefinition", "The weapon definition is invalid."));
		return false;
	}

	bool bHasOnlySupportedFireModes = true;
	for (const FGameplayTag FireMode : InDefinition->AvailableFireModes)
	{
		bHasOnlySupportedFireModes &= StulWeaponGameplayTags::IsSupportedFireMode(FireMode);
	}
	if (InDefinition->AvailableFireModes.IsEmpty() || !bHasOnlySupportedFireModes || !StulWeaponGameplayTags::IsSupportedFireMode(InDefinition->DefaultFireModeTag) || !InDefinition->AvailableFireModes.HasTagExact(InDefinition->DefaultFireModeTag))
	{
		FailInitialization(NSLOCTEXT(
			"StulWeaponSystem",
			"InvalidWeaponFireModes",
			"The weapon definition does not contain a valid default fire mode."));
		return false;
	}

	WeaponDefinition = InDefinition;
	WeaponDefinitionId = InDefinition->GetPrimaryAssetId();
	RuntimeInstanceData = InstanceData;
	RuntimeInstanceData.WeaponDefinitionId = WeaponDefinitionId;

	ApplyDefinitionAttributes(RuntimeInstanceData);

	const FGameplayTag RequestedFireMode = RuntimeInstanceData.CurrentFireModeTag;
	CurrentFireModeTag = WeaponDefinition->AvailableFireModes.HasTagExact(RequestedFireMode)
		? RequestedFireMode
		: WeaponDefinition->DefaultFireModeTag;
	AuthoritativeFireModeTag = CurrentFireModeTag;
	RuntimeInstanceData.CurrentFireModeTag = CurrentFireModeTag;

	GrantDefinitionAbilities();
	const bool bLoadStarted = BeginRuntimeAssetLoad();
	ForceNetUpdate();
	return bLoadStarted;
}

bool AStulWeapon::InitializeFromInstanceData(const FStulWeaponInstanceData& InstanceData)
{
	if (!BeginInitializationRequest())
	{
		return false;
	}

	if (!InstanceData.IsValid())
	{
		FailInitialization(NSLOCTEXT("StulWeaponSystem", "InvalidWeaponInstanceData", "The weapon instance data is invalid."));
		return false;
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	if (UStulWeaponDefinition* LoadedDefinition = Cast<UStulWeaponDefinition>(AssetManager.GetPrimaryAssetObject(InstanceData.WeaponDefinitionId)))
	{
		return InitializeFromResolvedDefinition(LoadedDefinition, InstanceData);
	}

	RuntimeInstanceData = InstanceData;
	bInitialized = false;
	DefinitionLoadHandle = AssetManager.LoadPrimaryAsset(
		InstanceData.WeaponDefinitionId,
		TArray<FName>(),
		FStreamableDelegate::CreateUObject(this, &ThisClass::FinishDefinitionLoad, InstanceData));

	if (!DefinitionLoadHandle.IsValid())
	{
		FailInitialization(NSLOCTEXT("StulWeaponSystem", "WeaponDefinitionLoadNotStarted", "The weapon definition could not be loaded."));
		return false;
	}

	return true;
}

void AStulWeapon::FinishDefinitionLoad(FStulWeaponInstanceData InstanceData)
{
	DefinitionLoadHandle.Reset();

	UStulWeaponDefinition* LoadedDefinition = Cast<UStulWeaponDefinition>(UAssetManager::Get().GetPrimaryAssetObject(InstanceData.WeaponDefinitionId));
	if (!IsValid(LoadedDefinition))
	{
		FailInitialization(FText::Format(
			NSLOCTEXT("StulWeaponSystem", "WeaponDefinitionLoadFailed", "The weapon definition '{0}' failed to load."),
			FText::FromString(InstanceData.WeaponDefinitionId.ToString())));
		return;
	}

	InitializeFromResolvedDefinition(LoadedDefinition, InstanceData);
}

void AStulWeapon::FinishReplicatedDefinitionLoad(const FPrimaryAssetId RequestedDefinitionId)
{
	if (RequestedDefinitionId != WeaponDefinitionId)
	{
		UE_LOG(LogStulWeaponSystem, Verbose, TEXT("Weapon '%s' ignored the stale replicated definition load '%s'; current definition is '%s'."), *GetNameSafe(this), *RequestedDefinitionId.ToString(), *WeaponDefinitionId.ToString());
		return;
	}
	DefinitionLoadHandle.Reset();

	UStulWeaponDefinition* LoadedDefinition = Cast<UStulWeaponDefinition>(
		UAssetManager::Get().GetPrimaryAssetObject(RequestedDefinitionId));
	if (!IsValid(LoadedDefinition))
	{
		FailInitialization(FText::Format(
			NSLOCTEXT("StulWeaponSystem", "ReplicatedWeaponDefinitionLoadFailed", "The replicated weapon definition '{0}' failed to load."),
			FText::FromString(RequestedDefinitionId.ToString())));
		return;
	}

	WeaponDefinition = LoadedDefinition;
	RuntimeInstanceData.WeaponDefinitionId = RequestedDefinitionId;
	BeginRuntimeAssetLoad();
}

void AStulWeapon::InitializeAbilityActorInfo()
{
	if (!AbilitySystemComponent)
	{
		UE_LOG(LogStulWeaponSystem, Error, TEXT("Weapon '%s' cannot initialize GAS actor info because its AbilitySystemComponent is missing."), *GetNameSafe(this));
		return;
	}

	AActor* AbilityOwner = GetOwner();
	if (!IsValid(AbilityOwner))
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Weapon '%s' cannot initialize GAS actor info without a valid Owner."), *GetNameSafe(this));
		return;
	}

	if (bAbilityActorInfoInitialized && AbilitySystemComponent->GetOwnerActor() == AbilityOwner && AbilitySystemComponent->GetAvatarActor() == this)
	{
		AbilitySystemComponent->RefreshAbilityActorInfo();
		RefreshNetworkRoleFromOwner();
		TryFinishInitialization();
		return;
	}

	UninitializeAbilitySystem();
	if (APawn* AbilityOwnerPawn = Cast<APawn>(AbilityOwner))
	{
		BoundAbilityOwnerPawn = AbilityOwnerPawn;
		AbilityOwnerPawn->ReceiveControllerChangedDelegate.AddUniqueDynamic(this, &ThisClass::HandleOwnerControllerChanged);
	}
	RefreshNetworkRoleFromOwner();

	AbilitySystemComponent->InitAbilityActorInfo(AbilityOwner, this);
	bAbilityActorInfoInitialized = true;
	UE_LOG(LogStulWeaponSystem, Verbose, TEXT("Weapon '%s' initialized GAS actor info with owner '%s' and avatar '%s'."), *GetNameSafe(this), *GetNameSafe(AbilityOwner), *GetNameSafe(this));
	TryFinishInitialization();
}

void AStulWeapon::UninitializeAbilitySystem()
{
	if (APawn* AbilityOwnerPawn = BoundAbilityOwnerPawn.Get()) AbilityOwnerPawn->ReceiveControllerChangedDelegate.RemoveDynamic(this, &ThisClass::HandleOwnerControllerChanged);
	BoundAbilityOwnerPawn.Reset();

	if (!bAbilityActorInfoInitialized || !AbilitySystemComponent)
	{
		bAbilityActorInfoInitialized = false;
		return;
	}

	if (AbilitySystemComponent->GetAvatarActor() == this)
	{
		AbilitySystemComponent->CancelAbilities();
		AbilitySystemComponent->ClearAbilityInput();
		AbilitySystemComponent->RemoveAllGameplayCues();
		if (AbilitySystemComponent->GetOwnerActor()) AbilitySystemComponent->SetAvatarActor(nullptr);
		else AbilitySystemComponent->ClearActorInfo();
	}
	ActivePresentationCues.Reset();
	ResetAimState();
	bAbilityActorInfoInitialized = false;
}

void AStulWeapon::RefreshNetworkRoleFromOwner()
{
	if (!HasAuthority()) return;
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const APlayerController* OwnerPlayerController = OwnerPawn ? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;
	SetAutonomousProxy(OwnerPlayerController && !OwnerPlayerController->IsLocalController());
}

void AStulWeapon::HandleOwnerControllerChanged(APawn* Pawn, AController* /*OldController*/, AController* /*NewController*/)
{
	if (Pawn != GetOwner()) return;
	CancelActiveActions();
	RefreshNetworkRoleFromOwner();
	if (bAbilityActorInfoInitialized && AbilitySystemComponent && AbilitySystemComponent->GetAvatarActor() == this) AbilitySystemComponent->RefreshAbilityActorInfo();
	else InitializeAbilityActorInfo();
}

void AStulWeapon::TryFinishInitialization()
{
	if (bInitialized || !WeaponDefinition || !bRuntimeAssetsLoaded || !bAbilityActorInfoInitialized)
	{
		return;
	}

	bInitialized = true;
	AbilitySystemComponent->TryActivateAbilitiesOnSpawn();
	UE_LOG(LogStulWeaponSystem, Verbose, TEXT("Weapon '%s' initialized successfully from definition '%s'."), *GetNameSafe(this), *GetNameSafe(WeaponDefinition));
	OnWeaponReady.Broadcast(this);
}

void AStulWeapon::CancelPendingLoads()
{
	if (DefinitionLoadHandle.IsValid())
	{
		DefinitionLoadHandle->CancelHandle();
	}
	DefinitionLoadHandle.Reset();

	if (RuntimeAssetsLoadHandle.IsValid())
	{
		RuntimeAssetsLoadHandle->CancelHandle();
	}
	RuntimeAssetsLoadHandle.Reset();
}

void AStulWeapon::CollectRuntimeAssetPaths(TArray<FSoftObjectPath>& OutPaths) const
{
	OutPaths.Reset();
	if (!WeaponDefinition) return;

	WeaponDefinition->GetGameplayAssetPaths(OutPaths);
	if (GetNetMode() == NM_DedicatedServer) return;

	TArray<FSoftObjectPath> PresentationAssetPaths;
	WeaponDefinition->GetPresentationAssetPaths(PresentationAssetPaths);
	for (const FSoftObjectPath& AssetPath : PresentationAssetPaths) OutPaths.AddUnique(AssetPath);
}

/*********************************************************************************************/
/******************************** Attributes And Abilities ***********************************/
/*********************************************************************************************/

void AStulWeapon::ApplyDefinitionAttributes(const FStulWeaponInstanceData& InstanceData)
{
	if (!AbilitySystemComponent || !WeaponDefinition)
	{
		UE_LOG(LogStulWeaponSystem, Error, TEXT("Weapon '%s' cannot apply definition attributes (ASC: %s, definition: %s)."), *GetNameSafe(this), *GetNameSafe(AbilitySystemComponent), *GetNameSafe(WeaponDefinition));
		return;
	}

	auto SetBaseValue = [this](const FGameplayAttribute& Attribute, const float Value)
	{
		AbilitySystemComponent->SetNumericAttributeBase(Attribute, Value);
	};

	const float MaxAmmo = static_cast<float>(WeaponDefinition->BaseMagazineCapacity);
	const float InitialAmmo = InstanceData.CurrentAmmo == INDEX_NONE
		? MaxAmmo
		: FMath::Clamp(static_cast<float>(InstanceData.CurrentAmmo), 0.0f, MaxAmmo);

	SetBaseValue(UStulWeaponAttributeSet::GetMaxAmmoAttribute(), MaxAmmo);
	SetBaseValue(UStulWeaponAttributeSet::GetCurrentAmmoAttribute(), InitialAmmo);
	SetBaseValue(UStulWeaponAttributeSet::GetFireCostAttribute(), WeaponDefinition->BaseFireCost);
	SetBaseValue(UStulWeaponAttributeSet::GetBurstShotCountAttribute(), WeaponDefinition->BaseBurstShotCount);
	SetBaseValue(UStulWeaponAttributeSet::GetBurstIntervalAttribute(), WeaponDefinition->BaseBurstInterval);
	SetBaseValue(UStulWeaponAttributeSet::GetShotsPerFireAttribute(), WeaponDefinition->BaseShotsPerFire);
	SetBaseValue(UStulWeaponAttributeSet::GetPatternScaleAttribute(), WeaponDefinition->BasePatternScale);
	SetBaseValue(UStulWeaponAttributeSet::GetMinSpreadAttribute(), WeaponDefinition->BaseMinSpread);
	SetBaseValue(UStulWeaponAttributeSet::GetAirMinSpreadAttribute(), WeaponDefinition->BaseAirMinSpread);
	SetBaseValue(UStulWeaponAttributeSet::GetMaxSpreadAttribute(), WeaponDefinition->BaseMaxSpread);
	SetBaseValue(UStulWeaponAttributeSet::GetCrouchSpreadMultiplierAttribute(), WeaponDefinition->BaseCrouchSpreadMultiplier);
	SetBaseValue(UStulWeaponAttributeSet::GetStandSpreadMultiplierAttribute(), WeaponDefinition->BaseStandSpreadMultiplier);
	SetBaseValue(UStulWeaponAttributeSet::GetAirSpreadMultiplierAttribute(), WeaponDefinition->BaseAirSpreadMultiplier);
	SetBaseValue(UStulWeaponAttributeSet::GetSpreadIncreasePerShotAttribute(), WeaponDefinition->BaseSpreadIncreasePerShot);
	SetBaseValue(UStulWeaponAttributeSet::GetMaxSpreadOvershootAttribute(), WeaponDefinition->BaseMaxSpreadOvershoot);
	SetBaseValue(UStulWeaponAttributeSet::GetSpreadInterpSpeedAttribute(), WeaponDefinition->BaseSpreadInterpSpeed);
	SetBaseValue(UStulWeaponAttributeSet::GetDamageAttribute(), WeaponDefinition->BaseDamage);
	SetBaseValue(UStulWeaponAttributeSet::GetReloadDurationAttribute(), WeaponDefinition->BaseReloadDuration);
	SetBaseValue(UStulWeaponAttributeSet::GetAimDurationAttribute(), WeaponDefinition->BaseAimDuration);
	SetBaseValue(UStulWeaponAttributeSet::GetFireModeChangeDurationAttribute(), WeaponDefinition->BaseFireModeChangeDuration);
	SetBaseValue(UStulWeaponAttributeSet::GetFireIntervalAttribute(), WeaponDefinition->BaseFireInterval);
	SetBaseValue(UStulWeaponAttributeSet::GetMaxPenetrationCountAttribute(), WeaponDefinition->BasePenetrationCount);
	SetBaseValue(UStulWeaponAttributeSet::GetMaxPenetrationStrengthAttribute(), WeaponDefinition->BasePenetrationStrength);
}

void AStulWeapon::GrantDefinitionAbilities()
{
	if (!HasAuthority() || !AbilitySystemComponent || !WeaponDefinition)
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Weapon '%s' cannot grant definition abilities (authority: %s, ASC: %s, definition: %s)."), *GetNameSafe(this), HasAuthority() ? TEXT("true") : TEXT("false"), *GetNameSafe(AbilitySystemComponent), *GetNameSafe(WeaponDefinition));
		return;
	}

	for (const FStulWeaponAbilityMapping& Mapping : WeaponDefinition->BaseAbilities)
	{
		if (!Mapping.AbilityClass)
		{
			UE_LOG(LogStulWeaponSystem, Warning, TEXT("Weapon definition '%s' contains an ability mapping with no AbilityClass."), *GetNameSafe(WeaponDefinition));
			continue;
		}

		FGameplayAbilitySpec AbilitySpec(Mapping.AbilityClass, FMath::Max(1, Mapping.AbilityLevel));
		if (Mapping.InputTag.IsValid())
		{
			AbilitySpec.GetDynamicSpecSourceTags().AddTag(Mapping.InputTag);
		}

		const FGameplayAbilitySpecHandle GrantedHandle = AbilitySystemComponent->GiveAbility(AbilitySpec);
		if (!GrantedHandle.IsValid())
		{
			UE_LOG(LogStulWeaponSystem, Error, TEXT("Weapon '%s' failed to grant ability '%s' from definition '%s'."), *GetNameSafe(this), *GetNameSafe(Mapping.AbilityClass.Get()), *GetNameSafe(WeaponDefinition));
			continue;
		}

		GrantedAbilityHandles.Add(GrantedHandle);
	}
}

void AStulWeapon::ClearGrantedAbilities()
{
	if (HasAuthority() && AbilitySystemComponent)
	{
		for (const FGameplayAbilitySpecHandle Handle : GrantedAbilityHandles)
		{
			AbilitySystemComponent->ClearAbility(Handle);
		}
	}

	GrantedAbilityHandles.Reset();
}

/*********************************************************************************************/
/************************************* Asset Loading *****************************************/
/*********************************************************************************************/

bool AStulWeapon::BeginRuntimeAssetLoad()
{
	if (!WeaponDefinition)
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Weapon '%s' cannot load runtime assets because its definition is missing."), *GetNameSafe(this));
		return false;
	}

	TArray<FSoftObjectPath> RuntimeAssetPaths;
	CollectRuntimeAssetPaths(RuntimeAssetPaths);

	if (RuntimeAssetPaths.IsEmpty())
	{
		FinishRuntimeAssetLoad();
		return bInitialized;
	}

	RuntimeAssetsLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		RuntimeAssetPaths,
		FStreamableDelegate::CreateUObject(this, &ThisClass::FinishRuntimeAssetLoad));

	if (!RuntimeAssetsLoadHandle.IsValid())
	{
		FailInitialization(NSLOCTEXT("StulWeaponSystem", "WeaponAssetsLoadNotStarted", "The weapon runtime assets could not be loaded."));
		return false;
	}

	return true;
}

void AStulWeapon::FinishRuntimeAssetLoad()
{
	if (!WeaponDefinition)
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Weapon '%s' ignored its runtime asset callback because its definition is no longer valid."), *GetNameSafe(this));
		return;
	}

	if (!WeaponMesh || !AbilitySystemComponent)
	{
		FailInitialization(NSLOCTEXT(
			"StulWeaponSystem",
			"MissingWeaponRuntimeComponent",
			"A required weapon runtime component is missing."));
		return;
	}

	TArray<FSoftObjectPath> RuntimeAssetPaths;
	CollectRuntimeAssetPaths(RuntimeAssetPaths);
	for (const FSoftObjectPath& AssetPath : RuntimeAssetPaths)
	{
		if (!AssetPath.ResolveObject())
		{
			FailInitialization(FText::Format(
				NSLOCTEXT("StulWeaponSystem", "WeaponRuntimeAssetLoadFailed", "The runtime asset '{0}' failed to load."),
				FText::FromString(AssetPath.ToString())));
			return;
		}
	}

	WeaponMesh->SetSkeletalMesh(GetNetMode() == NM_DedicatedServer ? nullptr : WeaponDefinition->WeaponMesh.Get());
	if (WeaponMesh->GetSkeletalMeshAsset() && !WeaponMesh->DoesSocketExist(WeaponDefinition->MuzzleSocketName))
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Weapon '%s' mesh '%s' does not contain muzzle socket '%s'."), *GetNameSafe(this), *GetNameSafe(WeaponMesh->GetSkeletalMeshAsset()), *WeaponDefinition->MuzzleSocketName.ToString());
	}
	if (WeaponMesh->GetSkeletalMeshAsset() && !WeaponMesh->DoesSocketExist(WeaponDefinition->AimData.AimSocketName))
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Weapon '%s' mesh '%s' does not contain aim socket '%s'."), *GetNameSafe(this), *GetNameSafe(WeaponMesh->GetSkeletalMeshAsset()), *WeaponDefinition->AimData.AimSocketName.ToString());
	}

	bRuntimeAssetsLoaded = true;
	TryFinishInitialization();
}

void AStulWeapon::FailInitialization(const FText& Reason)
{
	CancelPendingLoads();
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->ClearAbilityInput();
		ClearGrantedAbilities();
	}

	bInitialized = false;
	bRuntimeAssetsLoaded = false;
	UE_LOG(LogStulWeaponSystem, Error, TEXT("Failed to initialize weapon '%s': %s"), *GetNameSafe(this), *Reason.ToString());
	OnWeaponInitializationFailed.Broadcast(this, Reason);
}

/*********************************************************************************************/
/****************************************** Data *********************************************/
/*********************************************************************************************/

bool AStulWeapon::TryMakeInstanceData(FStulWeaponInstanceData& OutInstanceData) const
{
	OutInstanceData = FStulWeaponInstanceData();
	if (!HasAuthority() || !bInitialized)
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Weapon '%s' cannot create instance data (authority: %s, initialized: %s)."), *GetNameSafe(this), HasAuthority() ? TEXT("true") : TEXT("false"), bInitialized ? TEXT("true") : TEXT("false"));
		return false;
	}

	OutInstanceData.ItemLevel = RuntimeInstanceData.ItemLevel;
	OutInstanceData.GenerationSeed = RuntimeInstanceData.GenerationSeed;
	OutInstanceData.WeaponDefinitionId = WeaponDefinitionId;

	if (WeaponAttributeSet)
	{
		OutInstanceData.CurrentAmmo = FMath::RoundToInt(WeaponAttributeSet->GetCurrentAmmo());
	}
	OutInstanceData.CurrentFireModeTag = CurrentFireModeTag;
	return true;
}

FTransform AStulWeapon::GetMuzzleTransform() const
{
	if (WeaponMesh && WeaponDefinition && WeaponMesh->DoesSocketExist(WeaponDefinition->MuzzleSocketName))
	{
		return WeaponMesh->GetSocketTransform(WeaponDefinition->MuzzleSocketName);
	}

	return GetActorTransform();
}

bool AStulWeapon::GetPresentationData_Implementation(FStulWeaponPresentationData& OutPresentation) const
{
	OutPresentation = FStulWeaponPresentationData();
	if (!WeaponDefinition)
	{
		return false;
	}

	OutPresentation = WeaponDefinition->Presentation;
	return true;
}

bool AStulWeapon::GetHitscanTracerData_Implementation(FStulWeaponTracerData& OutTracer) const
{
	OutTracer = FStulWeaponTracerData();
	if (!WeaponDefinition || WeaponDefinition->ShotType != EStulWeaponShotType::Hitscan)
	{
		return false;
	}

	OutTracer = WeaponDefinition->HitscanTracer;
	return true;
}

/*********************************************************************************************/
/******************************************* Aim *********************************************/
/*********************************************************************************************/

bool AStulWeapon::GetAimData_Implementation(FStulWeaponAimData& OutAimData) const
{
	OutAimData = FStulWeaponAimData();
	if (!WeaponDefinition)
	{
		return false;
	}

	OutAimData = WeaponDefinition->AimData;
	return true;
}

FTransform AStulWeapon::GetAimTransform_Implementation() const
{
	FStulWeaponAimData ResolvedAimData;
	if (WeaponMesh && GetAimData(ResolvedAimData) && WeaponMesh->DoesSocketExist(ResolvedAimData.AimSocketName))
	{
		return WeaponMesh->GetSocketTransform(ResolvedAimData.AimSocketName);
	}

	return GetActorTransform();
}

bool AStulWeapon::IsAiming() const
{
	return HasPresentationOrGameplayState(StulWeaponGameplayTags::State_Aiming, StulWeaponGameplayTags::GameplayCue_Aim);
}

float AStulWeapon::GetAimAlpha() const
{
	if (AimTransitionDuration <= UE_SMALL_NUMBER) return AimTransitionTargetAlpha;

	const UWorld* World = GetWorld();
	if (!World) return AimTransitionTargetAlpha;

	const float ElapsedTime = World->GetTimeSeconds() - AimTransitionStartTime;
	const float TransitionAlpha = FMath::Clamp(ElapsedTime / AimTransitionDuration, 0.0f, 1.0f);
	return FMath::Lerp(AimTransitionStartAlpha, AimTransitionTargetAlpha, TransitionAlpha);
}

bool AStulWeapon::IsFullyAimed() const
{
	return IsAiming() && GetAimAlpha() >= 1.0f - UE_KINDA_SMALL_NUMBER;
}

void AStulWeapon::CancelActiveActions()
{
	ClearAbilityInput();
	if (FireComponent) FireComponent->InterruptActiveSession();
	if (AbilitySystemComponent)
	{
		FGameplayTagContainer ActionTags;
		ActionTags.AddTag(StulWeaponGameplayTags::Ability_Fire);
		ActionTags.AddTag(StulWeaponGameplayTags::Ability_Aim);
		ActionTags.AddTag(StulWeaponGameplayTags::Ability_Reload);
		ActionTags.AddTag(StulWeaponGameplayTags::Ability_ChangeFireMode);
		AbilitySystemComponent->CancelAbilities(&ActionTags);
	}
	ClearPredictedProjectiles();
	ResetAimState();
}

void AStulWeapon::SetAimPresentationActive(const bool bActive)
{
	const float TargetAlpha = bActive ? 1.0f : 0.0f;
	if (FMath::IsNearlyEqual(AimTransitionTargetAlpha, TargetAlpha)) return;

	BeginAimTransition(bActive);
	OnAimStateChanged.Broadcast(this, bActive);
}

void AStulWeapon::BeginAimTransition(const bool bNewAiming)
{
	AimTransitionStartAlpha = GetAimAlpha();
	AimTransitionTargetAlpha = bNewAiming ? 1.0f : 0.0f;
	AimTransitionStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	AimTransitionDuration = WeaponAttributeSet ? FMath::Max(0.0f, WeaponAttributeSet->GetAimDuration()) : 0.0f;
}

void AStulWeapon::ResetAimState()
{
	AimTransitionStartAlpha = 0.0f;
	AimTransitionTargetAlpha = 0.0f;
	AimTransitionStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	AimTransitionDuration = 0.0f;
}

/*********************************************************************************************/
/***************************************** Reload ********************************************/
/*********************************************************************************************/

bool AStulWeapon::IsReloading() const
{
	return HasPresentationOrGameplayState(StulWeaponGameplayTags::State_Reloading, StulWeaponGameplayTags::GameplayCue_Reload);
}

/*********************************************************************************************/
/************************************** Fire Mode ********************************************/
/*********************************************************************************************/

bool AStulWeapon::GetNextFireMode(FGameplayTag& OutFireMode) const
{
	OutFireMode = FGameplayTag();
	if (!WeaponDefinition || WeaponDefinition->AvailableFireModes.IsEmpty())
	{
		return false;
	}

	const TConstArrayView<FGameplayTag> FireModeCycle = StulWeaponGameplayTags::GetSupportedFireModeCycle();
	int32 CurrentIndex = INDEX_NONE;
	for (int32 Index = 0; Index < FireModeCycle.Num(); ++Index)
	{
		if (FireModeCycle[Index] == CurrentFireModeTag)
		{
			CurrentIndex = Index;
			break;
		}
	}
	for (int32 Offset = 1; Offset <= FireModeCycle.Num(); ++Offset)
	{
		const FGameplayTag Candidate = FireModeCycle[(CurrentIndex + Offset + FireModeCycle.Num()) % FireModeCycle.Num()];
		if (Candidate != CurrentFireModeTag && WeaponDefinition->AvailableFireModes.HasTagExact(Candidate))
		{
			OutFireMode = Candidate;
			return true;
		}
	}
	return false;
}

bool AStulWeapon::IsChangingFireMode() const
{
	return HasPresentationOrGameplayState(StulWeaponGameplayTags::State_ChangingFireMode, StulWeaponGameplayTags::GameplayCue_ChangeFireMode);
}

bool AStulWeapon::CommitFireModeChange(const FGameplayTag NewFireMode)
{
	if (IsReloading() || !WeaponDefinition || !StulWeaponGameplayTags::IsSupportedFireMode(NewFireMode) || !WeaponDefinition->AvailableFireModes.HasTagExact(NewFireMode)) return false;
	if (!HasAuthority()) return HasLocalNetOwner() && ApplyPredictedFireMode(NewFireMode);
	if (CurrentFireModeTag == NewFireMode) return true;

	const FGameplayTag PreviousFireMode = CurrentFireModeTag;
	CurrentFireModeTag = NewFireMode;
	AuthoritativeFireModeTag = NewFireMode;
	RuntimeInstanceData.CurrentFireModeTag = NewFireMode;
	OnFireModeChanged.Broadcast(PreviousFireMode, CurrentFireModeTag);
	ForceNetUpdate();
	return true;
}

bool AStulWeapon::ApplyPredictedFireMode(const FGameplayTag NewFireMode)
{
	if (HasAuthority() || !HasLocalNetOwner() || !WeaponDefinition || !StulWeaponGameplayTags::IsSupportedFireMode(NewFireMode) || !WeaponDefinition->AvailableFireModes.HasTagExact(NewFireMode)) return false;
	if (CurrentFireModeTag == NewFireMode) return true;

	const FGameplayTag PreviousFireMode = CurrentFireModeTag;
	CurrentFireModeTag = NewFireMode;
	RuntimeInstanceData.CurrentFireModeTag = NewFireMode;
	OnFireModeChanged.Broadcast(PreviousFireMode, CurrentFireModeTag);
	return true;
}

bool AStulWeapon::RestoreAuthoritativeFireMode()
{
	return AuthoritativeFireModeTag.IsValid() && ApplyPredictedFireMode(AuthoritativeFireModeTag);
}

/*********************************************************************************************/
/***************************************** Input *********************************************/
/*********************************************************************************************/

void AStulWeapon::InputTagPressed(const FGameplayTag InputTag)
{
	if (!bInitialized)
	{
		UE_LOG(LogStulWeaponSystem, Verbose, TEXT("Weapon '%s' ignored pressed input '%s' because it is not initialized."), *GetNameSafe(this), *InputTag.ToString());
		return;
	}

	if (!AbilitySystemComponent)
	{
		UE_LOG(LogStulWeaponSystem, Error, TEXT("Weapon '%s' cannot process pressed input because its AbilitySystemComponent is missing."), *GetNameSafe(this));
		return;
	}

	AbilitySystemComponent->AbilityInputTagPressed(InputTag);
}

void AStulWeapon::InputTagReleased(const FGameplayTag InputTag)
{
	if (!AbilitySystemComponent)
	{
		UE_LOG(LogStulWeaponSystem, Error, TEXT("Weapon '%s' cannot process released input because its AbilitySystemComponent is missing."), *GetNameSafe(this));
		return;
	}

	AbilitySystemComponent->AbilityInputTagReleased(InputTag);
}

void AStulWeapon::ProcessAbilityInput(const bool bGamePaused)
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->ProcessAbilityInput(bGamePaused);
	}
}

void AStulWeapon::ClearAbilityInput()
{
	if (AbilitySystemComponent) AbilitySystemComponent->ClearAbilityInput();
}

/*********************************************************************************************/
/**************************************** Shooting *******************************************/
/*********************************************************************************************/
void AStulWeapon::HandleFiringStarted()
{
	if (HasLocalNetOwner()) OnLocalFiringStarted.Broadcast(this);
}

void AStulWeapon::HandleShotExecuted(const FStulWeaponShotExecution& ShotExecution)
{
	if (HasLocalNetOwner()) 
		OnLocalShotExecuted.Broadcast(this, ShotExecution);
	
	if (ShotExecution.Results.IsEmpty()) 
		return;

	ExecuteFireGameplayCue(ShotExecution.Results[0]);
	if (WeaponDefinition && WeaponDefinition->ShotType == EStulWeaponShotType::Hitscan)
	{
		for (const FStulWeaponShotResult& ShotResult : ShotExecution.Results)
		{
			ExecuteTracerGameplayCue(ShotResult);
		}
	}
}

void AStulWeapon::HandleFiringEnded()
{
	if (HasLocalNetOwner()) OnLocalFiringEnded.Broadcast(this);
}

void AStulWeapon::RegisterPredictedProjectile(const FStulProjectileId& ProjectileId, AStulWeaponProjectile* Projectile)
{
	if (!HasLocalNetOwner() || !ProjectileId.IsValid() || !IsValid(Projectile)) return;

	if (TWeakObjectPtr<AStulWeaponProjectile>* ExistingProjectile = PredictedProjectiles.Find(ProjectileId))
	{
		if (AStulWeaponProjectile* Existing = ExistingProjectile->Get()) Existing->Destroy();
	}
	PredictedProjectiles.Add(ProjectileId, Projectile);
}

AStulWeaponProjectile* AStulWeapon::FindPredictedProjectile(const FStulProjectileId& ProjectileId) const
{
	if (!ProjectileId.IsValid()) return nullptr;
	const TWeakObjectPtr<AStulWeaponProjectile>* Projectile = PredictedProjectiles.Find(ProjectileId);
	return Projectile ? Projectile->Get() : nullptr;
}

void AStulWeapon::ConfirmPredictedShot(const FStulShotId& ShotId)
{
	for (const TPair<FStulProjectileId, TWeakObjectPtr<AStulWeaponProjectile>>& Pair : PredictedProjectiles)
	{
		if (Pair.Key.ShotId == ShotId)
		{
			if (AStulWeaponProjectile* Projectile = Pair.Value.Get()) Projectile->ConfirmPrediction();
		}
	}
}

void AStulWeapon::RejectPredictedShot(const FStulShotId& ShotId)
{
	TArray<FStulProjectileId> RejectedProjectileIds;
	for (const TPair<FStulProjectileId, TWeakObjectPtr<AStulWeaponProjectile>>& Pair : PredictedProjectiles)
	{
		if (Pair.Key.ShotId == ShotId) RejectedProjectileIds.Add(Pair.Key);
	}
	for (const FStulProjectileId& ProjectileId : RejectedProjectileIds)
	{
		TWeakObjectPtr<AStulWeaponProjectile> Projectile;
		PredictedProjectiles.RemoveAndCopyValue(ProjectileId, Projectile);
		if (AStulWeaponProjectile* RejectedProjectile = Projectile.Get()) RejectedProjectile->Destroy();
	}
}

void AStulWeapon::UnregisterPredictedProjectile(const FStulProjectileId& ProjectileId, const AStulWeaponProjectile* Projectile)
{
	const TWeakObjectPtr<AStulWeaponProjectile>* RegisteredProjectile = PredictedProjectiles.Find(ProjectileId);
	if (RegisteredProjectile && RegisteredProjectile->Get() == Projectile) PredictedProjectiles.Remove(ProjectileId);
}

void AStulWeapon::ClearPredictedProjectiles()
{
	TArray<TWeakObjectPtr<AStulWeaponProjectile>> Projectiles;
	PredictedProjectiles.GenerateValueArray(Projectiles);
	PredictedProjectiles.Reset();
	for (const TWeakObjectPtr<AStulWeaponProjectile>& Projectile : Projectiles)
	{
		if (AStulWeaponProjectile* PredictedProjectile = Projectile.Get()) PredictedProjectile->Destroy();
	}
}

void AStulWeapon::HandleAuthoritativeHit(const FHitResult& HitResult, const float Damage, AActor* EffectCauser, UStulAmmoDefinition* AmmoDefinition, const bool bExecutePresentation)
{
	if (!HasAuthority())
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Weapon '%s' rejected a non-authoritative hit."), *GetNameSafe(this));
		return;
	}

	UStulAmmoDefinition* ImpactAmmo = AmmoDefinition ? AmmoDefinition : GetCurrentAmmoDefinition();
	AActor* ResolvedEffectCauser = EffectCauser ? EffectCauser : this;
	if (bExecutePresentation) ExecuteImpactGameplayCue(HitResult, Damage, ResolvedEffectCauser, ImpactAmmo);

	AActor* HitActor = HitResult.GetActor();
	if (UAbilitySystemComponent* HitAbilitySystem = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor))
	{
		FGameplayEventData HitEventData;
		HitEventData.EventTag = StulWeaponGameplayTags::Event_Hit;
		HitEventData.Instigator = GetOwner();
		HitEventData.Target = HitActor;
		HitEventData.OptionalObject = this;
		HitEventData.EventMagnitude = FMath::Max(0.0f, Damage);
		HitEventData.TargetData.Add(new FGameplayAbilityTargetData_SingleTargetHit(HitResult));
		HitAbilitySystem->HandleGameplayEvent(StulWeaponGameplayTags::Event_Hit, &HitEventData);
	}

	OnAuthoritativeHit.Broadcast(this, HitResult, FMath::Max(0.0f, Damage), ResolvedEffectCauser, ImpactAmmo);
}

void AStulWeapon::ExecuteFireGameplayCue(const FStulWeaponShotResult& ShotResult)
{
	if (!AbilitySystemComponent)
	{
		UE_LOG(LogStulWeaponSystem, Error, TEXT("Weapon '%s' cannot execute its fire Gameplay Cue because its AbilitySystemComponent is missing."), *GetNameSafe(this));
		return;
	}

	FGameplayCueParameters CueParameters;
	CueParameters.Location = ShotResult.MuzzleLocation;
	CueParameters.Normal = ShotResult.Direction;
	CueParameters.Instigator = GetOwner();
	CueParameters.EffectCauser = this;
	CueParameters.SourceObject = WeaponDefinition;
	AbilitySystemComponent->InvokeGameplayCueEvent(StulWeaponGameplayTags::GameplayCue_Fire, EGameplayCueEvent::Executed, CueParameters);
}

void AStulWeapon::ExecuteTracerGameplayCue(const FStulWeaponShotResult& ShotResult)
{
	if (!AbilitySystemComponent)
	{
		UE_LOG(LogStulWeaponSystem, Error, TEXT("Weapon '%s' cannot execute its tracer Gameplay Cue because its AbilitySystemComponent is missing."), *GetNameSafe(this));
		return;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(WeaponDefinition);
	if (ShotResult.bBlockingHit)
	{
		EffectContext.AddHitResult(ShotResult.HitResult, true);
	}

	FGameplayCueParameters CueParameters(EffectContext);
	CueParameters.Location = ShotResult.MuzzleLocation;
	FVector CosmeticEnd = FVector(ShotResult.EndLocation);
	if (ShotResult.bBlockingHit && ShotResult.HitResult.bBlockingHit)
	{
		CosmeticEnd = FVector(ShotResult.HitResult.ImpactPoint);
	}
	const FVector CosmeticPath = CosmeticEnd - FVector(ShotResult.MuzzleLocation);
	CueParameters.Normal = CosmeticPath.GetSafeNormal();
	CueParameters.RawMagnitude = CosmeticPath.Length();
	CueParameters.Instigator = GetOwner();
	CueParameters.EffectCauser = this;
	CueParameters.SourceObject = WeaponDefinition;
	AbilitySystemComponent->InvokeGameplayCueEvent(StulWeaponGameplayTags::GameplayCue_Tracer, EGameplayCueEvent::Executed, CueParameters);
}

void AStulWeapon::ExecuteImpactGameplayCue(const FHitResult& HitResult, const float Damage, AActor* EffectCauser, UStulAmmoDefinition* AmmoDefinition)
{
	if (!AbilitySystemComponent)
	{
		UE_LOG(LogStulWeaponSystem, Error, TEXT("Weapon '%s' cannot execute its impact Gameplay Cue because its AbilitySystemComponent is missing."), *GetNameSafe(this));
		return;
	}

	UStulAmmoDefinition* ImpactAmmo = AmmoDefinition ? AmmoDefinition : GetCurrentAmmoDefinition();
	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(ImpactAmmo);
	EffectContext.AddHitResult(HitResult, true);

	FGameplayCueParameters CueParameters(EffectContext);
	CueParameters.Location = HitResult.ImpactPoint;
	CueParameters.Normal = HitResult.ImpactNormal;
	CueParameters.RawMagnitude = FMath::Max(0.0f, Damage);
	CueParameters.Instigator = GetOwner();
	CueParameters.EffectCauser = EffectCauser ? EffectCauser : this;
	CueParameters.SourceObject = ImpactAmmo;
	CueParameters.PhysicalMaterial = HitResult.PhysMaterial.Get();
	AbilitySystemComponent->InvokeGameplayCueEvent(StulWeaponGameplayTags::GameplayCue_Impact, EGameplayCueEvent::Executed, CueParameters);
}

/*********************************************************************************************/
/************************************* Replication *******************************************/
/*********************************************************************************************/

void AStulWeapon::OnRep_WeaponDefinitionId()
{
	bInitializationStarted = true;
	CancelPendingLoads();
	AbilitySystemComponent->ClearAbilityInput();
	bInitialized = false;
	bRuntimeAssetsLoaded = false;
	WeaponDefinition = nullptr;

	if (!WeaponDefinitionId.IsValid())
	{
		FailInitialization(NSLOCTEXT(
			"StulWeaponSystem",
			"InvalidReplicatedWeaponDefinitionId",
			"The replicated weapon definition ID is invalid."));
		return;
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	if (UStulWeaponDefinition* LoadedDefinition = Cast<UStulWeaponDefinition>(AssetManager.GetPrimaryAssetObject(WeaponDefinitionId)))
	{
		WeaponDefinition = LoadedDefinition;
		RuntimeInstanceData.WeaponDefinitionId = WeaponDefinitionId;
		BeginRuntimeAssetLoad();
		return;
	}

	const FPrimaryAssetId RequestedDefinitionId = WeaponDefinitionId;
	DefinitionLoadHandle = AssetManager.LoadPrimaryAsset(
		RequestedDefinitionId,
		TArray<FName>(),
		FStreamableDelegate::CreateUObject(
			this,
			&ThisClass::FinishReplicatedDefinitionLoad,
			RequestedDefinitionId));

	if (!DefinitionLoadHandle.IsValid())
	{
		FailInitialization(FText::Format(
			NSLOCTEXT("StulWeaponSystem", "ReplicatedWeaponDefinitionLoadNotStarted", "The replicated weapon definition '{0}' could not be loaded."),
			FText::FromString(RequestedDefinitionId.ToString())));
	}
}

void AStulWeapon::OnRep_CurrentFireModeTag(const FGameplayTag& PreviousFireMode)
{
	AuthoritativeFireModeTag = CurrentFireModeTag;
	if (PreviousFireMode != CurrentFireModeTag) OnFireModeChanged.Broadcast(PreviousFireMode, CurrentFireModeTag);
}

void AStulWeapon::OnRep_WeaponBallisticSeed()
{
	if (WeaponBallisticSeed == 0) UE_LOG(LogStulWeaponSystem, Warning, TEXT("Weapon '%s' received an invalid ballistic seed."), *GetNameSafe(this));
}
