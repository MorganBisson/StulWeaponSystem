#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "ProjectileVisualComponent.generated.h"

/** Cosmetic attachment root that converges a projectile visual from its muzzle origin to its gameplay trajectory. */
UCLASS(ClassGroup = (StulWeaponSystem), meta = (BlueprintSpawnableComponent))
class STULWEAPONSYSTEM_API UProjectileVisualComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UProjectileVisualComponent();

	/************************ Initialization ************************/
	/** Starts presentation at the captured muzzle origin. Has no effect on gameplay collision or movement. */
	void InitializeVisual(const FVector& CosmeticOrigin);

	/************************ State ************************/
	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Projectile|Visual")
	bool IsConverging() const { return bConvergenceActive; }

protected:
	/************************ Component ************************/
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/************************ Convergence ************************/
	/** Gameplay distance travelled before this visual root fully reaches the authoritative trajectory. Zero disables convergence. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Visual", meta = (ClampMin = "0.0", Units = "cm"))
	float ConvergenceDistance = 1000.0f;

	/************************ Debug ************************/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Visual|Debug")
	bool bDrawDebugConvergence = false;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Visual|Debug", meta = (ClampMin = "0.0", Units = "s", EditCondition = "bDrawDebugConvergence"))
	float DebugDrawDuration = 2.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Visual|Debug", meta = (ClampMin = "0.0", EditCondition = "bDrawDebugConvergence"))
	float DebugLineThickness = 0.0f;

private:
	/************************ Runtime State ************************/
	FVector ConvergenceStartLocation = FVector::ZeroVector;
	FVector InitialWorldOffset = FVector::ZeroVector;
	FVector LastDebugVisualLocation = FVector::ZeroVector;
	bool bConvergenceActive = false;
};
