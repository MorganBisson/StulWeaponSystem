#include "Components/StulWeaponAimComponent.h"

#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StulWeaponManagerComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/StulWeaponOwnerInterface.h"
#include "StulWeaponSystem.h"
#include "Weapons/StulWeapon.h"

/*********************************************************************************************/
/************************************ Actor Component ****************************************/
/*********************************************************************************************/

UStulWeaponAimComponent::UStulWeaponAimComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UStulWeaponAimComponent::BeginPlay()
{
	Super::BeginPlay();
	if (APawn* OwnerPawn = Cast<APawn>(GetOwner())) OwnerPawn->ReceiveControllerChangedDelegate.AddUniqueDynamic(this, &ThisClass::HandleControllerChanged);
	RefreshAimPresentation();
}

void UStulWeaponAimComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	BindWeapon(nullptr);
	if (WeaponManager) WeaponManager->OnWeaponEquipped.RemoveDynamic(this, &ThisClass::HandleWeaponEquipped);
	if (APawn* OwnerPawn = Cast<APawn>(GetOwner())) OwnerPawn->ReceiveControllerChangedDelegate.RemoveDynamic(this, &ThisClass::HandleControllerChanged);
	WeaponManager = nullptr;
	Super::EndPlay(EndPlayReason);
}

void UStulWeaponAimComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!IsLocallyPresented()) return;
	ApplyAimPresentation();
}

/*********************************************************************************************/
/******************************************* Aim *********************************************/
/*********************************************************************************************/

float UStulWeaponAimComponent::GetAimAlpha() const
{
	return EquippedWeapon ? EquippedWeapon->GetAimAlpha() : 0.0f;
}

bool UStulWeaponAimComponent::IsFullyAimed() const
{
	return EquippedWeapon && EquippedWeapon->IsFullyAimed();
}

void UStulWeaponAimComponent::RefreshAimPresentation()
{
	if (!IsLocallyPresented())
	{
		if (WeaponManager) WeaponManager->OnWeaponEquipped.RemoveDynamic(this, &ThisClass::HandleWeaponEquipped);
		WeaponManager = nullptr;
		BindWeapon(nullptr);
		SetComponentTickEnabled(false);
		return;
	}

	UStulWeaponManagerComponent* ResolvedManager = GetOwner() ? GetOwner()->FindComponentByClass<UStulWeaponManagerComponent>() : nullptr;
	if (WeaponManager != ResolvedManager)
	{
		if (WeaponManager) WeaponManager->OnWeaponEquipped.RemoveDynamic(this, &ThisClass::HandleWeaponEquipped);
		WeaponManager = ResolvedManager;
		if (WeaponManager) WeaponManager->OnWeaponEquipped.AddUniqueDynamic(this, &ThisClass::HandleWeaponEquipped);
	}

	if (!WeaponManager)
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Aim component '%s' requires a Stul Weapon Manager on its Owner."), *GetNameSafe(this));
		BindWeapon(nullptr);
		return;
	}

	BindWeapon(WeaponManager->GetEquippedWeapon());
}

/*********************************************************************************************/
/*************************************** Internal ********************************************/
/*********************************************************************************************/

bool UStulWeaponAimComponent::IsLocallyPresented() const
{
	if (!GetOwner() || GetNetMode() == NM_DedicatedServer) return false;
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	return !OwnerPawn || OwnerPawn->IsLocallyControlled();
}

bool UStulWeaponAimComponent::ResolveViewTransform(FTransform& OutViewTransform) const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return false;

	FStulWeaponViewData ViewData;
	if (OwnerActor->GetClass()->ImplementsInterface(UStulWeaponOwnerInterface::StaticClass()) && IStulWeaponOwnerInterface::Execute_GetWeaponViewData(OwnerActor, ViewData))
	{
		OutViewTransform = FTransform(ViewData.ViewRotation, ViewData.ViewLocation);
		return true;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	OwnerActor->GetActorEyesViewPoint(ViewLocation, ViewRotation);
	OutViewTransform = FTransform(ViewRotation, ViewLocation);
	return true;
}

