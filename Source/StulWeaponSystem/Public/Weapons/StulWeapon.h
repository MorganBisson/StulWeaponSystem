#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Actor.h"
#include "GameplayAbilitySpecHandle.h"
#include "Weapons/Presentation/StulWeaponPresentationTypes.h"
#include "Weapons/Shooting/StulWeaponShootingTypes.h"
#include "Weapons/StulWeaponTypes.h"
#include "StulWeapon.generated.h"

class AStulWeapon;
class FLifetimeProperty;
class UAbilitySystemComponent;
class USkeletalMeshComponent;
class UStulWeaponAbilitySystemComponent;
class UStulWeaponAttributeSet;
class UStulWeaponChangeFireModeAbility;
class UStulWeaponDefinition;
struct FGameplayEventData;
struct FStreamableHandle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStulWeaponReadySignature, AStulWeapon*, Weapon);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStulWeaponInitializationFailedSignature, AStulWeapon*, Weapon, FText, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStulWeaponFireModeChangedSignature, FGameplayTag, PreviousFireMode, FGameplayTag, NewFireMode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStulWeaponFireModeChangeStateSignature, AStulWeapon*, Weapon, bool, bIsChangingFireMode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStulWeaponFiringSignature, AStulWeapon*, Weapon);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStulWeaponShotSignature, AStulWeapon*, Weapon, const FStulWeaponShotResult&, ShotResult);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStulWeaponAimStateChangedSignature, AStulWeapon*, Weapon, bool, bIsAiming);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStulWeaponReloadSignature, AStulWeapon*, Weapon);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStulWeaponReloadCommitSignature, AStulWeapon*, Weapon, int32, AmmoRestored);

