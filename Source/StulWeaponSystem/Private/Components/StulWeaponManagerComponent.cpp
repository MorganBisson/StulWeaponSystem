#include "Components/StulWeaponManagerComponent.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "StulWeaponSystem.h"
#include "Weapons/StulWeapon.h"
#include "Weapons/StulWeaponDefinition.h"

/*********************************************************************************************/
/******************************** Replicated Weapon List *************************************/
/*********************************************************************************************/

bool FStulWeaponList::NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
{
	return FFastArraySerializer::FastArrayDeltaSerialize<FStulWeaponEntry, FStulWeaponList>(Entries, DeltaParams, *this);
}

void FStulWeaponList::SetOwner(UStulWeaponManagerComponent* InOwnerComponent)
{
	OwnerComponent = InOwnerComponent;
}

void FStulWeaponList::AddWeapon(AStulWeapon* Weapon, const int32 SlotIndex)
{
	FStulWeaponEntry& NewEntry = Entries.Emplace_GetRef();
	NewEntry.Weapon = Weapon;
	NewEntry.SlotIndex = SlotIndex;
	MarkItemDirty(NewEntry);
}

bool FStulWeaponList::RemoveWeapon(AStulWeapon* Weapon, int32& OutSlotIndex)
{
	const int32 EntryIndex = Entries.IndexOfByPredicate([Weapon](const FStulWeaponEntry& Entry) { return Entry.Weapon.Get() == Weapon; });
	if (EntryIndex == INDEX_NONE) return false;

	OutSlotIndex = Entries[EntryIndex].SlotIndex;
	Entries.RemoveAtSwap(EntryIndex);
	MarkArrayDirty();
	return true;
}

AStulWeapon* FStulWeaponList::GetWeaponAtSlot(const int32 SlotIndex) const
{
	const FStulWeaponEntry* Entry = Entries.FindByPredicate([SlotIndex](const FStulWeaponEntry& Candidate) { return Candidate.SlotIndex == SlotIndex; });
	return Entry ? Entry->Weapon.Get() : nullptr;
}

int32 FStulWeaponList::GetSlotForWeapon(const AStulWeapon* Weapon) const
{
	if (!Weapon) return INDEX_NONE;

	const FStulWeaponEntry* Entry = Entries.FindByPredicate([Weapon](const FStulWeaponEntry& Candidate) { return Candidate.Weapon.Get() == Weapon; });
	return Entry ? Entry->SlotIndex : INDEX_NONE;
}

bool FStulWeaponList::Contains(const AStulWeapon* Weapon) const
{
	return GetSlotForWeapon(Weapon) != INDEX_NONE;
}

void FStulWeaponList::PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, const int32 /*FinalSize*/)
{
	if (!IsValid(OwnerComponent)) return;

	for (const int32 EntryIndex : RemovedIndices)
	{
		if (!Entries.IsValidIndex(EntryIndex)) continue;

		FStulWeaponEntry& Entry = Entries[EntryIndex];
		if (Entry.bAddedEventBroadcast) OwnerComponent->HandleReplicatedWeaponRemoved(Entry.Weapon, Entry.SlotIndex);
	}
}

void FStulWeaponList::PostReplicatedAdd(const TArrayView<int32>& AddedIndices, const int32 /*FinalSize*/)
{
	if (!IsValid(OwnerComponent)) return;

	for (const int32 EntryIndex : AddedIndices)
	{
		if (!Entries.IsValidIndex(EntryIndex)) continue;

		FStulWeaponEntry& Entry = Entries[EntryIndex];
		if (IsValid(Entry.Weapon))
		{
			Entry.bAddedEventBroadcast = true;
			OwnerComponent->HandleReplicatedWeaponAdded(Entry.Weapon, Entry.SlotIndex);
		}
	}
}

void FStulWeaponList::PostReplicatedChange(const TArrayView<int32>& ChangedIndices, const int32 /*FinalSize*/)
{
	if (!IsValid(OwnerComponent)) return;

	for (const int32 EntryIndex : ChangedIndices)
	{
		if (!Entries.IsValidIndex(EntryIndex)) continue;

		FStulWeaponEntry& Entry = Entries[EntryIndex];
		if (!Entry.bAddedEventBroadcast && IsValid(Entry.Weapon))
		{
			Entry.bAddedEventBroadcast = true;
			OwnerComponent->HandleReplicatedWeaponAdded(Entry.Weapon, Entry.SlotIndex);
		}
	}
}

