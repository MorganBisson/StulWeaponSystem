#include "Weapons/Ammunition/StulImpactProfile.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace StulImpactPrimaryAssetTypes
{
	const FPrimaryAssetType ImpactProfile(TEXT("StulImpactProfile"));
}

FPrimaryAssetId UStulImpactProfile::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(StulImpactPrimaryAssetTypes::ImpactProfile, GetFName());
}

#if WITH_EDITOR
EDataValidationResult UStulImpactProfile::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	bool bHasResponse = !DefaultResponse.IsEmpty();
	for (const TPair<TEnumAsByte<EPhysicalSurface>, FStulSurfaceImpactResponse>& ResponsePair : ResponsesBySurface)
	{
		if (ResponsePair.Value.IsEmpty())
		{
			Context.AddError(FText::Format(NSLOCTEXT("StulImpactValidation", "EmptySurfaceResponse", "Physical Surface '{0}' has an empty impact response."), FText::AsNumber(ResponsePair.Key.GetValue())));
			Result = EDataValidationResult::Invalid;
		}
		else
		{
			bHasResponse = true;
		}
	}

	if (!bHasResponse)
	{
		Context.AddError(NSLOCTEXT("StulImpactValidation", "EmptyImpactProfile", "An impact profile must define at least one effect or sound."));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

bool UStulImpactProfile::ResolveResponse(const EPhysicalSurface SurfaceType, FStulSurfaceImpactResponse& OutResponse) const
{
	OutResponse = DefaultResponse;
	if (const FStulSurfaceImpactResponse* SurfaceResponse = ResponsesBySurface.Find(SurfaceType))
	{
		if (!SurfaceResponse->Effect.IsNull())
		{
			OutResponse.Effect = SurfaceResponse->Effect;
		}
		if (!SurfaceResponse->Sound.IsNull())
		{
			OutResponse.Sound = SurfaceResponse->Sound;
		}
	}

	return !OutResponse.IsEmpty();
}

void UStulImpactProfile::GetPresentationAssetPaths(TArray<FSoftObjectPath>& OutPaths) const
{
	OutPaths.Reset();
	if (!DefaultResponse.Effect.IsNull())
	{
		OutPaths.AddUnique(DefaultResponse.Effect.ToSoftObjectPath());
	}
	if (!DefaultResponse.Sound.IsNull())
	{
		OutPaths.AddUnique(DefaultResponse.Sound.ToSoftObjectPath());
	}

	for (const TPair<TEnumAsByte<EPhysicalSurface>, FStulSurfaceImpactResponse>& ResponsePair : ResponsesBySurface)
	{
		if (!ResponsePair.Value.Effect.IsNull())
		{
			OutPaths.AddUnique(ResponsePair.Value.Effect.ToSoftObjectPath());
		}
		if (!ResponsePair.Value.Sound.IsNull())
		{
			OutPaths.AddUnique(ResponsePair.Value.Sound.ToSoftObjectPath());
		}
	}
}
