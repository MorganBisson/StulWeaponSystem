#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "Weapons/StulWeaponTypes.h"
#include "StulWeaponManagerComponent.generated.h"

class AActor;
class AStulWeapon;
class FLifetimeProperty;
class UStulWeaponManagerComponent;
class UStulWeaponDefinition;

/************************ Replicated Weapon List ************************/
/** One occupied weapon slot replicated by FStulWeaponList. */
USTRUCT()
struct STULWEAPONSYSTEM_API FStulWeaponEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<AStulWeapon> Weapon;

	UPROPERTY()
	int32 SlotIndex = INDEX_NONE;

	/** Prevents duplicate client events when a replicated Actor reference is resolved after the entry arrives. */
	UPROPERTY(NotReplicated, Transient)
	bool bAddedEventBroadcast = false;
};

/** Delta-replicated collection of occupied weapon slots. */
USTRUCT()
struct STULWEAPONSYSTEM_API FStulWeaponList : public FFastArraySerializer
{
	GENERATED_BODY()

	/************************ Serialization ************************/
	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams);
	void PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);

	/************************ Weapon Entries ************************/
	void SetOwner(UStulWeaponManagerComponent* InOwnerComponent);
	void AddWeapon(AStulWeapon* Weapon, int32 SlotIndex);
	bool RemoveWeapon(AStulWeapon* Weapon, int32& OutSlotIndex);
	AStulWeapon* GetWeaponAtSlot(int32 SlotIndex) const;
	int32 GetSlotForWeapon(const AStulWeapon* Weapon) const;
	bool Contains(const AStulWeapon* Weapon) const;
	int32 Num() const { return Entries.Num(); }
	const TArray<FStulWeaponEntry>& GetEntries() const { return Entries; }

private:
	UPROPERTY()
	TArray<FStulWeaponEntry> Entries;

	UPROPERTY(NotReplicated, Transient)
	TObjectPtr<UStulWeaponManagerComponent> OwnerComponent;
};

template<>
struct TStructOpsTypeTraits<FStulWeaponList> : public TStructOpsTypeTraitsBase2<FStulWeaponList>
{
	enum { WithNetDeltaSerializer = true };
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FStulWeaponManagerReadySignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStulWeaponEquippedSignature, AStulWeapon*, NewWeapon, AStulWeapon*, PreviousWeapon);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStulWeaponAddedSignature, AStulWeapon*, Weapon, int32, SlotIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStulWeaponRemovedSignature, AStulWeapon*, Weapon, int32, SlotIndex);

