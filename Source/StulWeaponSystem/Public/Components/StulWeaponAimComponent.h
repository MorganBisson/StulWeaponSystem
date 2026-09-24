#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StulWeaponAimComponent.generated.h"

class AStulWeapon;
class AController;
class APawn;
class USceneComponent;
class UStulWeaponManagerComponent;

/** Generic local aim presentation for projects that do not provide their own animation implementation. */
UCLASS(ClassGroup = (StulWeaponSystem), meta = (BlueprintSpawnableComponent))
class STULWEAPONSYSTEM_API UStulWeaponAimComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStulWeaponAimComponent();

	/************************ Aim ************************/
	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Aim")
	float GetAimAlpha() const;
	UFUNCTION(BlueprintPure, Category = "Stul Weapon System|Aim")
	bool IsFullyAimed() const;
	/** Re-resolves the Manager and equipped weapon after runtime Blueprint setup. */
	UFUNCTION(BlueprintCallable, Category = "Stul Weapon System|Aim")
	void RefreshAimPresentation();

protected:
	/************************ Actor Component ************************/
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/************************ Configuration ************************/
	/** Moves the equipped weapon so its resolved aim socket follows the owning Character viewpoint. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stul Weapon System|Aim")
	bool bAlignAimSocket = true;
	/** Offset from the viewpoint in local view space. X is forward. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stul Weapon System|Aim")
	FVector AimTargetOffset = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stul Weapon System|Aim")
	FRotator AimTargetRotationOffset = FRotator::ZeroRotator;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stul Weapon System|Aim|Debug")
	bool bDrawAimDebug = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stul Weapon System|Aim|Debug", meta = (ClampMin = "0.0"))
	float DebugDrawSize = 5.0f;

private:
	/************************ Internal ************************/
	bool IsLocallyPresented() const;
	bool ResolveViewTransform(FTransform& OutViewTransform) const;
	bool CaptureHipTransform();
	void BeginHipTransformCapture();
	void StopHipTransformCapture();
	void RestoreWeaponPresentation();
	void ApplyAimPresentation();
	void BindWeapon(AStulWeapon* NewWeapon);

	UFUNCTION()
	void HandleWeaponEquipped(AStulWeapon* NewWeapon, AStulWeapon* PreviousWeapon);
	UFUNCTION()
	void HandleAimStateChanged(AStulWeapon* Weapon, bool bIsAiming);
	UFUNCTION()
	void HandleWeaponReady(AStulWeapon* Weapon);
	UFUNCTION()
	void HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);
	void HandleWeaponRootTransformUpdated(USceneComponent* UpdatedComponent, EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport);

	UPROPERTY(Transient)
	TObjectPtr<UStulWeaponManagerComponent> WeaponManager;
	UPROPERTY(Transient)
	TObjectPtr<AStulWeapon> EquippedWeapon;

	FTransform HipRelativeTransform = FTransform::Identity;
	bool bHasHipTransform = false;
};
