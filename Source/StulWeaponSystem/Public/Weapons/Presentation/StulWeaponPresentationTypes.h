#pragma once

#include "CoreMinimal.h"
#include "StulWeaponPresentationTypes.generated.h"

class UNiagaraSystem;
class USoundBase;

/** Optional client-side assets and parameters used by the generic weapon Gameplay Cues. */
USTRUCT(BlueprintType)
struct STULWEAPONSYSTEM_API FStulWeaponPresentationData
{
	GENERATED_BODY()

	/************************ Fire ************************/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire")
	TSoftObjectPtr<UNiagaraSystem> MuzzleFlash;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire")
	TSoftObjectPtr<USoundBase> FireSound;
	/** Optional value for a Niagara user parameter such as User.MuzzleColor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire")
	FLinearColor MuzzleColor = FLinearColor::White;

	/************************ Hitscan Tracer ************************/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hitscan Tracer")
	TSoftObjectPtr<UNiagaraSystem> HitscanTracer;
	/** Optional value for a Niagara user parameter such as User.TracerColor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hitscan Tracer")
	FLinearColor TracerColor = FLinearColor::White;

	/************************ Impact ************************/
	/** Fallback used when no effect is configured for the impacted Physical Surface. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Impact")
	TSoftObjectPtr<UNiagaraSystem> DefaultImpactEffect;
	/** Fallback used when no sound is configured for the impacted Physical Surface. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Impact")
	TSoftObjectPtr<USoundBase> DefaultImpactSound;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Impact")
	TMap<TEnumAsByte<EPhysicalSurface>, TSoftObjectPtr<UNiagaraSystem>> ImpactEffectsBySurface;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Impact")
	TMap<TEnumAsByte<EPhysicalSurface>, TSoftObjectPtr<USoundBase>> ImpactSoundsBySurface;
};