/*********************************************************************************************/
/************************************* Initialization ****************************************/
/*********************************************************************************************/

UStulWeaponManagerComponent::UStulWeaponManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	WeaponClass = AStulWeapon::StaticClass();
	WeaponList.SetOwner(this);
}

void UStulWeaponManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	MaxWeaponSlots = FMath::Max(1, MaxWeaponSlots);
	WeaponList.SetOwner(this);
}

void UStulWeaponManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		TSet<TWeakObjectPtr<AStulWeapon>> WeaponsToDestroy;
		for (const FStulWeaponEntry& Entry : WeaponList.GetEntries())
		{
			if (IsValid(Entry.Weapon)) WeaponsToDestroy.Add(TWeakObjectPtr<AStulWeapon>(Entry.Weapon));
		}
		for (const TPair<TWeakObjectPtr<AStulWeapon>, int32>& PendingWeapon : PendingWeaponSlots)
		{
			if (PendingWeapon.Key.IsValid()) WeaponsToDestroy.Add(PendingWeapon.Key);
		}
		for (const TWeakObjectPtr<AStulWeapon>& Weapon : WeaponsToDestroy)
		{
			Weapon->OnWeaponReady.RemoveDynamic(this, &ThisClass::HandleWeaponReady);
			Weapon->OnWeaponInitializationFailed.RemoveDynamic(this, &ThisClass::HandleWeaponInitializationFailed);
			Weapon->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleWeaponDestroyed);
			Weapon->Destroy();
		}
	}

	PendingWeaponSlots.Reset();
	PendingDefaultLoadoutWeapons.Reset();
	Super::EndPlay(EndPlayReason);
}

void UStulWeaponManagerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UStulWeaponManagerComponent, WeaponList);
	DOREPLIFETIME(UStulWeaponManagerComponent, EquippedWeapon);
	DOREPLIFETIME(UStulWeaponManagerComponent, bIsReady);
}

void UStulWeaponManagerComponent::InitializeDefaultLoadout()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Weapon Manager '%s' rejected default loadout initialization because it must run on the server."), *GetNameSafe(this));
		return;
	}
	if (bDefaultLoadoutInitializationStarted)
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Weapon Manager '%s' ignored repeated default loadout initialization."), *GetNameSafe(this));
		return;
	}

	bDefaultLoadoutInitializationStarted = true;
	const int32 WeaponsToSpawn = FMath::Min(DefaultWeaponLoadout.Num(), MaxWeaponSlots);
	if (DefaultWeaponLoadout.Num() > MaxWeaponSlots) UE_LOG(LogStulWeaponSystem, Warning, TEXT("Weapon Manager '%s' has %d default weapons but only %d slots; overflowing entries will be skipped."), *GetNameSafe(this), DefaultWeaponLoadout.Num(), MaxWeaponSlots);

	for (int32 LoadoutIndex = 0; LoadoutIndex < WeaponsToSpawn; ++LoadoutIndex)
	{
		const FPrimaryAssetId& WeaponDefinitionId = DefaultWeaponLoadout[LoadoutIndex];
		if (!WeaponDefinitionId.IsValid())
		{
			UE_LOG(LogStulWeaponSystem, Warning, TEXT("Weapon Manager '%s' skipped invalid default weapon at index %d."), *GetNameSafe(this), LoadoutIndex);
			continue;
		}

		FStulWeaponInstanceData InstanceData;
		InstanceData.WeaponDefinitionId = WeaponDefinitionId;
		SpawnAndInitializeWeapon(nullptr, InstanceData, true);
	}

	bDefaultLoadoutSchedulingComplete = true;
	CheckDefaultLoadoutReady();
}

void UStulWeaponManagerComponent::CheckDefaultLoadoutReady()
{
	if (!bDefaultLoadoutInitializationStarted || !bDefaultLoadoutSchedulingComplete || bIsReady || !PendingDefaultLoadoutWeapons.IsEmpty()) return;

	bIsReady = true;
	UE_LOG(LogStulWeaponSystem, Verbose, TEXT("Weapon Manager '%s' finished initializing its default loadout."), *GetNameSafe(this));
	OnWeaponManagerReady.Broadcast();
	if (AActor* OwnerActor = GetOwner()) OwnerActor->ForceNetUpdate();
}

/*********************************************************************************************/
/********************************** Weapon Management ****************************************/
/*********************************************************************************************/

AStulWeapon* UStulWeaponManagerComponent::GiveWeaponFromDefinition(UStulWeaponDefinition* WeaponDefinition)
{
	if (!IsValid(WeaponDefinition))
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Weapon Manager '%s' cannot give a weapon from an invalid definition."), *GetNameSafe(this));
		return nullptr;
	}

	FStulWeaponInstanceData InstanceData;
	InstanceData.WeaponDefinitionId = WeaponDefinition->GetPrimaryAssetId();
	return SpawnAndInitializeWeapon(WeaponDefinition, InstanceData, false);
}

AStulWeapon* UStulWeaponManagerComponent::GiveWeaponFromInstanceData(const FStulWeaponInstanceData& InstanceData)
{
	if (!InstanceData.IsValid())
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Weapon Manager '%s' cannot restore a weapon from invalid instance data."), *GetNameSafe(this));
		return nullptr;
	}

	return SpawnAndInitializeWeapon(nullptr, InstanceData, false);
}

AStulWeapon* UStulWeaponManagerComponent::SpawnAndInitializeWeapon(UStulWeaponDefinition* ResolvedDefinition, const FStulWeaponInstanceData& InstanceData, const bool bDefaultLoadoutWeapon)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Weapon Manager '%s' rejected weapon creation because it must run on the server."), *GetNameSafe(this));
		return nullptr;
	}
	if (!WeaponClass)
	{
		UE_LOG(LogStulWeaponSystem, Error, TEXT("Weapon Manager '%s' cannot create a weapon because WeaponClass is null."), *GetNameSafe(this));
		return nullptr;
	}

	const int32 SlotIndex = FindAvailableSlot();
	if (SlotIndex == INDEX_NONE)
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Weapon Manager '%s' cannot create another weapon because all %d slots are occupied or reserved."), *GetNameSafe(this), MaxWeaponSlots);
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogStulWeaponSystem, Error, TEXT("Weapon Manager '%s' cannot create a weapon because its World is invalid."), *GetNameSafe(this));
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = OwnerActor;
	SpawnParameters.Instigator = Cast<APawn>(OwnerActor);
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStulWeapon* NewWeapon = World->SpawnActor<AStulWeapon>(WeaponClass, OwnerActor->GetActorTransform(), SpawnParameters);
	if (!NewWeapon)
	{
		UE_LOG(LogStulWeaponSystem, Error, TEXT("Weapon Manager '%s' failed to spawn weapon class '%s'."), *GetNameSafe(this), *GetNameSafe(WeaponClass.Get()));
		return nullptr;
	}
	const APawn* OwnerPawn = Cast<APawn>(OwnerActor);
	const APlayerController* OwnerPlayerController = OwnerPawn ? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;
	if (OwnerPlayerController && !OwnerPlayerController->IsLocalController()) NewWeapon->SetAutonomousProxy(true);

	const TWeakObjectPtr<AStulWeapon> WeaponKey(NewWeapon);
	PendingWeaponSlots.Add(WeaponKey, SlotIndex);
	if (bDefaultLoadoutWeapon) PendingDefaultLoadoutWeapons.Add(WeaponKey);
	NewWeapon->OnWeaponReady.AddUniqueDynamic(this, &ThisClass::HandleWeaponReady);
	NewWeapon->OnWeaponInitializationFailed.AddUniqueDynamic(this, &ThisClass::HandleWeaponInitializationFailed);
	NewWeapon->OnDestroyed.AddUniqueDynamic(this, &ThisClass::HandleWeaponDestroyed);

	const bool bInitializationStarted = ResolvedDefinition ? NewWeapon->InitializeFromDefinition(ResolvedDefinition) : NewWeapon->InitializeFromInstanceData(InstanceData);
	if (!bInitializationStarted && PendingWeaponSlots.Contains(WeaponKey)) HandleInitializationStartFailure(NewWeapon);
	return bInitializationStarted ? NewWeapon : nullptr;
}

