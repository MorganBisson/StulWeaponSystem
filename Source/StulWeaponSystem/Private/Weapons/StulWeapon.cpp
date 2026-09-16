#include "Weapons/StulWeapon.h"

#include "AbilitySystem/StulWeaponAbilitySystemComponent.h"
#include "AbilitySystem/StulWeaponAttributeSet.h"
#include "AbilitySystem/StulWeaponGameplayAbility.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Actor.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffectTypes.h"
#include "Net/UnrealNetwork.h"
#include "StulWeaponGameplayTags.h"
#include "StulWeaponSystem.h"
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
}

UAbilitySystemComponent* AStulWeapon::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AStulWeapon::BeginPlay()
{
	Super::BeginPlay();
	AbilitySystemComponent->RegisterGameplayTagEvent(StulWeaponGameplayTags::State_Aiming, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleAimStateTagChanged);
	AbilitySystemComponent->RegisterGameplayTagEvent(StulWeaponGameplayTags::State_Reloading, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleReloadStateTagChanged);
	AbilitySystemComponent->RegisterGameplayTagEvent(StulWeaponGameplayTags::State_ChangingFireMode, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleFireModeChangeStateTagChanged);
	AbilitySystemComponent->GenericGameplayEventCallbacks.FindOrAdd(StulWeaponGameplayTags::Event_Reload_Commit).AddUObject(this, &ThisClass::HandleReloadGameplayEvent, StulWeaponGameplayTags::Event_Reload_Commit.GetTag());
	AbilitySystemComponent->GenericGameplayEventCallbacks.FindOrAdd(StulWeaponGameplayTags::Event_Reload_Completed).AddUObject(this, &ThisClass::HandleReloadGameplayEvent, StulWeaponGameplayTags::Event_Reload_Completed.GetTag());
	AbilitySystemComponent->GenericGameplayEventCallbacks.FindOrAdd(StulWeaponGameplayTags::Event_Reload_Cancelled).AddUObject(this, &ThisClass::HandleReloadGameplayEvent, StulWeaponGameplayTags::Event_Reload_Cancelled.GetTag());
}

void AStulWeapon::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelPendingLoads();
	AbilitySystemComponent->RegisterGameplayTagEvent(StulWeaponGameplayTags::State_Aiming, EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
	AbilitySystemComponent->RegisterGameplayTagEvent(StulWeaponGameplayTags::State_Reloading, EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
	AbilitySystemComponent->RegisterGameplayTagEvent(StulWeaponGameplayTags::State_ChangingFireMode, EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
	AbilitySystemComponent->GenericGameplayEventCallbacks.FindOrAdd(StulWeaponGameplayTags::Event_Reload_Commit).RemoveAll(this);
	AbilitySystemComponent->GenericGameplayEventCallbacks.FindOrAdd(StulWeaponGameplayTags::Event_Reload_Completed).RemoveAll(this);
	AbilitySystemComponent->GenericGameplayEventCallbacks.FindOrAdd(StulWeaponGameplayTags::Event_Reload_Cancelled).RemoveAll(this);
	AbilitySystemComponent->ClearAbilityInput();
	ClearGrantedAbilities();

	if (AbilitySystemComponent && bAbilityActorInfoInitialized)
	{
		AbilitySystemComponent->ClearActorInfo();
		bAbilityActorInfoInitialized = false;
	}

	Super::EndPlay(EndPlayReason);
}

void AStulWeapon::OnRep_Owner()
{
	Super::OnRep_Owner();
	AbilitySystemComponent->ClearAbilityInput();

	if (IsValid(GetOwner()))
	{
		InitializeAbilityActorInfo();
	}
	else if (bAbilityActorInfoInitialized)
	{
		AbilitySystemComponent->ClearActorInfo();
		bAbilityActorInfoInitialized = false;
		bInitialized = false;
	}
}

void AStulWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AStulWeapon, WeaponDefinitionId);
	DOREPLIFETIME_CONDITION_NOTIFY(AStulWeapon, CurrentFireModeTag, COND_None, REPNOTIFY_Always);
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

	if (InDefinition->AvailableFireModes.IsEmpty() || !InDefinition->DefaultFireModeTag.IsValid() || !InDefinition->AvailableFireModes.HasTagExact(InDefinition->DefaultFireModeTag))
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

	// The ASC initially sees its component owner (the weapon) during registration.
	// Clearing first guarantees that InitAbilityActorInfo treats the weapon as a newly assigned avatar
	// and dispatches OnAvatarSet after the logical owner is known.
	AbilitySystemComponent->ClearActorInfo();
	AbilitySystemComponent->InitAbilityActorInfo(AbilityOwner, this);
	bAbilityActorInfoInitialized = true;
	UE_LOG(LogStulWeaponSystem, Verbose, TEXT("Weapon '%s' initialized GAS actor info with owner '%s' and avatar '%s'."), *GetNameSafe(this), *GetNameSafe(AbilityOwner), *GetNameSafe(this));
	TryFinishInitialization();
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

		FGameplayAbilitySpec AbilitySpec(Mapping.AbilityClass, FMath::Max(1, Mapping.AbilityLevel), INDEX_NONE, this);
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
	WeaponDefinition->GetGameplayAssetPaths(RuntimeAssetPaths);
	if (GetNetMode() != NM_DedicatedServer)
	{
		TArray<FSoftObjectPath> PresentationAssetPaths;
		WeaponDefinition->GetPresentationAssetPaths(PresentationAssetPaths);
		for (const FSoftObjectPath& AssetPath : PresentationAssetPaths)
		{
			RuntimeAssetPaths.AddUnique(AssetPath);
		}
	}

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
	WeaponDefinition->GetGameplayAssetPaths(RuntimeAssetPaths);
	if (GetNetMode() != NM_DedicatedServer)
	{
		TArray<FSoftObjectPath> PresentationAssetPaths;
		WeaponDefinition->GetPresentationAssetPaths(PresentationAssetPaths);
		for (const FSoftObjectPath& AssetPath : PresentationAssetPaths)
		{
			RuntimeAssetPaths.AddUnique(AssetPath);
		}
	}
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
	return AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(StulWeaponGameplayTags::State_Aiming);
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
	if (AbilitySystemComponent)
	{
		FGameplayTagContainer ActionTags;
		ActionTags.AddTag(StulWeaponGameplayTags::Ability_Fire);
		ActionTags.AddTag(StulWeaponGameplayTags::Ability_Aim);
		ActionTags.AddTag(StulWeaponGameplayTags::Ability_Reload);
		ActionTags.AddTag(StulWeaponGameplayTags::Ability_ChangeFireMode);
		AbilitySystemComponent->CancelAbilities(&ActionTags);
	}
	ResetAimState();
}

