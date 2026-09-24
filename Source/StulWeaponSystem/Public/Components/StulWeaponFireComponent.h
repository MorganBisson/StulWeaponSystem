#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "Weapons/Shooting/StulWeaponFireSessionTypes.h"
#include "Weapons/Shooting/StulWeaponShootingTypes.h"
#include "StulWeaponFireComponent.generated.h"

class AStulWeapon;
class APawn;
class FLifetimeProperty;
class UStulWeaponDefinition;
struct FStulWeaponProjectileInitData;

DECLARE_DELEGATE_RetVal_OneParam(bool, FStulAuthorizeWeaponShot, const FStulShotId& /* ShotId */);
DECLARE_DELEGATE_OneParam(FStulFireSessionEnded, bool /* bWasCancelled */);

/** Owns weapon fire timing, shot identities, transport, reconciliation and shot execution. */
UCLASS(ClassGroup = (StulWeaponSystem), meta = (BlueprintSpawnableComponent))
class STULWEAPONSYSTEM_API UStulWeaponFireComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStulWeaponFireComponent();

	/************************ Component ************************/
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/************************ Fire Lifecycle ************************/
	bool StartSingle();
	bool StartBurst();
	bool StartAutomatic();
	void RequestStopAutomatic();
	void InterruptActiveSession();
	/** Replicates one server-authoritative projectile impact through the fire presentation transport. */
	void PresentAuthoritativeProjectileImpact(const FHitResult& HitResult, float Damage, AActor* EffectCauser);

	/************************ Ability Authorization ************************/
	bool RegisterAbilityAuthorization(EStulFireSessionType SessionType, FGameplayAbilitySpecHandle AbilityHandle, UObject* AbilityInstance, FStulAuthorizeWeaponShot AuthorizeShot, FStulFireSessionEnded SessionEnded);
	void UnregisterAbilityAuthorization(UObject* AbilityInstance, bool bWasCancelled);

	/************************ State ************************/
	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Fire")
	bool IsProducingShots() const;
	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Fire")
	int32 GetDisplayedAmmo() const;
	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Fire")
	AStulWeapon* GetWeapon() const;
	APawn* GetOwningPawn() const;
	EStulFireExecutionRole ResolveExecutionRole() const;