void UStulWeaponManagerComponent::HandleInitializationStartFailure(AStulWeapon* Weapon)
{
	if (!Weapon) return;

	const TWeakObjectPtr<AStulWeapon> WeaponKey(Weapon);
	PendingWeaponSlots.Remove(WeaponKey);
	PendingDefaultLoadoutWeapons.Remove(WeaponKey);
	Weapon->OnWeaponReady.RemoveDynamic(this, &ThisClass::HandleWeaponReady);
	Weapon->OnWeaponInitializationFailed.RemoveDynamic(this, &ThisClass::HandleWeaponInitializationFailed);
	Weapon->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleWeaponDestroyed);
	Weapon->Destroy();
	CheckDefaultLoadoutReady();
}

void UStulWeaponManagerComponent::HandleWeaponReady(AStulWeapon* Weapon)
{
	if (!IsValid(Weapon)) return;

	const TWeakObjectPtr<AStulWeapon> WeaponKey(Weapon);
	const int32* ReservedSlot = PendingWeaponSlots.Find(WeaponKey);
	if (!ReservedSlot || *ReservedSlot < 0 || *ReservedSlot >= MaxWeaponSlots || WeaponList.GetWeaponAtSlot(*ReservedSlot))
	{
		UE_LOG(LogStulWeaponSystem, Error, TEXT("Weapon Manager '%s' received ready weapon '%s' without a valid reserved slot."), *GetNameSafe(this), *GetNameSafe(Weapon));
		HandleInitializationStartFailure(Weapon);
		return;
	}

	const int32 SlotIndex = *ReservedSlot;
	const bool bWasDefaultLoadoutWeapon = PendingDefaultLoadoutWeapons.Contains(WeaponKey);
	PendingWeaponSlots.Remove(WeaponKey);
	PendingDefaultLoadoutWeapons.Remove(WeaponKey);
	Weapon->OnWeaponReady.RemoveDynamic(this, &ThisClass::HandleWeaponReady);
	Weapon->OnWeaponInitializationFailed.RemoveDynamic(this, &ThisClass::HandleWeaponInitializationFailed);
	WeaponList.AddWeapon(Weapon, SlotIndex);

	UE_LOG(LogStulWeaponSystem, Verbose, TEXT("Weapon Manager '%s' added weapon '%s' to slot %d."), *GetNameSafe(this), *GetNameSafe(Weapon), SlotIndex);
	OnWeaponAdded.Broadcast(Weapon, SlotIndex);
	if (bAutoEquipFirstWeapon && !EquippedWeapon && GetWeaponCount() == 1) SetEquippedWeapon(Weapon);
	if (AActor* OwnerActor = GetOwner()) OwnerActor->ForceNetUpdate();
	if (bWasDefaultLoadoutWeapon) CheckDefaultLoadoutReady();
}

void UStulWeaponManagerComponent::HandleWeaponInitializationFailed(AStulWeapon* Weapon, const FText Reason)
{
	UE_LOG(LogStulWeaponSystem, Error, TEXT("Weapon Manager '%s' failed to initialize weapon '%s': %s"), *GetNameSafe(this), *GetNameSafe(Weapon), *Reason.ToString());
	HandleInitializationStartFailure(Weapon);
}

void UStulWeaponManagerComponent::HandleWeaponDestroyed(AActor* DestroyedActor)
{
	AStulWeapon* DestroyedWeapon = Cast<AStulWeapon>(DestroyedActor);
	if (!DestroyedWeapon) return;

	const TWeakObjectPtr<AStulWeapon> WeaponKey(DestroyedWeapon);
	const bool bWasPendingDefaultLoadoutWeapon = PendingDefaultLoadoutWeapons.Remove(WeaponKey) > 0;
	PendingWeaponSlots.Remove(WeaponKey);

	int32 SlotIndex = INDEX_NONE;
	if (WeaponList.RemoveWeapon(DestroyedWeapon, SlotIndex))
	{
		if (EquippedWeapon == DestroyedWeapon) SetEquippedWeapon(nullptr);
		OnWeaponRemoved.Broadcast(DestroyedWeapon, SlotIndex);
		if (AActor* OwnerActor = GetOwner()) OwnerActor->ForceNetUpdate();
	}

	if (bWasPendingDefaultLoadoutWeapon) CheckDefaultLoadoutReady();
}