/** Runtime representation of a weapon: visuals, attributes and weapon abilities. */
UCLASS(BlueprintType, Blueprintable)
class STULWEAPONSYSTEM_API AStulWeapon : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AStulWeapon();

	/************************ Actor ************************/
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/************************ Initialization ************************/
	/** Starts one-time initialization using the definition's default state. A valid Owner must already be assigned. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Stul Weapon System|Initialization")
	bool InitializeFromDefinition(UStulWeaponDefinition* InDefinition);
	/** Starts one-time restoration from persistent data. A valid Owner must already be assigned. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Stul Weapon System|Initialization")
	bool InitializeFromInstanceData(const FStulWeaponInstanceData& InstanceData);
	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Initialization")
	bool IsInitialized() const { return bInitialized; }

	/************************ Weapon ************************/
	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Weapon")
	UStulWeaponDefinition* GetWeaponDefinition() const { return WeaponDefinition; }
	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Weapon")
	FPrimaryAssetId GetWeaponDefinitionId() const { return WeaponDefinitionId; }
	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Weapon")
	USkeletalMeshComponent* GetWeaponMesh() const { return WeaponMesh; }
	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Weapon")
	UStulWeaponAttributeSet* GetWeaponAttributeSet() const { return WeaponAttributeSet; }
	/** Captures the persistent state of an initialized weapon on the server. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Stul Weapon System|Weapon")
	bool TryMakeInstanceData(FStulWeaponInstanceData& OutInstanceData) const;
	/** Presentation transform from the loaded mesh socket. It is not the authoritative hitscan origin. */
	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Weapon")
	FTransform GetMuzzleTransform() const;
	/** Returns the effective presentation for this runtime weapon. Projects may override this to account for attachments or mods. */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Stul Weapon System|Presentation")
	bool GetPresentationData(FStulWeaponPresentationData& OutPresentation) const;
	virtual bool GetPresentationData_Implementation(FStulWeaponPresentationData& OutPresentation) const;

	/************************ Aim ************************/
	/** Returns effective aim configuration. Future sight customizations may override this resolution point. */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Stul Weapon System|Aim")
	bool GetAimData(FStulWeaponAimData& OutAimData) const;
	virtual bool GetAimData_Implementation(FStulWeaponAimData& OutAimData) const;
	/** Returns the current effective aim socket transform, with Actor transform as a safe fallback. */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Stul Weapon System|Aim")
	FTransform GetAimTransform() const;
	virtual FTransform GetAimTransform_Implementation() const;
	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Aim")
	bool IsAiming() const;
	/** Returns the canonical transition alpha, computed independently on the client and server. */
	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Aim")
	float GetAimAlpha() const;
	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Aim")
	bool IsFullyAimed() const;
	/** Cancels transient actions and resets aim state when this weapon is unequipped. */
	void CancelActiveActions();

	/************************ Reload ************************/
	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Reload")
	bool IsReloading() const;

	/************************ Fire Mode ************************/
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Stul Weapon System|Fire Mode")
	bool SetCurrentFireMode(FGameplayTag NewFireMode);
	/** Cycles immediately on authority or sends an authoritative request from the owning client. */
	UFUNCTION(BlueprintCallable, Category = "Stul Weapon System|Fire Mode")
	bool CycleFireMode();
	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Fire Mode")
	bool GetNextFireMode(FGameplayTag& OutFireMode) const;
	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Fire Mode")
	FGameplayTag GetCurrentFireMode() const { return CurrentFireModeTag; }
	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Fire Mode")
	bool IsChangingFireMode() const;

	/************************ Input ************************/
	/** Forwards a project input tag to matching weapon ability specs. */
	UFUNCTION(BlueprintCallable, Category = "Stul Weapon System|Input", meta = (Categories = "Stul.Weapon.Input"))
	void InputTagPressed(FGameplayTag InputTag);
	UFUNCTION(BlueprintCallable, Category = "Stul Weapon System|Input", meta = (Categories = "Stul.Weapon.Input"))
	void InputTagReleased(FGameplayTag InputTag);
	/** Processes queued input once after the owning project has gathered input for the frame. */
	UFUNCTION(BlueprintCallable, Category = "Stul Weapon System|Input")
	void ProcessAbilityInput(bool bGamePaused);
	/** Releases active input events and clears every queued input handle. */
	UFUNCTION(BlueprintCallable, Category = "Stul Weapon System|Input")
	void ClearAbilityInput();

	/************************ Shooting ************************/
	/** Runtime notifications used by presentation systems such as recoil, animation and audio. */
	void NotifyFiringStarted();
	void NotifyShotExecuted(const FStulWeaponShotResult& ShotResult);
	void NotifyFiringEnded();
	/** Executes the one-shot muzzle presentation cue. Called once for a multi-projectile discharge. */
	void ExecuteFireGameplayCue(const FStulWeaponShotResult& ShotResult);
	/** Executes presentation for one hitscan path. RawMagnitude contains its cosmetic path length. */
	void ExecuteTracerGameplayCue(const FStulWeaponShotResult& ShotResult);
	/** Executes presentation for one impact. RawMagnitude contains the shot damage. */
	void ExecuteImpactGameplayCue(const FHitResult& HitResult, float Damage, AActor* EffectCauser);

	/************************ Events ************************/
	UPROPERTY(BlueprintAssignable, Category = "Stul Weapon System|Events")
	FStulWeaponReadySignature OnWeaponReady;
	UPROPERTY(BlueprintAssignable, Category = "Stul Weapon System|Events")
	FStulWeaponInitializationFailedSignature OnWeaponInitializationFailed;
	UPROPERTY(BlueprintAssignable, Category = "Stul Weapon System|Events")
	FStulWeaponFireModeChangedSignature OnFireModeChanged;
	UPROPERTY(BlueprintAssignable, Category = "Stul Weapon System|Events")
	FStulWeaponFireModeChangeStateSignature OnFireModeChangeStateChanged;
	UPROPERTY(BlueprintAssignable, Category = "Stul Weapon System|Events")
	FStulWeaponFiringSignature OnFiringStarted;
	UPROPERTY(BlueprintAssignable, Category = "Stul Weapon System|Events")
	FStulWeaponShotSignature OnShotExecuted;
	UPROPERTY(BlueprintAssignable, Category = "Stul Weapon System|Events")
	FStulWeaponFiringSignature OnFiringEnded;
	UPROPERTY(BlueprintAssignable, Category = "Stul Weapon System|Events")
	FStulWeaponAimStateChangedSignature OnAimStateChanged;
	UPROPERTY(BlueprintAssignable, Category = "Stul Weapon System|Events")
	FStulWeaponReloadSignature OnReloadStarted;
	UPROPERTY(BlueprintAssignable, Category = "Stul Weapon System|Events")
	FStulWeaponReloadCommitSignature OnReloadCommit;
	UPROPERTY(BlueprintAssignable, Category = "Stul Weapon System|Events")
	FStulWeaponReloadSignature OnReloadCompleted;
	UPROPERTY(BlueprintAssignable, Category = "Stul Weapon System|Events")
	FStulWeaponReloadSignature OnReloadCancelled;

