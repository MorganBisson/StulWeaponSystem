#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystem/Cues/StulWeaponTracerCue.h"
#include "Components/StulWeaponFireComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Projectiles/StulWeaponProjectile.h"
#include "TimerManager.h"
#include "Weapons/StulWeapon.h"
#include "Weapons/Shooting/StulWeaponFireSessionTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStulWeaponFireIdentityTest, "StulWeaponSystem.FireSession.Identity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStulWeaponFireIdentityTest::RunTest(const FString& Parameters)
{
	FStulShotId InvalidShotId;
	TestFalse(TEXT("A default shot identity is invalid"), InvalidShotId.IsValid());

	FStulShotId ShotId;
	ShotId.SessionId = 42;
	ShotId.ShotSequence = MAX_uint32;
	TestTrue(TEXT("A non-zero session produces a valid shot identity"), ShotId.IsValid());
	TestEqual(TEXT("A shot identity has a stable readable representation"), ShotId.ToString(), FString(TEXT("42:4294967295")));

	FStulProjectileId FirstProjectileId;
	FirstProjectileId.ShotId = ShotId;
	FirstProjectileId.ProjectileIndex = 3;
	FStulProjectileId SameProjectileId = FirstProjectileId;
	FStulProjectileId OtherProjectileId = FirstProjectileId;
	OtherProjectileId.ProjectileIndex = 4;

	TestTrue(TEXT("A projectile inherits validity from its shot identity"), FirstProjectileId.IsValid());
	TestTrue(TEXT("Equal projectile identities compare equally"), FirstProjectileId == SameProjectileId);
	TestFalse(TEXT("Different projectile indices remain distinct"), FirstProjectileId == OtherProjectileId);
	TestEqual(TEXT("Equal projectile identities produce equal hashes"), GetTypeHash(FirstProjectileId), GetTypeHash(SameProjectileId));
	TestEqual(TEXT("A projectile identity has a stable readable representation"), FirstProjectileId.ToString(), FString(TEXT("42:4294967295:3")));

	FStulCapturedView CapturedView;
	CapturedView.Direction = FVector::ZeroVector;
	TestFalse(TEXT("A captured view rejects a zero direction"), CapturedView.IsValid());
	CapturedView.Direction = FVector::ForwardVector;
	TestTrue(TEXT("A captured view accepts a non-zero direction"), CapturedView.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStulWeaponFireTransportContractTest, "StulWeaponSystem.FireSession.TransportContract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStulWeaponFireTransportContractTest::RunTest(const FString& Parameters)
{
	FStulShotCommand FirstCommand;
	FirstCommand.ShotSequence = 10;
	FirstCommand.CapturedView.Direction = FVector::ForwardVector;
	FStulShotCommand SecondCommand = FirstCommand;
	SecondCommand.ShotSequence = 11;

	FStulShotBatch Batch;
	Batch.SessionId = 7;
	Batch.SessionType = EStulFireSessionType::Automatic;
	Batch.FirstSequence = 10;
	Batch.Commands = {FirstCommand, SecondCommand};
	TestTrue(TEXT("A contiguous self-describing batch is valid"), Batch.IsConsistent());

	Batch.Commands[1].ShotSequence = 12;
	TestFalse(TEXT("A batch with a contradictory command sequence is rejected"), Batch.IsConsistent());
	Batch.Commands[1].ShotSequence = 11;
	Batch.SessionId = 0;
	TestFalse(TEXT("A batch without a session identity is rejected"), Batch.IsConsistent());

	FStulCloseFireSessionRequest CloseRequest;
	CloseRequest.SessionId = 7;
	CloseRequest.FinalSequence = 11;
	CloseRequest.TailCommands = {FirstCommand, SecondCommand};
	TestTrue(TEXT("A close request accepts a contiguous tail ending at FinalSequence"), CloseRequest.IsValid());
	CloseRequest.FinalSequence = 12;
	TestFalse(TEXT("A close request rejects a tail that does not end at FinalSequence"), CloseRequest.IsValid());

	FStulFireAck Ack;
	Ack.SessionId = 7;
	Ack.Revision = 1;
	Ack.ResolvedAheadMask = 1ull << 2;
	Ack.AcceptedAheadMask = 1ull << 2;
	TestTrue(TEXT("An ACK accepts decisions that are a subset of resolved sequences"), Ack.IsValid());
	Ack.AcceptedAheadMask |= 1ull << 3;
	TestFalse(TEXT("An ACK rejects accepted sequences that are not resolved"), Ack.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStulWeaponFireServerProcessingTimerTest, "StulWeaponSystem.FireSession.ServerProcessingTimer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStulWeaponFireServerProcessingTimerTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("A test world can be created"), World)) return false;

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);
	World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false));

	AStulWeapon* Weapon = World->SpawnActor<AStulWeapon>();
	UStulWeaponFireComponent* FireComponent = Weapon ? Weapon->GetFireComponent() : nullptr;
	if (TestNotNull(TEXT("A spawned weapon owns its fire component"), FireComponent))
	{
		FireComponent->ServerSession.State = EStulFireSessionState::Closing;
		FireComponent->ServerSession.SessionId = 1;
		FireComponent->ServerSession.FinalSequence = 1;
		FireComponent->ScheduleServerProcessing(KINDA_SMALL_NUMBER);
		World->GetTimerManager().Tick(1.0f);
		TestEqual(TEXT("An elapsed processing timer resumes and finalizes an unresolved closing session"), FireComponent->ServerSession.State, EStulFireSessionState::None);

		FireComponent->ServerSession.State = EStulFireSessionState::Closing;
		FireComponent->ServerSession.SessionId = 2;
		FireComponent->ServerSession.FinalSequence = 0;
		FireComponent->ScheduleServerProcessing(1.0f);
		FireComponent->ProcessServerCommands();
		TestEqual(TEXT("A fully resolved closing session finalizes without waiting for its cadence timer"), FireComponent->ServerSession.State, EStulFireSessionState::None);
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStulWeaponFireTimelineTest, "StulWeaponSystem.FireSession.Timeline", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStulWeaponFireTimelineTest::RunTest(const FString& Parameters)
{
	constexpr double PreviousShotTime = 10.0;
	constexpr float FireInterval = 0.1f;
	constexpr double LateCallbackTime = 10.03;
	const double NextShotTime = UStulWeaponFireComponent::AdvanceShotTime(PreviousShotTime, FireInterval);
	const float RemainingDelay = UStulWeaponFireComponent::GetShotScheduleDelay(NextShotTime, LateCallbackTime);

	TestTrue(TEXT("A late callback preserves the session timeline instead of accumulating its delay"), FMath::IsNearlyEqual(NextShotTime, 10.1, UE_DOUBLE_SMALL_NUMBER));
	TestTrue(TEXT("Only the time remaining until the absolute deadline is scheduled"), FMath::IsNearlyEqual(RemainingDelay, 0.07f, UE_KINDA_SMALL_NUMBER));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStulWeaponProjectileRemoteVisualOriginTest, "StulWeaponSystem.Projectile.RemoteVisualUsesLocalMuzzle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStulWeaponProjectileRemoteVisualOriginTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("A test world can be created"), World)) return false;

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);
	World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false));

	AStulWeapon* Weapon = World->SpawnActor<AStulWeapon>();
	const FVector LocalWeaponLocation(150.0, -75.0, 25.0);
	const FVector ReplicatedServerMuzzle(5000.0, 5000.0, 5000.0);
	if (TestNotNull(TEXT("A local weapon representation can be spawned"), Weapon))
	{
		Weapon->SetActorLocation(LocalWeaponLocation);
		TestTrue(TEXT("A remote projectile visual starts from the local weapon muzzle"), AStulWeaponProjectile::ResolveSimulatedCosmeticOrigin(Weapon, ReplicatedServerMuzzle).Equals(LocalWeaponLocation, UE_KINDA_SMALL_NUMBER));
	}
	TestTrue(TEXT("The replicated origin remains a fallback when the source weapon is unavailable"), AStulWeaponProjectile::ResolveSimulatedCosmeticOrigin(nullptr, ReplicatedServerMuzzle).Equals(ReplicatedServerMuzzle, UE_KINDA_SMALL_NUMBER));

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStulWeaponHitscanTracerOriginTest, "StulWeaponSystem.HitscanTracer.RemoteVisualUsesLocalMuzzle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStulWeaponHitscanTracerOriginTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("A test world can be created"), World)) return false;

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);
	World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false));

	AStulWeapon* Weapon = World->SpawnActor<AStulWeapon>();
	const FVector LocalMuzzle(150.0, -75.0, 25.0);
	const FVector EncodedServerMuzzle(50.0, 25.0, -40.0);
	const FVector AuthoritativeTarget(1050.0, 25.0, -40.0);
	if (TestNotNull(TEXT("A local weapon representation can be spawned"), Weapon))
	{
		Weapon->SetActorLocation(LocalMuzzle);
		FGameplayCueParameters CueParameters;
		CueParameters.Location = EncodedServerMuzzle;
		CueParameters.Normal = FVector::ForwardVector;
		CueParameters.RawMagnitude = 1000.0f;

		FVector TracerStart;
		FVector TracerTarget;
		TestTrue(TEXT("A valid encoded tracer path can be resolved"), UStulWeaponTracerCue::ResolveTracerEndpoints(*Weapon, CueParameters, TracerStart, TracerTarget));
		TestTrue(TEXT("A remote hitscan tracer starts at the local weapon muzzle"), TracerStart.Equals(LocalMuzzle, UE_KINDA_SMALL_NUMBER));
		TestTrue(TEXT("Changing the presentation origin preserves the authoritative endpoint"), TracerTarget.Equals(AuthoritativeTarget, UE_KINDA_SMALL_NUMBER));
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif
