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
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UStulWeaponAimComponent::BeginPlay()
{
	Super::BeginPlay();
	RefreshAimPresentation();
}

void UStulWeaponAimComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	BindWeapon(nullptr);
	if (WeaponManager) WeaponManager->OnWeaponEquipped.RemoveDynamic(this, &ThisClass::HandleWeaponEquipped);
	WeaponManager = nullptr;
	Super::EndPlay(EndPlayReason);
}

void UStulWeaponAimComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!IsLocallyPresented())
	{
		SetComponentTickEnabled(ShouldWaitForLocalControl());
		return;
	}
	if (!WeaponManager)
	{
		RefreshAimPresentation();
		return;
	}
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
		SetComponentTickEnabled(ShouldWaitForLocalControl());
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

bool UStulWeaponAimComponent::ShouldWaitForLocalControl() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	return OwnerPawn && OwnerPawn->GetLocalRole() == ROLE_AutonomousProxy;
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
	bPendingHipCapture = false;
	bHasHipTransform = false;
	if (!EquippedWeapon || !EquippedWeapon->GetRootComponent() || !EquippedWeapon->GetRootComponent()->GetAttachParent()) return false;

	FStulWeaponAimData AimData;
	USkeletalMeshComponent* WeaponMesh = EquippedWeapon->GetWeaponMesh();
	if (!WeaponMesh || !EquippedWeapon->GetAimData(AimData) || !WeaponMesh->DoesSocketExist(AimData.AimSocketName))
	{
		if (!bLoggedInvalidSetup) UE_LOG(LogStulWeaponSystem, Warning, TEXT("Aim component cannot align weapon '%s' because its aim socket is unavailable."), *GetNameSafe(EquippedWeapon));
		bLoggedInvalidSetup = true;
		return false;
	}

	HipRelativeTransform = EquippedWeapon->GetRootComponent()->GetRelativeTransform();
	bHasHipTransform = true;
	bLoggedInvalidSetup = false;
	return true;
}

void UStulWeaponAimComponent::ApplyAimPresentation()
{
	if (!bAlignAimSocket || !EquippedWeapon)
	{
		SetComponentTickEnabled(false);
		return;
	}
	if (bPendingHipCapture && !CaptureHipTransform())
	{
		SetComponentTickEnabled(false);
		return;
	}
	if (!bHasHipTransform) return;

	USceneComponent* AttachParent = EquippedWeapon->GetRootComponent()->GetAttachParent();
	USceneComponent* WeaponRoot = EquippedWeapon->GetRootComponent();
	USkeletalMeshComponent* WeaponMesh = EquippedWeapon->GetWeaponMesh();
	FStulWeaponAimData AimData;
	FTransform ViewTransform;
	if (!AttachParent || !WeaponRoot || !WeaponMesh || !EquippedWeapon->GetAimData(AimData) || !ResolveViewTransform(ViewTransform)) return;

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
	if (EquippedWeapon == NewWeapon) return;
	if (EquippedWeapon) EquippedWeapon->OnAimStateChanged.RemoveDynamic(this, &ThisClass::HandleAimStateChanged);

	EquippedWeapon = NewWeapon;
	bHasHipTransform = false;
	bPendingHipCapture = IsValid(NewWeapon);
	bLoggedInvalidSetup = false;
	if (EquippedWeapon) EquippedWeapon->OnAimStateChanged.AddUniqueDynamic(this, &ThisClass::HandleAimStateChanged);
	SetComponentTickEnabled(bPendingHipCapture);
}

void UStulWeaponAimComponent::HandleWeaponEquipped(AStulWeapon* NewWeapon, AStulWeapon* /*PreviousWeapon*/)
{
	BindWeapon(NewWeapon);
}

void UStulWeaponAimComponent::HandleAimStateChanged(AStulWeapon* Weapon, const bool /*bIsAiming*/)
{
	if (Weapon == EquippedWeapon && bHasHipTransform) SetComponentTickEnabled(true);
}