bool UStulWeaponAimComponent::CaptureHipTransform()
{
	bHasHipTransform = false;
	if (!EquippedWeapon || !EquippedWeapon->GetRootComponent() || !EquippedWeapon->GetRootComponent()->GetAttachParent()) return false;
	if (!EquippedWeapon->IsInitialized()) return false;

	FStulWeaponAimData AimData;
	USkeletalMeshComponent* WeaponMesh = EquippedWeapon->GetWeaponMesh();
	if (!WeaponMesh || !EquippedWeapon->GetAimData(AimData) || !WeaponMesh->DoesSocketExist(AimData.AimSocketName))
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Aim component cannot align weapon '%s' because its aim socket is unavailable."), *GetNameSafe(EquippedWeapon));
		StopHipTransformCapture();
		return false;
	}

	HipRelativeTransform = EquippedWeapon->GetRootComponent()->GetRelativeTransform();
	bHasHipTransform = true;
	StopHipTransformCapture();
	if (EquippedWeapon->IsAiming() || EquippedWeapon->GetAimAlpha() > UE_KINDA_SMALL_NUMBER) SetComponentTickEnabled(true);
	return true;
}

void UStulWeaponAimComponent::BeginHipTransformCapture()
{
	USceneComponent* WeaponRoot = EquippedWeapon ? EquippedWeapon->GetRootComponent() : nullptr;
	if (!WeaponRoot) return;
	WeaponRoot->TransformUpdated.RemoveAll(this);
	WeaponRoot->TransformUpdated.AddUObject(this, &ThisClass::HandleWeaponRootTransformUpdated);
	CaptureHipTransform();
}

void UStulWeaponAimComponent::StopHipTransformCapture()
{
	if (EquippedWeapon && EquippedWeapon->GetRootComponent()) EquippedWeapon->GetRootComponent()->TransformUpdated.RemoveAll(this);
}

void UStulWeaponAimComponent::RestoreWeaponPresentation()
{
	if (bHasHipTransform && EquippedWeapon && EquippedWeapon->GetRootComponent()) EquippedWeapon->GetRootComponent()->SetRelativeTransform(HipRelativeTransform);
	bHasHipTransform = false;
}

void UStulWeaponAimComponent::ApplyAimPresentation()
{
	if (!bAlignAimSocket || !EquippedWeapon)
	{
		SetComponentTickEnabled(false);
		return;
	}
	if (!bHasHipTransform) return;

	USceneComponent* WeaponRoot = EquippedWeapon->GetRootComponent();
	USceneComponent* AttachParent = WeaponRoot ? WeaponRoot->GetAttachParent() : nullptr;
	USkeletalMeshComponent* WeaponMesh = EquippedWeapon->GetWeaponMesh();
	FStulWeaponAimData AimData;
	FTransform ViewTransform;
	if (!AttachParent)
	{
		bHasHipTransform = false;
		SetComponentTickEnabled(false);
		BeginHipTransformCapture();
		return;
	}
	if (!WeaponRoot || !WeaponMesh || !EquippedWeapon->GetAimData(AimData) || !ResolveViewTransform(ViewTransform)) return;

	const FVector TargetLocation = ViewTransform.TransformPosition(AimTargetOffset);
	const FQuat TargetRotation = ViewTransform.GetRotation() * AimTargetRotationOffset.Quaternion();
	const FTransform TargetSocketWorldTransform(TargetRotation, TargetLocation);
	const FTransform AttachFrameWorldTransform = AttachParent->GetSocketTransform(WeaponRoot->GetAttachSocketName(), ERelativeTransformSpace::RTS_World);
	const FTransform TargetSocketRelativeTransform = TargetSocketWorldTransform.GetRelativeTransform(AttachFrameWorldTransform);
	const FTransform AimSocketWorldTransform = WeaponMesh->GetSocketTransform(AimData.AimSocketName, ERelativeTransformSpace::RTS_World);
	const FTransform AimSocketRelativeTransform = AimSocketWorldTransform.GetRelativeTransform(WeaponRoot->GetComponentTransform());

	const FTransform UnscaledTargetWeaponTransform = FTransform(AimSocketRelativeTransform.GetRotation(), AimSocketRelativeTransform.GetLocation()).Inverse() * FTransform(TargetSocketRelativeTransform.GetRotation(), TargetSocketRelativeTransform.GetLocation());
	FTransform DesiredWeaponRelativeTransform(UnscaledTargetWeaponTransform.GetRotation(), FVector::ZeroVector, HipRelativeTransform.GetScale3D());
	DesiredWeaponRelativeTransform.SetLocation(TargetSocketRelativeTransform.GetLocation() - DesiredWeaponRelativeTransform.TransformVector(AimSocketRelativeTransform.GetLocation()));

	const float AimAlpha = EquippedWeapon->GetAimAlpha();
	FTransform PresentedRelativeTransform;
	PresentedRelativeTransform.Blend(HipRelativeTransform, DesiredWeaponRelativeTransform, AimAlpha);
	EquippedWeapon->SetActorRelativeTransform(PresentedRelativeTransform);

#if ENABLE_DRAW_DEBUG
	if (bDrawAimDebug)
	{
		const FVector SocketLocation = WeaponMesh->GetSocketLocation(AimData.AimSocketName);
		DrawDebugLine(GetWorld(), SocketLocation, TargetLocation, FColor::Cyan, false, 0.0f, 0, 1.0f);
		DrawDebugSphere(GetWorld(), TargetLocation, DebugDrawSize * 0.25f, 8, FColor::Yellow, false, 0.0f, 0, 1.0f);
		DrawDebugSphere(GetWorld(), SocketLocation, DebugDrawSize * 0.25f, 8, FColor::Magenta, false, 0.0f, 0, 1.0f);
		DrawDebugCoordinateSystem(GetWorld(), TargetLocation, TargetRotation.Rotator(), DebugDrawSize, false, 0.0f, 0, 1.0f);
	}
#endif

	if (!EquippedWeapon->IsAiming() && AimAlpha <= UE_KINDA_SMALL_NUMBER) SetComponentTickEnabled(false);
}

