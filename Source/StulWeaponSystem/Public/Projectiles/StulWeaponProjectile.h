#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Weapons/Shooting/StulWeaponFireSessionTypes.h"
#include "StulWeaponProjectile.generated.h"

class FLifetimeProperty;
class UPrimitiveComponent;
class UProjectileVisualComponent;
class UProjectileMovementComponent;
class USphereComponent;
class UStulAmmoDefinition;
class AStulWeapon;

/** Immutable launch data shared by the authoritative projectile and its simulated proxies. */
USTRUCT(BlueprintType)
struct STULWEAPONSYSTEM_API FStulWeaponProjectileInitData
{
	GENERATED_BODY()

	/** Producer-local launch point used by local presentation and as a fallback when a remote weapon representation is unavailable. */
	UPROPERTY(BlueprintReadOnly, Category = "Projectile")
	FVector_NetQuantize10 CosmeticOrigin = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Projectile")
	FVector_NetQuantizeNormal Direction = FVector::ForwardVector;

	UPROPERTY(BlueprintReadOnly, Category = "Projectile", meta = (Units = "cm/s"))
	float Speed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Projectile")
	float GravityScale = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Projectile", meta = (Units = "s"))
	float MaxLifeSeconds = 0.0f;

	/** Correlates an owning client's visual proxy with the authoritative server projectile. */
	UPROPERTY(BlueprintReadOnly, Category = "Projectile")
	FStulProjectileId ProjectileId;

	bool IsValid() const { return ProjectileId.IsValid() && !Direction.IsNearlyZero() && Speed > 0.0f && MaxLifeSeconds > 0.0f; }
};

/** Server-authoritative moving projectile. Simulated proxies are presentation-only and never process collision. */
UCLASS(BlueprintType, Blueprintable)
class STULWEAPONSYSTEM_API AStulWeaponProjectile : public AActor
{
	GENERATED_BODY()

public:
	AStulWeaponProjectile();

	/************************ Actor ************************/
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/************************ Initialization ************************/
	/** Initializes a deferred-spawned projectile on the server before FinishSpawning is called. */
	bool InitializeProjectile(const FStulWeaponProjectileInitData& InInitData, float InDamage, UStulAmmoDefinition* InAmmoDefinition);
	/** Initializes a local cosmetic prediction. It never replicates or applies gameplay impact. */
	bool InitializePredictedProjectile(const FStulWeaponProjectileInitData& InInitData);
	/** Confirms that the server accepted this visual prediction without replacing it. */
	void ConfirmPrediction();
	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Projectile")
	FStulWeaponProjectileInitData GetProjectileInitData() const { return InitData; }
	/** Presentation hook called after replicated launch data has configured the projectile. */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Stul Weapon System|Projectile", meta = (DisplayName = "On Projectile Initialized"))
	void K2_OnProjectileInitialized(const FStulWeaponProjectileInitData& ProjectileInitData);

	/************************ Components ************************/
	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Projectile")
	USphereComponent* GetCollisionComponent() const { return CollisionComponent; }
	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Projectile")
	UProjectileVisualComponent* GetVisualComponent() const { return VisualComponent; }
	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Projectile")
	UProjectileMovementComponent* GetProjectileMovementComponent() const { return ProjectileMovementComponent; }

protected:
	/************************ Actor ************************/
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	/************************ Impact ************************/
	UFUNCTION()
	void HandleProjectileStop(const FHitResult& ImpactResult);
	virtual void HandleAuthoritativeImpact(const FHitResult& ImpactResult);

	/************************ Prediction ************************/
	/** Maximum time a cosmetic prediction may exist without receiving its authoritative counterpart. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Prediction", meta = (ClampMin = "0.1", Units = "s"))
	float PredictedConfirmationTimeout = 1.0f;

	/************************ Debug ************************/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Debug")
	bool bDrawDebugTrajectory = false;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Debug", meta = (ClampMin = "0.0", Units = "s", EditCondition = "bDrawDebugTrajectory"))
	float DebugDrawDuration = 2.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Debug", meta = (ClampMin = "0.0", Units = "cm", EditCondition = "bDrawDebugTrajectory"))
	float DebugImpactRadius = 8.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Debug", meta = (ClampMin = "0.0", EditCondition = "bDrawDebugTrajectory"))
	float DebugLineThickness = 0.0f;

private:
	enum class EInstanceMode : uint8
	{
		Uninitialized,
		Authoritative,
		Predicted,
		Simulated
	};

	void ConfigureCollisionIgnores();
	static FVector ResolveSimulatedCosmeticOrigin(const AStulWeapon* SourceWeapon, const FVector& ReplicatedCosmeticOrigin);
	bool IsAuthoritativeProjectile() const { return InstanceMode == EInstanceMode::Authoritative; }
	bool IsPredictedProjectile() const { return InstanceMode == EInstanceMode::Predicted; }

	/************************ Components ************************/
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> CollisionComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UProjectileVisualComponent> VisualComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UProjectileMovementComponent> ProjectileMovementComponent;

	/************************ Runtime State ************************/
	UPROPERTY(Replicated)
	FStulWeaponProjectileInitData InitData;
	/** Server-side shot snapshot so an in-flight projectile keeps its original impact profile. */
	UPROPERTY(Transient)
	TObjectPtr<UStulAmmoDefinition> AmmoDefinition;
	TWeakObjectPtr<UPrimitiveComponent> IgnoringOwnerCollisionComponent;
	float Damage = 0.0f;
	FVector LastDebugLocation = FVector::ZeroVector;
	EInstanceMode InstanceMode = EInstanceMode::Uninitialized;
	bool bInitialized = false;
	bool bPredictionConfirmed = false;

#if WITH_DEV_AUTOMATION_TESTS
	friend class FStulWeaponProjectileRemoteVisualOriginTest;
#endif
};
