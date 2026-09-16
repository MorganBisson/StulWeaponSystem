#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/StulWeaponGameplayAbility.h"
#include "Weapons/Shooting/StulWeaponShootingTypes.h"
#include "StulWeaponFireAbility.generated.h"

class UAbilityTask_WaitInputRelease;
class AStulWeapon;
class UStulWeaponDefinition;

/** Predicted weapon firing ability with server-authoritative shot validation. */
UCLASS(Blueprintable)
class STULWEAPONSYSTEM_API UStulWeaponFireAbility : public UStulWeaponGameplayAbility
{
	GENERATED_BODY()

public:
	UStulWeaponFireAbility();

protected:
	/************************ Gameplay Ability ************************/
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo) const override;

	/************************ Extension Points ************************/
	/** Called for each locally predicted or authoritative projectile emitted by a shot. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Stul Weapon System|Shooting", meta = (DisplayName = "On Shot Executed"))
	void K2_OnShotExecuted(const FStulWeaponShotResult& ShotResult);

	/** Called only by the authority when a hitscan projectile produces a blocking hit. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Stul Weapon System|Shooting", meta = (DisplayName = "On Authoritative Hit"))
	void K2_OnAuthoritativeHit(const FStulWeaponShotResult& ShotResult);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Validation", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxClientViewOriginError = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Validation", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "deg"))
	float MaxClientAimErrorDegrees = 30.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooting|Validation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FireIntervalTolerance = 0.1f;

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
	/************************ Local Firing ************************/
	void BeginLocalFiring();
	bool SubmitLocalShot();
	void ScheduleNextLocalShot(float Delay);
	void HandleLocalShotTimer();
	void FinishFiring(bool bWasCancelled);
	float GetDelayAfterCurrentLocalShot() const;

	UFUNCTION()
	void HandleInputReleased(float TimeHeld);

	/************************ Target Data ************************/
	void BindServerTargetData();
	void UnbindServerTargetData();
	void HandleReplicatedTargetData(const FGameplayAbilityTargetDataHandle& TargetData, FGameplayTag ActivationTag);
	bool ValidateTargetData(const FGameplayAbilityTargetDataHandle& TargetData, FVector& OutViewOrigin, FVector& OutViewDirection, uint16& OutShotSequence) const;
	bool ValidateAuthoritativeCadence(uint16 ShotSequence);

	/************************ Shot Execution ************************/
	void ExecuteShot(const FVector& ViewOrigin, const FVector& ViewDirection, uint16 ShotSequence, bool bAuthoritative);
	void ExecuteHitscanShot(const UStulWeaponDefinition& Definition, const FVector& ViewOrigin, const TArray<AActor*>& IgnoredActors, FStulWeaponShotResult& ShotResult);
	bool SpawnAuthoritativeProjectile(const UStulWeaponDefinition& Definition, AStulWeapon& Weapon, const FVector& ViewOrigin, uint16 ShotSequence, FStulWeaponShotResult& ShotResult) const;
	void HandleAuthoritativeHitscanImpact(AStulWeapon& Weapon, const FStulWeaponShotResult& ShotResult);
	void NotifyShotExecuted(AStulWeapon& Weapon, const FStulWeaponShotResult& ShotResult);
	void DrawShotDebug(const FVector& ViewOrigin, const FVector& AimTarget, const FHitResult& AimHit, const FStulWeaponShotResult& ShotResult) const;
	void SendWeaponEvent(FGameplayTag EventTag, const FStulWeaponShotResult* ShotResult = nullptr) const;
	int32 MakeShotSeed(uint16 ShotSequence, int32 ProjectileIndex) const;
	float GetFireInterval() const;
	float GetBurstInterval() const;
	int32 GetBurstSize() const;

	/************************ Runtime State ************************/
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitInputRelease> WaitInputReleaseTask;

	FTimerHandle LocalShotTimer;
	double EarliestNextLocalShotTime = -TNumericLimits<double>::Max();
	double EarliestNextAuthoritativeShotTime = -TNumericLimits<double>::Max();
	uint16 NextLocalShotSequence = 0;
	uint16 NextExpectedServerShotSequence = 0;
	int32 ShotsRemainingInBurst = 0;
	int32 AcceptedShotsInServerBurst = 0;
	FGameplayTag ActiveFireMode;
	bool bInputReleased = false;
	bool bServerTargetDataBound = false;
	bool bFiringStarted = false;
};