void AStulWeapon::HandleAimStateTagChanged(const FGameplayTag /*CallbackTag*/, const int32 NewCount)
{
	const bool bNewAiming = NewCount > 0;
	BeginAimTransition(bNewAiming);
	OnAimStateChanged.Broadcast(this, bNewAiming);
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
	return AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(StulWeaponGameplayTags::State_Reloading);
}

void AStulWeapon::HandleReloadStateTagChanged(const FGameplayTag /*CallbackTag*/, const int32 NewCount)
{
	if (NewCount > 0) OnReloadStarted.Broadcast(this);
}

void AStulWeapon::HandleReloadGameplayEvent(const FGameplayEventData* Payload, const FGameplayTag EventTag)
{
	if (EventTag == StulWeaponGameplayTags::Event_Reload_Commit)
	{
		const int32 AmmoRestored = Payload ? FMath::Max(0, FMath::RoundToInt(Payload->EventMagnitude)) : 0;
		OnReloadCommit.Broadcast(this, AmmoRestored);
	}
	else if (EventTag == StulWeaponGameplayTags::Event_Reload_Completed)
	{
		OnReloadCompleted.Broadcast(this);
	}
	else if (EventTag == StulWeaponGameplayTags::Event_Reload_Cancelled)
	{
		OnReloadCancelled.Broadcast(this);
	}
}

/*********************************************************************************************/
/************************************** Fire Mode ********************************************/
/*********************************************************************************************/

bool AStulWeapon::SetCurrentFireMode(const FGameplayTag NewFireMode)
{
	if (!HasAuthority())
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Weapon '%s' rejected fire mode '%s': fire modes must be changed on the server."), *GetNameSafe(this), *NewFireMode.ToString());
		return false;
	}
	if (IsReloading())
	{
		UE_LOG(LogStulWeaponSystem, Verbose, TEXT("Weapon '%s' rejected fire mode '%s' while reloading."), *GetNameSafe(this), *NewFireMode.ToString());
		return false;
	}

	if (!WeaponDefinition || !WeaponDefinition->AvailableFireModes.HasTagExact(NewFireMode))
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Weapon '%s' rejected unavailable fire mode '%s' for definition '%s'."), *GetNameSafe(this), *NewFireMode.ToString(), *GetNameSafe(WeaponDefinition));
		return false;
	}

	if (CurrentFireModeTag == NewFireMode)
	{
		return true;
	}

	const FGameplayTag PreviousFireMode = CurrentFireModeTag;
	CurrentFireModeTag = NewFireMode;
	AuthoritativeFireModeTag = NewFireMode;
	RuntimeInstanceData.CurrentFireModeTag = NewFireMode;
	OnFireModeChanged.Broadcast(PreviousFireMode, CurrentFireModeTag);
	ForceNetUpdate();
	return true;
}

