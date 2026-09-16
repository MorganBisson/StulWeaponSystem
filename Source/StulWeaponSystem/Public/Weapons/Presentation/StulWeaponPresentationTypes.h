#pragma once

#include "CoreMinimal.h"
#include "StulWeaponPresentationTypes.generated.h"

class UNiagaraSystem;
class USoundBase;

/** Optional visual effect used by one cosmetic hitscan tracer. */
USTRUCT(BlueprintType)
struct STULWEAPONSYSTEM_API FStulWeaponTracerData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual|Effects|Hitscan Tracer")
	TSoftObjectPtr<UNiagaraSystem> System;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual|Effects|Hitscan Tracer")
	FLinearColor Color = FLinearColor::White;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual|Effects|Hitscan Tracer", meta = (ClampMin = "1.0", Units = "cm/s"))
	float Speed = 60000.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual|Effects|Hitscan Tracer", meta = (ClampMin = "0.01", Units = "cm"))
	float Length = 150.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual|Effects|Hitscan Tracer", meta = (ClampMin = "0.01", Units = "cm"))
	float Width = 2.0f;
};

/** Optional audiovisual effects used by the generic weapon Gameplay Cues. */
USTRUCT(BlueprintType)
struct STULWEAPONSYSTEM_API FStulWeaponPresentationData
{
	GENERATED_BODY()

	/************************ Fire ************************/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual|Effects|Fire")
	TSoftObjectPtr<UNiagaraSystem> MuzzleFlash;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual|Effects|Fire")
	TSoftObjectPtr<USoundBase> FireSound;
	/** Optional value for a Niagara user parameter such as User.MuzzleColor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual|Effects|Fire")
	FLinearColor MuzzleColor = FLinearColor::White;

	/************************ Impact ************************/
	/** Fallback used when no effect is configured for the impacted Physical Surface. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual|Effects|Impact")
	TSoftObjectPtr<UNiagaraSystem> DefaultImpactEffect;
	/** Fallback used when no sound is configured for the impacted Physical Surface. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual|Effects|Impact")
	TSoftObjectPtr<USoundBase> DefaultImpactSound;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual|Effects|Impact")
	TMap<TEnumAsByte<EPhysicalSurface>, TSoftObjectPtr<UNiagaraSystem>> ImpactEffectsBySurface;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual|Effects|Impact")
	TMap<TEnumAsByte<EPhysicalSurface>, TSoftObjectPtr<USoundBase>> ImpactSoundsBySurface;
};