protected:
	/************************ Actor ************************/
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnRep_Owner() override;

private:
	/************************ Initialization ************************/
	bool BeginInitializationRequest();
	bool InitializeFromResolvedDefinition(UStulWeaponDefinition* InDefinition, const FStulWeaponInstanceData& InstanceData);
	/** Establishes GAS actor info and consequently dispatches OnAvatarSet to granted abilities. */
	void InitializeAbilityActorInfo();
	void TryFinishInitialization();
	void CancelPendingLoads();
	void ApplyDefinitionAttributes(const FStulWeaponInstanceData& InstanceData);
	void GrantDefinitionAbilities();
	void ClearGrantedAbilities();
	bool BeginRuntimeAssetLoad();
	void FinishRuntimeAssetLoad();
	void FinishDefinitionLoad(FStulWeaponInstanceData InstanceData);
	void FinishReplicatedDefinitionLoad(FPrimaryAssetId RequestedDefinitionId);
	void FailInitialization(const FText& Reason);

	/************************ Aim ************************/
	void HandleAimStateTagChanged(const FGameplayTag CallbackTag, int32 NewCount);
	void BeginAimTransition(bool bNewAiming);
	void ResetAimState();

	/************************ Reload ************************/
	void HandleReloadStateTagChanged(const FGameplayTag CallbackTag, int32 NewCount);
	void HandleReloadGameplayEvent(const FGameplayEventData* Payload, FGameplayTag EventTag);

	/************************ Fire Mode ************************/
	bool CommitFireModeChange(FGameplayTag NewFireMode);
	bool ApplyPredictedFireMode(FGameplayTag NewFireMode);
	bool RestoreAuthoritativeFireMode();
	void HandleFireModeChangeStateTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

	/************************ Replication ************************/
	UFUNCTION(Server, Reliable)
	void ServerCycleFireMode();
	UFUNCTION()
	void OnRep_WeaponDefinitionId();
	UFUNCTION()
	void OnRep_CurrentFireModeTag(const FGameplayTag& PreviousFireMode);

	/************************ Components ************************/
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStulWeaponAbilitySystemComponent> AbilitySystemComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStulWeaponAttributeSet> WeaponAttributeSet;

	/************************ Definition ************************/
	/** Locally resolved definition kept alive after loading; the asset pointer itself is never replicated. */
	UPROPERTY(Transient)
	TObjectPtr<UStulWeaponDefinition> WeaponDefinition;
	UPROPERTY(ReplicatedUsing = OnRep_WeaponDefinitionId)
	FPrimaryAssetId WeaponDefinitionId;

	/************************ Runtime State ************************/
	UPROPERTY(ReplicatedUsing = OnRep_CurrentFireModeTag)
	FGameplayTag CurrentFireModeTag;
	FGameplayTag AuthoritativeFireModeTag;
	UPROPERTY(Transient)
	FStulWeaponInstanceData RuntimeInstanceData;
	TArray<FGameplayAbilitySpecHandle> GrantedAbilityHandles;
	TSharedPtr<FStreamableHandle> DefinitionLoadHandle;
	TSharedPtr<FStreamableHandle> RuntimeAssetsLoadHandle;
	bool bAbilityActorInfoInitialized = false;
	bool bInitializationStarted = false;
	bool bRuntimeAssetsLoaded = false;
	bool bInitialized = false;
	float AimTransitionStartAlpha = 0.0f;
	float AimTransitionTargetAlpha = 0.0f;
	float AimTransitionStartTime = 0.0f;
	float AimTransitionDuration = 0.0f;

	friend class UStulWeaponChangeFireModeAbility;
};