/** Server-authoritative weapon inventory, equipment state and input router. */
UCLASS(ClassGroup = (StulWeaponSystem), meta = (BlueprintSpawnableComponent))
class STULWEAPONSYSTEM_API UStulWeaponManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStulWeaponManagerComponent();

	/************************ Initialization ************************/
	/** Initializes the configured loadout once. Call on the server after the owning Pawn is ready and possessed. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Stul Weapon System|Initialization")
	void InitializeDefaultLoadout();

	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Initialization")
	bool IsReady() const { return bIsReady; }

	/************************ Weapon Management ************************/
	/** Spawns and initializes a new weapon using its definition defaults. Returns null when no slot is available. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Stul Weapon System|Weapon Management")
	AStulWeapon* GiveWeaponFromDefinition(UStulWeaponDefinition* WeaponDefinition);

	/** Spawns and restores a weapon from persistent instance data. Returns null when no slot is available. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Stul Weapon System|Weapon Management")
	AStulWeapon* GiveWeaponFromInstanceData(const FStulWeaponInstanceData& InstanceData);

	/** Removes and destroys a weapon owned by this Manager. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Stul Weapon System|Weapon Management")
	bool RemoveWeapon(AStulWeapon* Weapon);

	/************************ Equipment ************************/
	/** Requests the server to equip the weapon stored in the specified slot. */
	UFUNCTION(BlueprintCallable, Category = "Stul Weapon System|Equipment")
	void EquipWeaponAtSlot(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Stul Weapon System|Equipment")
	void HolsterWeapon();

	UFUNCTION(BlueprintCallable, Category = "Stul Weapon System|Equipment")
	void EquipNextWeapon();

	UFUNCTION(BlueprintCallable, Category = "Stul Weapon System|Equipment")
	void EquipPreviousWeapon();

	/************************ Input ************************/
	/** Forwards a semantic input press to the equipped weapon. */
	UFUNCTION(BlueprintCallable, Category = "Stul Weapon System|Input", meta = (Categories = "Stul.Weapon.Input"))
	void InputTagPressed(FGameplayTag InputTag);

	/** Forwards a semantic input release to the equipped weapon. */
	UFUNCTION(BlueprintCallable, Category = "Stul Weapon System|Input", meta = (Categories = "Stul.Weapon.Input"))
	void InputTagReleased(FGameplayTag InputTag);

	/** Processes the equipped weapon input once after the project has gathered input for the frame. */
	UFUNCTION(BlueprintCallable, Category = "Stul Weapon System|Input")
	void ProcessAbilityInput(bool bGamePaused);

	/************************ Getters ************************/
	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Weapon Management")
	AStulWeapon* GetEquippedWeapon() const { return EquippedWeapon; }

	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Weapon Management")
	AStulWeapon* GetWeaponAtSlot(int32 SlotIndex) const;

	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Weapon Management")
	int32 GetEquippedWeaponSlot() const;

	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Weapon Management")
	int32 GetWeaponCount() const;

	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Weapon Management")
	int32 GetMaxWeaponSlots() const { return MaxWeaponSlots; }
	/** Convenience accessor; the equipped weapon remains the source of truth. */
	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Aim")
	float GetAimAlpha() const;
	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Aim")
	bool IsFullyAimed() const;

	/** Returns the occupied weapons ordered by slot index. Empty slots are omitted. */
	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Weapon Management")
	TArray<AStulWeapon*> GetWeapons() const;

	/************************ Events ************************/
	UPROPERTY(BlueprintAssignable, Category = "Stul Weapon System|Events")
	FStulWeaponManagerReadySignature OnWeaponManagerReady;

	UPROPERTY(BlueprintAssignable, Category = "Stul Weapon System|Events")
	FStulWeaponEquippedSignature OnWeaponEquipped;

	UPROPERTY(BlueprintAssignable, Category = "Stul Weapon System|Events")
	FStulWeaponAddedSignature OnWeaponAdded;

	UPROPERTY(BlueprintAssignable, Category = "Stul Weapon System|Events")
	FStulWeaponRemovedSignature OnWeaponRemoved;

protected:
	/************************ Actor Component ************************/
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/************************ Configuration ************************/
	/** Runtime actor class used for every weapon spawned by this Manager. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stul Weapon System|Configuration")
	TSubclassOf<AStulWeapon> WeaponClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stul Weapon System|Configuration", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxWeaponSlots = 2;

	/** Primary Asset IDs spawned by InitializeDefaultLoadout. Invalid or overflowing entries are skipped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stul Weapon System|Configuration", meta = (AllowedTypes = "StulWeaponDefinition"))
	TArray<FPrimaryAssetId> DefaultWeaponLoadout;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stul Weapon System|Configuration")
	bool bAutoEquipFirstWeapon = true;

private:
	/************************ Internal Weapon Management ************************/
	AStulWeapon* SpawnAndInitializeWeapon(UStulWeaponDefinition* ResolvedDefinition, const FStulWeaponInstanceData& InstanceData, bool bDefaultLoadoutWeapon);
	int32 FindAvailableSlot() const;
	int32 FindAdjacentOccupiedSlot(int32 Direction) const;
	void HandleInitializationStartFailure(AStulWeapon* Weapon);
	void CheckDefaultLoadoutReady();
	void ApplyEquippedSlot(int32 SlotIndex);
	void SetEquippedWeapon(AStulWeapon* NewWeapon);
	void HandleReplicatedWeaponAdded(AStulWeapon* Weapon, int32 SlotIndex);
	void HandleReplicatedWeaponRemoved(AStulWeapon* Weapon, int32 SlotIndex);

	UFUNCTION()
	void HandleWeaponReady(AStulWeapon* Weapon);

	UFUNCTION()
	void HandleWeaponInitializationFailed(AStulWeapon* Weapon, FText Reason);
	UFUNCTION()
	void HandleWeaponDestroyed(AActor* DestroyedActor);

	/************************ Replication ************************/
	UFUNCTION(Server, Reliable)
	void ServerEquipWeaponAtSlot(int32 SlotIndex);

	UFUNCTION()
	void OnRep_EquippedWeapon(AStulWeapon* PreviousWeapon);

	UFUNCTION()
	void OnRep_IsReady();

	UPROPERTY(Replicated)
	FStulWeaponList WeaponList;

	UPROPERTY(ReplicatedUsing = OnRep_EquippedWeapon)
	TObjectPtr<AStulWeapon> EquippedWeapon;

	UPROPERTY(ReplicatedUsing = OnRep_IsReady)
	bool bIsReady = false;

	/************************ Runtime State ************************/
	TMap<TWeakObjectPtr<AStulWeapon>, int32> PendingWeaponSlots;
	TSet<TWeakObjectPtr<AStulWeapon>> PendingDefaultLoadoutWeapons;

	bool bDefaultLoadoutInitializationStarted = false;
	bool bDefaultLoadoutSchedulingComplete = false;

	friend struct FStulWeaponList;
};
