#include "Weapons/Ammunition/StulAmmoDefinition.h"

#include "Weapons/Ammunition/StulImpactProfile.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace StulAmmoPrimaryAssetTypes
{
	const FPrimaryAssetType AmmoDefinition(TEXT("StulAmmoDefinition"));
}

FPrimaryAssetId UStulAmmoDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(StulAmmoPrimaryAssetTypes::AmmoDefinition, GetFName());
}

#if WITH_EDITOR
EDataValidationResult UStulAmmoDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (!ImpactProfile)
	{
		Context.AddError(NSLOCTEXT("StulAmmoValidation", "MissingImpactProfile", "An ammunition definition must reference an ImpactProfile."));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

void UStulAmmoDefinition::GetPresentationAssetPaths(TArray<FSoftObjectPath>& OutPaths) const
{
	OutPaths.Reset();
	if (ImpactProfile)
	{
		ImpactProfile->GetPresentationAssetPaths(OutPaths);
	}
}