void UStulWeaponAimComponent::BindWeapon(AStulWeapon* NewWeapon)
{
	if (EquippedWeapon == NewWeapon)
	{
		if (EquippedWeapon && !bHasHipTransform) BeginHipTransformCapture();
		return;
	}
	if (EquippedWeapon)
	{
		StopHipTransformCapture();
		RestoreWeaponPresentation();
		EquippedWeapon->OnAimStateChanged.RemoveDynamic(this, &ThisClass::HandleAimStateChanged);
		EquippedWeapon->OnWeaponReady.RemoveDynamic(this, &ThisClass::HandleWeaponReady);
	}

	EquippedWeapon = NewWeapon;
	bHasHipTransform = false;
	SetComponentTickEnabled(false);
	if (EquippedWeapon)
	{
		EquippedWeapon->OnAimStateChanged.AddUniqueDynamic(this, &ThisClass::HandleAimStateChanged);
		EquippedWeapon->OnWeaponReady.AddUniqueDynamic(this, &ThisClass::HandleWeaponReady);
		BeginHipTransformCapture();
	}
}

void UStulWeaponAimComponent::HandleWeaponEquipped(AStulWeapon* NewWeapon, AStulWeapon* /*PreviousWeapon*/)
{
	BindWeapon(NewWeapon);
}

void UStulWeaponAimComponent::HandleAimStateChanged(AStulWeapon* Weapon, const bool /*bIsAiming*/)
{
	if (Weapon == EquippedWeapon && bHasHipTransform) SetComponentTickEnabled(true);
}

void UStulWeaponAimComponent::HandleWeaponReady(AStulWeapon* Weapon)
{
	if (Weapon == EquippedWeapon && !bHasHipTransform) CaptureHipTransform();
}

void UStulWeaponAimComponent::HandleControllerChanged(APawn* Pawn, AController* /*OldController*/, AController* /*NewController*/)
{
	if (Pawn == GetOwner()) RefreshAimPresentation();
}

void UStulWeaponAimComponent::HandleWeaponRootTransformUpdated(USceneComponent* UpdatedComponent, const EUpdateTransformFlags /*UpdateTransformFlags*/, const ETeleportType /*Teleport*/)
{
	if (EquippedWeapon && UpdatedComponent == EquippedWeapon->GetRootComponent()) CaptureHipTransform();
}
