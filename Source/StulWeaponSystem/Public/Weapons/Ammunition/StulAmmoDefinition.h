#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StulAmmoDefinition.generated.h"

class UStulImpactProfile;
class FDataValidationContext;

/** Immutable ammunition payload shared by weapons independently of their delivery method. */
UCLASS(BlueprintType, Const)
class STULWEAPONSYSTEM_API UStulAmmoDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	void GetPresentationAssetPaths(TArray<FSoftObjectPath>& OutPaths) const;

	/** Reusable cosmetic responses for impacts produced by this ammunition. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Impact")
	TObjectPtr<UStulImpactProfile> ImpactProfile;
};
