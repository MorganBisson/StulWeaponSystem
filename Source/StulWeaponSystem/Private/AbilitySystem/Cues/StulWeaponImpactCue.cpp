#include "AbilitySystem/Cues/StulWeaponImpactCue.h"

#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "StulWeaponSystem.h"
#include "Weapons/Ammunition/StulAmmoDefinition.h"
#include "Weapons/Ammunition/StulImpactProfile.h"

bool UStulWeaponImpactCue::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	if (!MyTarget || MyTarget->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	const UStulAmmoDefinition* AmmoDefinition = Cast<UStulAmmoDefinition>(Parameters.SourceObject.Get());
	if (!AmmoDefinition || !AmmoDefinition->ImpactProfile)
	{
		return false;
	}

	const UPhysicalMaterial* PhysicalMaterial = Parameters.PhysicalMaterial.Get();
	if (!PhysicalMaterial)
	{
		if (const FHitResult* HitResult = Parameters.EffectContext.GetHitResult())
		{
			PhysicalMaterial = HitResult->PhysMaterial.Get();
		}
	}

	const EPhysicalSurface SurfaceType = UPhysicalMaterial::DetermineSurfaceType(PhysicalMaterial);
	FStulSurfaceImpactResponse ImpactResponse;
	if (!AmmoDefinition->ImpactProfile->ResolveResponse(SurfaceType, ImpactResponse))
	{
		return false;
	}

	bool bPlayedImpact = false;
	if (!ImpactResponse.Effect.IsNull())
	{
		if (UNiagaraSystem* ImpactEffect = ImpactResponse.Effect.Get())
		{
			const FVector ImpactNormal = FVector(Parameters.Normal).GetSafeNormal();
			const FRotator ImpactRotation = ImpactNormal.IsNearlyZero() ? FRotator::ZeroRotator : FRotationMatrix::MakeFromZ(ImpactNormal).Rotator();
			bPlayedImpact |= UNiagaraFunctionLibrary::SpawnSystemAtLocation(MyTarget, ImpactEffect, Parameters.Location, ImpactRotation, FVector::OneVector, true, true, ENCPoolMethod::AutoRelease, true) != nullptr;
		}
		else
		{
			UE_LOG(LogStulWeaponSystem, Warning, TEXT("Impact profile '%s' cannot play effect '%s' because it was not preloaded."), *GetNameSafe(AmmoDefinition->ImpactProfile), *ImpactResponse.Effect.ToSoftObjectPath().ToString());
		}
	}

	if (!ImpactResponse.Sound.IsNull())
	{
		if (USoundBase* ImpactSound = ImpactResponse.Sound.Get())
		{
			UGameplayStatics::PlaySoundAtLocation(MyTarget, ImpactSound, Parameters.Location);
			bPlayedImpact = true;
		}
		else
		{
			UE_LOG(LogStulWeaponSystem, Warning, TEXT("Impact profile '%s' cannot play sound '%s' because it was not preloaded."), *GetNameSafe(AmmoDefinition->ImpactProfile), *ImpactResponse.Sound.ToSoftObjectPath().ToString());
		}
	}

	return bPlayedImpact;
}