bool AStulWeapon::CycleFireMode()
{
	if (IsReloading() || IsChangingFireMode())
	{
		return false;
	}

	if (!HasAuthority())
	{
		if (!HasLocalNetOwner())
		{
			UE_LOG(LogStulWeaponSystem, Warning, TEXT("Weapon '%s' rejected CycleFireMode: only its owning client may send this request."), *GetNameSafe(this));
			return false;
		}

		ServerCycleFireMode();
		return true;
	}

	if (!WeaponDefinition)
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Weapon '%s' cannot cycle fire mode because its definition is missing."), *GetNameSafe(this));
		return false;
	}

	if (WeaponDefinition->AvailableFireModes.IsEmpty())
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Weapon definition '%s' has no available fire mode."), *GetNameSafe(WeaponDefinition));
		return false;
	}

	FGameplayTag NextFireMode;
	if (GetNextFireMode(NextFireMode)) return SetCurrentFireMode(NextFireMode);

	UE_LOG(LogStulWeaponSystem, Warning, TEXT("Weapon definition '%s' contains no fire mode supported by CycleFireMode."), *GetNameSafe(WeaponDefinition));
	return false;
}

bool AStulWeapon::GetNextFireMode(FGameplayTag& OutFireMode) const
{
	OutFireMode = FGameplayTag();
	if (!WeaponDefinition || WeaponDefinition->AvailableFireModes.IsEmpty())
	{
		return false;
	}

	static const TArray<FGameplayTag> FireModeCycle =
	{
		StulWeaponGameplayTags::FireMode_Single,
		StulWeaponGameplayTags::FireMode_Burst,
		StulWeaponGameplayTags::FireMode_Automatic
	};

	const int32 CurrentIndex = FireModeCycle.IndexOfByKey(CurrentFireModeTag);
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
	return AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(StulWeaponGameplayTags::State_ChangingFireMode);
}

bool AStulWeapon::CommitFireModeChange(const FGameplayTag NewFireMode)
{
	if (HasAuthority()) return SetCurrentFireMode(NewFireMode);
	if (!HasLocalNetOwner() || IsReloading() || !WeaponDefinition || !WeaponDefinition->AvailableFireModes.HasTagExact(NewFireMode)) return false;
	return ApplyPredictedFireMode(NewFireMode);
}

bool AStulWeapon::ApplyPredictedFireMode(const FGameplayTag NewFireMode)
{
	if (HasAuthority() || !HasLocalNetOwner() || !WeaponDefinition || !WeaponDefinition->AvailableFireModes.HasTagExact(NewFireMode)) return false;
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

void AStulWeapon::HandleFireModeChangeStateTagChanged(const FGameplayTag /*CallbackTag*/, const int32 NewCount)
{
	OnFireModeChangeStateChanged.Broadcast(this, NewCount > 0);
}

void AStulWeapon::ServerCycleFireMode_Implementation()
{
	CycleFireMode();
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
void AStulWeapon::NotifyFiringStarted()
{
	OnFiringStarted.Broadcast(this);
}

void AStulWeapon::NotifyShotExecuted(const FStulWeaponShotResult& ShotResult)
{
	OnShotExecuted.Broadcast(this, ShotResult);
}

void AStulWeapon::NotifyFiringEnded()
{
	OnFiringEnded.Broadcast(this);
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
	AbilitySystemComponent->ExecuteGameplayCue(StulWeaponGameplayTags::GameplayCue_Fire, CueParameters);
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
	const FVector CosmeticPath = FVector(ShotResult.EndLocation) - FVector(ShotResult.MuzzleLocation);
	CueParameters.Normal = CosmeticPath.GetSafeNormal();
	CueParameters.RawMagnitude = CosmeticPath.Length();
	CueParameters.Instigator = GetOwner();
	CueParameters.EffectCauser = this;
	CueParameters.SourceObject = WeaponDefinition;
	AbilitySystemComponent->ExecuteGameplayCue(StulWeaponGameplayTags::GameplayCue_Tracer, CueParameters);
}

void AStulWeapon::ExecuteImpactGameplayCue(const FHitResult& HitResult, const float Damage, AActor* EffectCauser)
{
	if (!AbilitySystemComponent)
	{
		UE_LOG(LogStulWeaponSystem, Error, TEXT("Weapon '%s' cannot execute its impact Gameplay Cue because its AbilitySystemComponent is missing."), *GetNameSafe(this));
		return;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(WeaponDefinition);
	EffectContext.AddHitResult(HitResult, true);

	FGameplayCueParameters CueParameters(EffectContext);
	CueParameters.Location = HitResult.ImpactPoint;
	CueParameters.Normal = HitResult.ImpactNormal;
	CueParameters.RawMagnitude = FMath::Max(0.0f, Damage);
	CueParameters.Instigator = GetOwner();
	CueParameters.EffectCauser = EffectCauser ? EffectCauser : this;
	CueParameters.SourceObject = WeaponDefinition;
	CueParameters.PhysicalMaterial = HitResult.PhysMaterial.Get();
	AbilitySystemComponent->ExecuteGameplayCue(StulWeaponGameplayTags::GameplayCue_Impact, CueParameters);
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