bool UStulWeaponManagerComponent::RemoveWeapon(AStulWeapon* Weapon)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Weapon Manager '%s' rejected weapon removal because it must run on the server."), *GetNameSafe(this));
		return false;
	}
	if (!IsValid(Weapon))
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Weapon Manager '%s' cannot remove an invalid weapon."), *GetNameSafe(this));
		return false;
	}

	const int32 SlotIndex = WeaponList.GetSlotForWeapon(Weapon);
	if (SlotIndex == INDEX_NONE)
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Weapon Manager '%s' cannot remove weapon '%s' because it is not in the inventory."), *GetNameSafe(this), *GetNameSafe(Weapon));
		return false;
	}

	if (!Weapon->Destroy())
	{
		UE_LOG(LogStulWeaponSystem, Error, TEXT("Weapon Manager '%s' failed to destroy weapon '%s'."), *GetNameSafe(this), *GetNameSafe(Weapon));
		return false;
	}
	return true;
}

int32 UStulWeaponManagerComponent::FindAvailableSlot() const
{
	for (int32 SlotIndex = 0; SlotIndex < MaxWeaponSlots; ++SlotIndex)
	{
		if (WeaponList.GetWeaponAtSlot(SlotIndex)) continue;

		bool bSlotReserved = false;
		for (const TPair<TWeakObjectPtr<AStulWeapon>, int32>& PendingWeapon : PendingWeaponSlots)
		{
			if (PendingWeapon.Key.IsValid() && PendingWeapon.Value == SlotIndex)
			{
				bSlotReserved = true;
				break;
			}
		}
		if (!bSlotReserved) return SlotIndex;
	}

	return INDEX_NONE;
}

/*********************************************************************************************/
/************************************** Equipment ********************************************/
/*********************************************************************************************/

void UStulWeaponManagerComponent::EquipWeaponAtSlot(const int32 SlotIndex)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		UE_LOG(LogStulWeaponSystem, Error, TEXT("Weapon Manager '%s' cannot equip a weapon without an Owner."), *GetNameSafe(this));
		return;
	}

	if (OwnerActor->HasAuthority()) ApplyEquippedSlot(SlotIndex);
	else ServerEquipWeaponAtSlot(SlotIndex);
}

void UStulWeaponManagerComponent::HolsterWeapon()
{
	EquipWeaponAtSlot(INDEX_NONE);
}

void UStulWeaponManagerComponent::EquipNextWeapon()
{
	const int32 SlotIndex = FindAdjacentOccupiedSlot(1);
	if (SlotIndex != INDEX_NONE) EquipWeaponAtSlot(SlotIndex);
}

void UStulWeaponManagerComponent::EquipPreviousWeapon()
{
	const int32 SlotIndex = FindAdjacentOccupiedSlot(-1);
	if (SlotIndex != INDEX_NONE) EquipWeaponAtSlot(SlotIndex);
}

int32 UStulWeaponManagerComponent::FindAdjacentOccupiedSlot(const int32 Direction) const
{
	if (WeaponList.Num() == 0) return INDEX_NONE;

	const int32 CurrentSlot = GetEquippedWeaponSlot();
	const int32 StartSlot = CurrentSlot == INDEX_NONE ? (Direction > 0 ? MaxWeaponSlots - 1 : 0) : CurrentSlot;
	for (int32 Offset = 1; Offset <= MaxWeaponSlots; ++Offset)
	{
		const int32 SlotIndex = (StartSlot + Direction * Offset + MaxWeaponSlots) % MaxWeaponSlots;
		if (WeaponList.GetWeaponAtSlot(SlotIndex) && SlotIndex != CurrentSlot) return SlotIndex;
	}

	return INDEX_NONE;
}

void UStulWeaponManagerComponent::ServerEquipWeaponAtSlot_Implementation(const int32 SlotIndex)
{
	ApplyEquippedSlot(SlotIndex);
}

void UStulWeaponManagerComponent::ApplyEquippedSlot(const int32 SlotIndex)
{
	if (SlotIndex == INDEX_NONE)
	{
		SetEquippedWeapon(nullptr);
		return;
	}
	AStulWeapon* WeaponToEquip = WeaponList.GetWeaponAtSlot(SlotIndex);
	if (!IsValid(WeaponToEquip))
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Weapon Manager '%s' rejected invalid equipped slot %d."), *GetNameSafe(this), SlotIndex);
		return;
	}

	SetEquippedWeapon(WeaponToEquip);
}

