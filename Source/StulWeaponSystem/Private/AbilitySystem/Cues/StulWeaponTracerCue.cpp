// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Cues/StulWeaponTracerCue.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "StulWeaponSystem.h"
#include "Weapons/StulWeapon.h"

namespace StulWeaponTracerParameters
{
	const FName Start(TEXT("User.Start"));
	const FName Target(TEXT("User.Target"));
	const FName Color(TEXT("User.Color"));
	const FName Speed(TEXT("User.Speed"));
	const FName Length(TEXT("User.Length"));
	const FName Width(TEXT("User.Width"));
}

bool UStulWeaponTracerCue::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	const AStulWeapon* Weapon = Cast<AStulWeapon>(MyTarget);
	if (!Weapon || Weapon->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	FStulWeaponTracerData TracerData;
	if (!Weapon->GetHitscanTracerData(TracerData) || TracerData.System.IsNull())
	{
		return false;
	}

	UNiagaraSystem* TracerSystem = TracerData.System.Get();
	if (!TracerSystem)
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Weapon '%s' cannot play its hitscan tracer because '%s' was not preloaded."), *GetNameSafe(Weapon), *TracerData.System.ToSoftObjectPath().ToString());
		return false;
	}

	FVector TracerStart;
	FVector TracerTarget;
	if (!ResolveTracerEndpoints(*Weapon, Parameters, TracerStart, TracerTarget)) return false;
	UNiagaraComponent* TracerComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(Weapon, TracerSystem, TracerStart, FRotator::ZeroRotator, FVector::OneVector, false, false, ENCPoolMethod::AutoRelease, true);
	if (!TracerComponent)
	{
		return false;
	}

	TracerComponent->SetVariablePosition(StulWeaponTracerParameters::Start, TracerStart);
	TracerComponent->SetVariablePosition(StulWeaponTracerParameters::Target, TracerTarget);
	TracerComponent->SetVariableLinearColor(StulWeaponTracerParameters::Color, TracerData.Color);
	TracerComponent->SetVariableFloat(StulWeaponTracerParameters::Speed, TracerData.Speed);
	TracerComponent->SetVariableFloat(StulWeaponTracerParameters::Length, TracerData.Length);
	TracerComponent->SetVariableFloat(StulWeaponTracerParameters::Width, TracerData.Width);
	TracerComponent->Activate(true);

	return true;
}

bool UStulWeaponTracerCue::ResolveTracerEndpoints(const AStulWeapon& Weapon, const FGameplayCueParameters& Parameters, FVector& OutStart, FVector& OutTarget)
{
	const float TracerDistance = FMath::Max(0.0f, Parameters.RawMagnitude);
	const FVector TracerDirection = FVector(Parameters.Normal).GetSafeNormal();
	if (TracerDistance <= UE_KINDA_SMALL_NUMBER || TracerDirection.IsNearlyZero()) return false;

	OutStart = Weapon.GetMuzzleTransform().GetLocation();
	OutTarget = FVector(Parameters.Location) + TracerDirection * TracerDistance;
	return true;
}

