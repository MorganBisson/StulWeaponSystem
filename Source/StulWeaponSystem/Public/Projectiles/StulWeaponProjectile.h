#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StulWeaponProjectile.generated.h"

class FLifetimeProperty;
class UProjectileVisualComponent;
class UProjectileMovementComponent;
class USphereComponent;

/** Immutable launch data shared by the authoritative projectile and its simulated proxies. */
USTRUCT(BlueprintType)
struct STULWEAPONSYSTEM_API FStulWeaponProjectileInitData
{
	GENERATED_BODY()

	/** Cosmetic launch point used later to converge presentation from the muzzle onto the gameplay trajectory. */
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

	/** Compact network sequence; intentionally not exposed because Blueprint does not support uint16 properties. */
	UPROPERTY()
	uint16 ShotSequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Projectile")
	int32 ProjectileIndex = 0;

	bool IsValid() const { return !Direction.IsNearlyZero() && Speed > 0.0f && MaxLifeSeconds > 0.0f; }
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
	bool InitializeProjectile(const FStulWeaponProjectileInitData& InInitData, float InDamage);
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
	virtual void Tick(float DeltaSeconds) override;

	/************************ Impact ************************/
	UFUNCTION()
	void HandleProjectileStop(const FHitResult& ImpactResult);
	virtual void HandleAuthoritativeImpact(const FHitResult& ImpactResult);

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
	float Damage = 0.0f;
	FVector LastDebugLocation = FVector::ZeroVector;
	bool bInitialized = false;
};
