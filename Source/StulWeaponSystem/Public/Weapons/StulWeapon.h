#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Actor.h"
#include "GameplayCueInterface.h"
#include "GameplayAbilitySpecHandle.h"
#include "Weapons/Presentation/StulWeaponPresentationTypes.h"
#include "Weapons/Shooting/StulWeaponShootingTypes.h"
#include "Weapons/StulWeaponTypes.h"
#include "StulWeapon.generated.h"

class AStulWeapon;
class AStulWeaponProjectile;
class AController;
class APawn;
class FLifetimeProperty;
class UAbilitySystemComponent;
class USkeletalMeshComponent;
class UStulWeaponAbilitySystemComponent;
class UStulAmmoDefinition;
class UStulWeaponAttributeSet;
class UStulWeaponChangeFireModeAbility;
class UStulWeaponDefinition;
class UStulWeaponFireAbility;
class UStulWeaponFireComponent;
class UStulWeaponAimAbility;
struct FStreamableHandle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStulWeaponReadySignature, AStulWeapon*, Weapon);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStulWeaponInitializationFailedSignature, AStulWeapon*, Weapon, FText, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStulWeaponFireModeChangedSignature, FGameplayTag, PreviousFireMode, FGameplayTag, NewFireMode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStulWeaponFireModeChangeStateSignature, AStulWeapon*, Weapon, bool, bIsChangingFireMode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStulWeaponFiringSignature, AStulWeapon*, Weapon);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStulWeaponShotSignature, AStulWeapon*, Weapon, const FStulWeaponShotExecution&, ShotExecution);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FStulWeaponAuthoritativeHitSignature, AStulWeapon*, Weapon, const FHitResult&, HitResult, float, Damage, AActor*, EffectCauser, UStulAmmoDefinition*, AmmoDefinition);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStulWeaponAimStateChangedSignature, AStulWeapon*, Weapon, bool, bIsAiming);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStulWeaponReloadSignature, AStulWeapon*, Weapon);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStulWeaponReloadCommitSignature, AStulWeapon*, Weapon, int32, AmmoRestored);

/** Runtime representation of a weapon: visuals, attributes and weapon abilities. */
UCLASS(BlueprintType, Blueprintable)
class STULWEAPONSYSTEM_API AStulWeapon : public AActor, public IAbilitySystemInterface, public IGameplayCueInterface
{
	GENERATED_BODY()

public:
	AStulWeapon();

