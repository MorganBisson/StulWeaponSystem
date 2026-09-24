#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Weapons/Shooting/StulWeaponShootingLibrary.h"
#include "Weapons/Shooting/StulWeaponShootingTypes.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStulWeaponDeterministicShootingTest, "StulWeaponSystem.Shooting.DeterministicCalculation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStulWeaponDeterministicShootingTest::RunTest(const FString& Parameters)
{
	FStulWeaponShotRequest Request;
	Request.ShotOrigin = FVector(100.0, 200.0, 300.0);
	Request.TargetLocation = FVector(1100.0, 250.0, 400.0);
	Request.PatternOffset = FVector2D(1.5, -2.0);
	Request.PatternScale = 0.75f;
	Request.SpreadHalfAngleDegrees = 3.0f;
	Request.RandomSeed = 4242;

	const FVector FirstDirection = UStulWeaponShootingLibrary::CalculateShotDirection(Request);
	const FVector SecondDirection = UStulWeaponShootingLibrary::CalculateShotDirection(Request);
	TestTrue(TEXT("The same request and seed produce the same shot direction"), FirstDirection.Equals(SecondDirection, UE_KINDA_SMALL_NUMBER));
	TestTrue(TEXT("Calculated shot direction stays normalized"), FMath::IsNearlyEqual(FirstDirection.SizeSquared(), 1.0f, UE_KINDA_SMALL_NUMBER));

	const FTransform MuzzleTransform(FRotator::ZeroRotator, FVector(10.0, 20.0, 30.0));
	const FVector MuzzleOffset = UStulWeaponShootingLibrary::CalculateMuzzleOffset(MuzzleTransform, FVector2D(2.0, -3.0), 4.0f);
	TestTrue(TEXT("Muzzle pattern uses the local up and right axes"), MuzzleOffset.Equals(FVector(10.0, 8.0, 38.0), UE_KINDA_SMALL_NUMBER));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStulWeaponServerViewValidationTest, "StulWeaponSystem.Shooting.ServerViewValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStulWeaponServerViewValidationTest::RunTest(const FString& Parameters)
{
	const FVector ServerOrigin(100.0, 200.0, 300.0);
	FVector TraceOrigin;
	FVector AimDirection;

	TestTrue(TEXT("A plausible client view is accepted"), UStulWeaponShootingLibrary::ValidateAndResolveClientView(ServerOrigin + FVector(100.0, 0.0, 0.0), FRotator(0.0, 20.0, 0.0).Vector(), ServerOrigin, 150.0f, TraceOrigin, AimDirection));
	TestTrue(TEXT("The authoritative trace origin is reconstructed from the server view"), TraceOrigin.Equals(ServerOrigin, UE_KINDA_SMALL_NUMBER));
	TestTrue(TEXT("An accepted client direction remains normalized"), FMath::IsNearlyEqual(AimDirection.SizeSquared(), 1.0f, UE_KINDA_SMALL_NUMBER));
	TestFalse(TEXT("An origin outside the accepted radius is rejected"), UStulWeaponShootingLibrary::ValidateAndResolveClientView(ServerOrigin + FVector(151.0, 0.0, 0.0), FVector::ForwardVector, ServerOrigin, 150.0f, TraceOrigin, AimDirection));
	TestFalse(TEXT("A zero direction is rejected"), UStulWeaponShootingLibrary::ValidateAndResolveClientView(ServerOrigin, FVector::ZeroVector, ServerOrigin, 150.0f, TraceOrigin, AimDirection));
	TestFalse(TEXT("Non-finite target data is rejected"), UStulWeaponShootingLibrary::ValidateAndResolveClientView(FVector(std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0), FVector::ForwardVector, ServerOrigin, 150.0f, TraceOrigin, AimDirection));
	return true;
}
#endif