protected:
	/************************ Component ************************/
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/************************ Network ************************/
	UFUNCTION(Server, Reliable)
	void ServerOpenFireSession(const FStulOpenFireSessionRequest& Request);
	UFUNCTION(Server, Unreliable)
	void ServerReceiveShotBatch(const FStulShotBatch& Batch);
	UFUNCTION(Server, Reliable)
	void ServerCloseFireSession(const FStulCloseFireSessionRequest& Request);
	UFUNCTION(Client, Unreliable)
	void ClientReceiveFireAck(const FStulFireAck& Ack);
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPresentAuthoritativeShot(const FStulWeaponShotExecution& ShotExecution);
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPresentHitscanImpact(const FHitResult& HitResult, float Damage, AActor* EffectCauser, bool bSkipOwningClient);
	UFUNCTION()
	void OnRep_OwnerReconciliationState();

	/************************ Validation ************************/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Validation", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxClientViewOriginError = 150.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Validation", meta = (ClampMin = "0.0", Units = "s"))
	float MissingCommandGraceSeconds = 0.15f;

	/************************ Transport ************************/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Network", meta = (ClampMin = "1", ClampMax = "16"))
	int32 RedundantCommandCount = 4;

	/************************ Debug ************************/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Debug")
	bool bDrawDebugTraces = false;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Debug", meta = (ClampMin = "0.0", Units = "s", EditCondition = "bDrawDebugTraces"))
	float DebugDrawDuration = 2.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Debug", meta = (ClampMin = "0.0", Units = "cm", EditCondition = "bDrawDebugTraces"))
	float DebugImpactRadius = 8.0f;
	/** Zero uses Unreal's thinnest debug-line rendering. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Debug", meta = (ClampMin = "0.0", EditCondition = "bDrawDebugTraces"))
	float DebugLineThickness = 0.0f;

private:
	static constexpr uint32 MaxSequenceLead = 64;

	struct FAbilityAuthorization
	{
		FGameplayAbilitySpecHandle AbilityHandle;
		EStulFireSessionType SessionType = EStulFireSessionType::Single;
		TWeakObjectPtr<UObject> AbilityInstance;
		FStulAuthorizeWeaponShot AuthorizeShot;
		FStulFireSessionEnded SessionEnded;

		bool IsValid() const { return AbilityHandle.IsValid() && AbilityInstance.IsValid() && AuthorizeShot.IsBound(); }
		void Reset();
	};

	struct FLocalFireSession
	{
		EStulFireSessionState State = EStulFireSessionState::None;
		EStulFireSessionType Type = EStulFireSessionType::Single;
		EStulFireExecutionRole ExecutionRole = EStulFireExecutionRole::PresentationOnly;
		uint32 SessionId = 0;
		uint32 NextSequence = 1;
		int32 ShotsRemaining = 0;
		float FireInterval = 0.0f;
		double NextShotTime = 0.0;
		TArray<FStulShotCommand> CommandHistory;
		FTimerHandle ShotTimer;

		void Reset();
	};

	struct FServerFireSession
	{
		EStulFireSessionState State = EStulFireSessionState::None;
		EStulFireSessionType Type = EStulFireSessionType::Single;
		uint32 SessionId = 0;
		uint32 ResolvedThroughSequence = 0;
		uint32 FinalSequence = 0;
		uint32 AckRevision = 0;
		uint64 AcceptedThroughMask = 0;
		uint32 WaitingForMissingSequence = 0;
		float FireInterval = 0.0f;
		double NextShotTime = 0.0;
		TMap<uint32, FStulShotCommand> BufferedCommands;
		FTimerHandle ProcessTimer;

		void Reset();
	};

	/************************ Fire Lifecycle ************************/
	bool StartSession(EStulFireSessionType SessionType);
	void ProduceLocalShot();
	void ScheduleLocalShot();
	void CloseLocalSession(bool bWasCancelled);
	void NotifyAbilitySessionEnded(bool bWasCancelled);
	float GetFireInterval() const;
	int32 GetBurstSize() const;
	static double AdvanceShotTime(double PreviousShotTime, float FireInterval);
	static float GetShotScheduleDelay(double ShotTime, double CurrentTime);

	/************************ Session Opening ************************/
	void TryCompleteServerOpening();
	bool ValidateServerOpening(const FStulOpenFireSessionRequest& Request) const;
	void StartServerSession(const FStulOpenFireSessionRequest& Request);

	/************************ Shot Transport ************************/
	void SendCurrentShotBatch();
	FStulCloseFireSessionRequest MakeCloseRequest() const;
	void BufferServerBatch(const FStulShotBatch& Batch);
	void BufferServerCommands(const TArray<FStulShotCommand>& Commands);
	void ProcessServerCommands();
	void ScheduleServerProcessing(float Delay);
	void HandleServerProcessingTimer();
	bool ResolveServerCommand(const FStulShotCommand& Command);
	void ResolveMissingServerCommand();
	void FinalizeServerSession(EStulFireSessionFinalState FinalState);

	/************************ Reconciliation ************************/
	void SendServerAck();
	void ApplyFireAck(const FStulFireAck& Ack);
	void ResolvePendingShot(const FStulShotId& ShotId, bool bAccepted);
	int32 GetAuthoritativeAmmoSnapshot() const;

	/************************ Shot Execution ************************/
	bool ExecuteShot(const FStulShotId& ShotId, const FVector& ViewOrigin, const FVector& ViewDirection, bool bAuthoritative, FStulWeaponShotExecution& OutShotExecution);
	void ExecuteHitscanShot(const UStulWeaponDefinition& Definition, const FVector& ViewOrigin, const TArray<AActor*>& IgnoredActors, FStulWeaponShotResult& ShotResult) const;
	FStulWeaponProjectileInitData MakeProjectileInitData(const UStulWeaponDefinition& Definition, const FStulProjectileId& ProjectileId, const FStulWeaponShotResult& ShotResult) const;
	bool SpawnPredictedProjectile(const UStulWeaponDefinition& Definition, AStulWeapon& Weapon, const FVector& ViewOrigin, const FStulProjectileId& ProjectileId, const FStulWeaponShotResult& ShotResult) const;
	bool SpawnAuthoritativeProjectile(const UStulWeaponDefinition& Definition, AStulWeapon& Weapon, const FVector& ViewOrigin, const FStulProjectileId& ProjectileId, FStulWeaponShotResult& ShotResult) const;
	void DrawShotDebug(const FVector& ViewOrigin, const FVector& AimTarget, const FHitResult& AimHit, const FStulWeaponShotResult& ShotResult) const;
	int32 MakeShotSeed(const FStulProjectileId& ProjectileId) const;
	void DispatchFiringStarted();
	void DispatchShotExecuted(const FStulWeaponShotExecution& ShotExecution);
	void DispatchFiringEnded();

	/************************ Validation ************************/
	bool CaptureView(FStulCapturedView& OutCapturedView) const;
	bool ValidateAndResolveView(const FStulCapturedView& CapturedView, FVector& OutViewOrigin, FVector& OutViewDirection) const;
	bool IsWeaponEquipped() const;
	bool IsAbilityAuthorizationValid(EStulFireSessionType SessionType) const;

	/************************ Runtime State ************************/
	FAbilityAuthorization AbilityAuthorization;
	FLocalFireSession LocalSession;
	FServerFireSession ServerSession;
	TOptional<FStulOpenFireSessionRequest> PendingOpenRequest;
	TOptional<FStulCloseFireSessionRequest> PendingCloseRequest;
	TArray<FStulShotBatch> OrphanBatches;
	TMap<FStulShotId, int32> PendingLocalShotCosts;
	uint32 NextLocalSessionId = 0;
	uint32 LastServerSessionId = 0;
	uint32 LastAppliedAckRevision = 0;
	uint32 LastAppliedAckSessionId = 0;
	int32 LatestAuthoritativeAmmoBaseline = 0;
	double EarliestNextLocalShotTime = -TNumericLimits<double>::Max();
	double EarliestNextAuthoritativeShotTime = -TNumericLimits<double>::Max();
	bool bCompletingAbilityCallback = false;

	UPROPERTY(ReplicatedUsing = OnRep_OwnerReconciliationState)
	FStulOwnerReconciliationState OwnerReconciliationState;

#if WITH_DEV_AUTOMATION_TESTS
	friend class FStulWeaponFireServerProcessingTimerTest;
	friend class FStulWeaponFireTimelineTest;
#endif
};
