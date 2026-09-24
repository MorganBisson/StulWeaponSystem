#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StulImpactProfile.generated.h"

class UNiagaraSystem;
class USoundBase;
class FDataValidationContext;

/** Audiovisual response produced when one ammunition impact hits a surface. */
USTRUCT(BlueprintType)
struct STULWEAPONSYSTEM_API FStulSurfaceImpactResponse
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Impact|Visual")
	TSoftObjectPtr<UNiagaraSystem> Effect;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Impact|Audio")
	TSoftObjectPtr<USoundBase> Sound;

	bool IsEmpty() const { return Effect.IsNull() && Sound.IsNull(); }
};

/** Reusable audiovisual impact responses indexed by the project's Physical Surfaces. */
UCLASS(BlueprintType, Const)
class STULWEAPONSYSTEM_API UStulImpactProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	/** Resolves independent effect and sound fallbacks for one surface. */
	bool ResolveResponse(EPhysicalSurface SurfaceType, FStulSurfaceImpactResponse& OutResponse) const;
	void GetPresentationAssetPaths(TArray<FSoftObjectPath>& OutPaths) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Impact")
	FStulSurfaceImpactResponse DefaultResponse;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Impact")
	TMap<TEnumAsByte<EPhysicalSurface>, FStulSurfaceImpactResponse> ResponsesBySurface;
};
