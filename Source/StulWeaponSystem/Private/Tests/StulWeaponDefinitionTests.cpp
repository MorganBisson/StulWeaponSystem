#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "AbilitySystem/Abilities/StulWeaponAimAbility.h"
#include "StulWeaponGameplayTags.h"
#include "Weapons/Ammunition/StulAmmoDefinition.h"
#include "Weapons/Ammunition/StulImpactProfile.h"
#include "Weapons/StulWeaponDefinition.h"

namespace
{
	UStulWeaponDefinition* MakeValidWeaponDefinition()
	{
		UStulWeaponDefinition* Definition = NewObject<UStulWeaponDefinition>();
		Definition->DefaultAmmo = NewObject<UStulAmmoDefinition>(Definition);
		Definition->DefaultAmmo->ImpactProfile = NewObject<UStulImpactProfile>(Definition->DefaultAmmo);
		Definition->AvailableFireModes.AddTag(StulWeaponGameplayTags::FireMode_Single);
		Definition->DefaultFireModeTag = StulWeaponGameplayTags::FireMode_Single;
		return Definition;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStulWeaponDefinitionFireModeValidationTest, "StulWeaponSystem.Definition.FireModeValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStulWeaponDefinitionFireModeValidationTest::RunTest(const FString& Parameters)
{
	UStulWeaponDefinition* Definition = MakeValidWeaponDefinition();
	FDataValidationContext ValidContext;
	TestNotEqual(TEXT("A supported baseline fire mode is accepted"), Definition->IsDataValid(ValidContext), EDataValidationResult::Invalid);

	Definition->AvailableFireModes.AddTag(StulWeaponGameplayTags::FireMode_Root);
	FDataValidationContext InvalidContext;
	TestEqual(TEXT("A fire mode unknown to the runtime cycle is rejected"), Definition->IsDataValid(InvalidContext), EDataValidationResult::Invalid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStulWeaponDefinitionShotPatternValidationTest, "StulWeaponSystem.Definition.ShotPatternValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStulWeaponDefinitionShotPatternValidationTest::RunTest(const FString& Parameters)
{
	UStulWeaponDefinition* Definition = MakeValidWeaponDefinition();
	FStulWeaponShotPattern Pattern;
	Pattern.ShotPoints.Add(FVector2D::ZeroVector);
	Definition->ShotPatterns.Add(3, Pattern);

	FDataValidationContext Context;
	TestEqual(TEXT("A shot pattern whose key and point count disagree is rejected"), Definition->IsDataValid(Context), EDataValidationResult::Invalid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStulWeaponDefinitionAbilityMappingValidationTest, "StulWeaponSystem.Definition.AbilityMappingValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStulWeaponDefinitionAbilityMappingValidationTest::RunTest(const FString& Parameters)
{
	UStulWeaponDefinition* Definition = MakeValidWeaponDefinition();
	FStulWeaponAbilityMapping Mapping;
	Mapping.AbilityClass = UStulWeaponAimAbility::StaticClass();
	Mapping.InputTag = StulWeaponGameplayTags::Input_Aim;
	Definition->BaseAbilities.Add(Mapping);
	Definition->BaseAbilities.Add(Mapping);

	FDataValidationContext Context;
	TestEqual(TEXT("A duplicate ability mapping is rejected"), Definition->IsDataValid(Context), EDataValidationResult::Invalid);
	return true;
}

#endif