	/************************ Actor ************************/
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void HandleGameplayCue(UObject* Self, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters) override;
	virtual void SetOwner(AActor* NewOwner) override;
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
	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Weapon")
	UStulWeaponFireComponent* GetFireComponent() const { return FireComponent; }
	/** Server-generated immutable seed shared with the owning client for deterministic fire prediction. */
	uint32 GetBallisticSeed() const { return WeaponBallisticSeed; }
	bool CanPredictWeaponFire() const { return WeaponBallisticSeed != 0; }
	/** Returns the ammunition payload used for newly executed shots. */
	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Weapon")
	UStulAmmoDefinition* GetCurrentAmmoDefinition() const;
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
	/** Returns the effective hitscan tracer presentation. Projects may override this for runtime weapon modifications. */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Stul Weapon System|Presentation")
	bool GetHitscanTracerData(FStulWeaponTracerData& OutTracer) const;
	virtual bool GetHitscanTracerData_Implementation(FStulWeaponTracerData& OutTracer) const;
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
	/** Shared server-only hit pipeline used by hitscan and projectile shots. */
	void HandleAuthoritativeHit(const FHitResult& HitResult, float Damage, AActor* EffectCauser, UStulAmmoDefinition* AmmoDefinition = nullptr, bool bExecutePresentation = true);
	/** Executes presentation for one impact. RawMagnitude contains the shot damage. */
	void ExecuteImpactGameplayCue(const FHitResult& HitResult, float Damage, AActor* EffectCauser, UStulAmmoDefinition* AmmoDefinition = nullptr);

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
	FStulWeaponFiringSignature OnLocalFiringStarted;
	UPROPERTY(BlueprintAssignable, Category = "Stul Weapon System|Events")
	FStulWeaponShotSignature OnLocalShotExecuted;
	UPROPERTY(BlueprintAssignable, Category = "Stul Weapon System|Events")
	FStulWeaponFiringSignature OnLocalFiringEnded;
	UPROPERTY(BlueprintAssignable, Category = "Stul Weapon System|Events")
	FStulWeaponAuthoritativeHitSignature OnAuthoritativeHit;
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
	void UninitializeAbilitySystem();
	void RefreshNetworkRoleFromOwner();
	UFUNCTION()
	void HandleOwnerControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);
	void TryFinishInitialization();
	void CancelPendingLoads();
	void CollectRuntimeAssetPaths(TArray<FSoftObjectPath>& OutPaths) const;
	void ApplyDefinitionAttributes(const FStulWeaponInstanceData& InstanceData);
	void GrantDefinitionAbilities();
	void ClearGrantedAbilities();
	bool BeginRuntimeAssetLoad();
	void FinishRuntimeAssetLoad();
	void FinishDefinitionLoad(FStulWeaponInstanceData InstanceData);
	void FinishReplicatedDefinitionLoad(FPrimaryAssetId RequestedDefinitionId);
	void FailInitialization(const FText& Reason);

	/************************ Presentation ************************/
	void SetPresentationCueActive(FGameplayTag GameplayCueTag, bool bActive);
	bool HasPresentationOrGameplayState(FGameplayTag GameplayStateTag, FGameplayTag PresentationCueTag) const;
	/** Executes the one-shot muzzle presentation cue. Called once for a multi-projectile discharge. */
	void ExecuteFireGameplayCue(const FStulWeaponShotResult& ShotResult);
	/** Executes presentation for one hitscan path. RawMagnitude contains its cosmetic path length. */
	void ExecuteTracerGameplayCue(const FStulWeaponShotResult& ShotResult);

	/************************ Shooting ************************/
	void HandleFiringStarted();
	void HandleShotExecuted(const FStulWeaponShotExecution& ShotExecution);
	void HandleFiringEnded();
	void RegisterPredictedProjectile(const FStulProjectileId& ProjectileId, AStulWeaponProjectile* Projectile);
	AStulWeaponProjectile* FindPredictedProjectile(const FStulProjectileId& ProjectileId) const;
	void ConfirmPredictedShot(const FStulShotId& ShotId);
	void RejectPredictedShot(const FStulShotId& ShotId);
	void UnregisterPredictedProjectile(const FStulProjectileId& ProjectileId, const AStulWeaponProjectile* Projectile);
	void ClearPredictedProjectiles();

	/************************ Aim ************************/
	void SetAimPresentationActive(bool bActive);
	void BeginAimTransition(bool bNewAiming);
	void ResetAimState();

	/************************ Fire Mode ************************/
	bool CommitFireModeChange(FGameplayTag NewFireMode);
	bool ApplyPredictedFireMode(FGameplayTag NewFireMode);
	bool RestoreAuthoritativeFireMode();

	/************************ Replication ************************/
	UFUNCTION()
	void OnRep_WeaponDefinitionId();
	UFUNCTION()
	void OnRep_CurrentFireModeTag(const FGameplayTag& PreviousFireMode);
	UFUNCTION()
	void OnRep_WeaponBallisticSeed();

	/************************ Components ************************/
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStulWeaponAbilitySystemComponent> AbilitySystemComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStulWeaponAttributeSet> WeaponAttributeSet;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStulWeaponFireComponent> FireComponent;

	/************************ Definition ************************/
	/** Locally resolved definition kept alive after loading; the asset pointer itself is never replicated. */
	UPROPERTY(Transient)
	TObjectPtr<UStulWeaponDefinition> WeaponDefinition;
	UPROPERTY(ReplicatedUsing = OnRep_WeaponDefinitionId)
	FPrimaryAssetId WeaponDefinitionId;

	/************************ Runtime State ************************/
	UPROPERTY(ReplicatedUsing = OnRep_CurrentFireModeTag)
	FGameplayTag CurrentFireModeTag;
	/** Owner-only because only the predicting owner needs to reconstruct future ballistic directions. */
	UPROPERTY(ReplicatedUsing = OnRep_WeaponBallisticSeed)
	uint32 WeaponBallisticSeed = 0;
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
	TWeakObjectPtr<APawn> BoundAbilityOwnerPawn;
	FGameplayTagContainer ActivePresentationCues;
	TMap<FStulProjectileId, TWeakObjectPtr<AStulWeaponProjectile>> PredictedProjectiles;
	float AimTransitionStartAlpha = 0.0f;
	float AimTransitionTargetAlpha = 0.0f;
	float AimTransitionStartTime = 0.0f;
	float AimTransitionDuration = 0.0f;

	friend class UStulWeaponChangeFireModeAbility;
	friend class UStulWeaponFireAbility;
	friend class UStulWeaponFireComponent;
	friend class UStulWeaponAimAbility;
	friend class AStulWeaponProjectile;
};
