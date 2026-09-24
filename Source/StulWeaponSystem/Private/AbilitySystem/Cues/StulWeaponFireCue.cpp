#include "AbilitySystem/Cues/StulWeaponFireCue.h"

#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "StulWeaponSystem.h"
#include "Weapons/StulWeapon.h"

namespace StulWeaponFireParameters
{
	const FName MuzzleColor(TEXT("User.MuzzleColor"));
}

bool UStulWeaponFireCue::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	const AStulWeapon* Weapon = Cast<AStulWeapon>(MyTarget);
	if (!Weapon || Weapon->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	FStulWeaponPresentationData Presentation;
	if (!Weapon->GetPresentationData(Presentation))
	{
		return false;
	}

	bool bPlayedFirePresentation = false;
	const FTransform LocalMuzzleTransform = Weapon->GetMuzzleTransform();
	const FVector LocalMuzzleLocation = LocalMuzzleTransform.GetLocation();
	if (!Presentation.MuzzleFlash.IsNull())
	{
		if (UNiagaraSystem* MuzzleFlash = Presentation.MuzzleFlash.Get())
		{
			const FVector FireDirection = FVector(Parameters.Normal).GetSafeNormal();
			const FRotator FireRotation = FireDirection.IsNearlyZero() ? LocalMuzzleTransform.Rotator() : FireDirection.Rotation();
			if (UNiagaraComponent* MuzzleComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(Weapon, MuzzleFlash, LocalMuzzleLocation, FireRotation, FVector::OneVector, false, false, ENCPoolMethod::AutoRelease, true))
			{
				MuzzleComponent->SetVariableLinearColor(StulWeaponFireParameters::MuzzleColor, Presentation.MuzzleColor);
				MuzzleComponent->Activate(true);
				bPlayedFirePresentation = true;
			}
		}
		else
		{
			UE_LOG(LogStulWeaponSystem, Warning, TEXT("Weapon '%s' cannot play muzzle flash '%s' because it was not preloaded."), *GetNameSafe(Weapon), *Presentation.MuzzleFlash.ToSoftObjectPath().ToString());
		}
	}

	if (!Presentation.FireSound.IsNull())
	{
		if (USoundBase* FireSound = Presentation.FireSound.Get())
		{
			UGameplayStatics::PlaySoundAtLocation(Weapon, FireSound, LocalMuzzleLocation, FRotator::ZeroRotator, 1.0f, 1.0f, 0.0f, nullptr, nullptr, Weapon);
			bPlayedFirePresentation = true;
		}
		else
		{
			UE_LOG(LogStulWeaponSystem, Warning, TEXT("Weapon '%s' cannot play fire sound '%s' because it was not preloaded."), *GetNameSafe(Weapon), *Presentation.FireSound.ToSoftObjectPath().ToString());
		}
	}

	return bPlayedFirePresentation;
}