void UStulWeaponManagerComponent::SetEquippedWeapon(AStulWeapon* NewWeapon)
{
	if (EquippedWeapon == NewWeapon) return;
	if (NewWeapon && !WeaponList.Contains(NewWeapon))
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Weapon Manager '%s' cannot equip weapon '%s' because it is not in the inventory."), *GetNameSafe(this), *GetNameSafe(NewWeapon));
		return;
	}

	AStulWeapon* PreviousWeapon = EquippedWeapon;
	if (PreviousWeapon) PreviousWeapon->CancelActiveActions();
	EquippedWeapon = NewWeapon;

	UE_LOG(LogStulWeaponSystem, Verbose, TEXT("Weapon Manager '%s' changed equipped weapon from '%s' to '%s'."), *GetNameSafe(this), *GetNameSafe(PreviousWeapon), *GetNameSafe(NewWeapon));
	OnWeaponEquipped.Broadcast(NewWeapon, PreviousWeapon);
	if (AActor* OwnerActor = GetOwner()) OwnerActor->ForceNetUpdate();
}

/*********************************************************************************************/
/**************************************** Input **********************************************/
/*********************************************************************************************/

void UStulWeaponManagerComponent::InputTagPressed(const FGameplayTag InputTag)
{
	if (EquippedWeapon) EquippedWeapon->InputTagPressed(InputTag);
}

void UStulWeaponManagerComponent::InputTagReleased(const FGameplayTag InputTag)
{
	if (EquippedWeapon) EquippedWeapon->InputTagReleased(InputTag);
}

void UStulWeaponManagerComponent::ProcessAbilityInput(const bool bGamePaused)
{
	if (EquippedWeapon) EquippedWeapon->ProcessAbilityInput(bGamePaused);
}

/*********************************************************************************************/
/*************************************** Getters *********************************************/
/*********************************************************************************************/

AStulWeapon* UStulWeaponManagerComponent::GetWeaponAtSlot(const int32 SlotIndex) const
{
	return WeaponList.GetWeaponAtSlot(SlotIndex);
}

int32 UStulWeaponManagerComponent::GetEquippedWeaponSlot() const
{
	return WeaponList.GetSlotForWeapon(EquippedWeapon);
}

int32 UStulWeaponManagerComponent::GetWeaponCount() const
{
	return WeaponList.Num();
}

float UStulWeaponManagerComponent::GetAimAlpha() const
{
	return EquippedWeapon ? EquippedWeapon->GetAimAlpha() : 0.0f;
}

bool UStulWeaponManagerComponent::IsFullyAimed() const
{
	return EquippedWeapon && EquippedWeapon->IsFullyAimed();
}

TArray<AStulWeapon*> UStulWeaponManagerComponent::GetWeapons() const
{
	TArray<AStulWeapon*> Result;
	Result.Reserve(WeaponList.Num());
	for (int32 SlotIndex = 0; SlotIndex < MaxWeaponSlots; ++SlotIndex)
	{
		if (AStulWeapon* Weapon = WeaponList.GetWeaponAtSlot(SlotIndex)) Result.Add(Weapon);
	}
	return Result;
}

/*********************************************************************************************/
/************************************* Replication *******************************************/
/*********************************************************************************************/

void UStulWeaponManagerComponent::HandleReplicatedWeaponAdded(AStulWeapon* Weapon, const int32 SlotIndex)
{
	if (!IsValid(Weapon))
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Weapon Manager '%s' received an invalid replicated weapon for slot %d."), *GetNameSafe(this), SlotIndex);
		return;
	}

	OnWeaponAdded.Broadcast(Weapon, SlotIndex);
}

void UStulWeaponManagerComponent::HandleReplicatedWeaponRemoved(AStulWeapon* Weapon, const int32 SlotIndex)
{
	OnWeaponRemoved.Broadcast(Weapon, SlotIndex);
}

void UStulWeaponManagerComponent::OnRep_EquippedWeapon(AStulWeapon* PreviousWeapon)
{
	if (PreviousWeapon == EquippedWeapon) return;

	if (PreviousWeapon) PreviousWeapon->CancelActiveActions();
	OnWeaponEquipped.Broadcast(EquippedWeapon, PreviousWeapon);
}

void UStulWeaponManagerComponent::OnRep_IsReady()
{
	if (bIsReady) OnWeaponManagerReady.Broadcast();
}
