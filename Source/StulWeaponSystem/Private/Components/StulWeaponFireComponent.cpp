#include "Components/StulWeaponFireComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/StulWeaponAttributeSet.h"
#include "Components/StulWeaponManagerComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameplayAbilitySpec.h"
#include "Interfaces/StulWeaponOwnerInterface.h"
#include "Net/UnrealNetwork.h"
#include "Projectiles/StulWeaponProjectile.h"
#include "StulWeaponGameplayTags.h"
#include "StulWeaponSystem.h"
#include "TimerManager.h"
#include "Weapons/Shooting/StulWeaponShootingLibrary.h"
#include "Weapons/StulWeapon.h"
#include "Weapons/StulWeaponDefinition.h"

UStulWeaponFireComponent::UStulWeaponFireComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

/*********************************************************************************************/
/*************************************** Component ******************************************/
/*********************************************************************************************/
void UStulWeaponFireComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UStulWeaponFireComponent, OwnerReconciliationState, COND_OwnerOnly, REPNOTIFY_Always);
}

void UStulWeaponFireComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LocalSession.ShotTimer);
		World->GetTimerManager().ClearTimer(ServerSession.ProcessTimer);
	}
	AbilityAuthorization.Reset();
	LocalSession.Reset();
	ServerSession.Reset();
	PendingLocalShotCosts.Reset();
	Super::EndPlay(EndPlayReason);
}

void UStulWeaponFireComponent::FAbilityAuthorization::Reset()
{
	AbilityHandle = FGameplayAbilitySpecHandle();
	SessionType = EStulFireSessionType::Single;
	AbilityInstance.Reset();
	AuthorizeShot.Unbind();
	SessionEnded.Unbind();
}

void UStulWeaponFireComponent::FLocalFireSession::Reset()
{
	State = EStulFireSessionState::None;
	Type = EStulFireSessionType::Single;
	ExecutionRole = EStulFireExecutionRole::PresentationOnly;
	SessionId = 0;
	NextSequence = 1;
	ShotsRemaining = 0;
	FireInterval = 0.0f;
	NextShotTime = 0.0;
	CommandHistory.Reset();
	ShotTimer.Invalidate();
}

void UStulWeaponFireComponent::FServerFireSession::Reset()
{
	State = EStulFireSessionState::None;
	Type = EStulFireSessionType::Single;
	SessionId = 0;
	ResolvedThroughSequence = 0;
	FinalSequence = 0;
	AckRevision = 0;
	AcceptedThroughMask = 0;
	WaitingForMissingSequence = 0;
	FireInterval = 0.0f;
	NextShotTime = 0.0;
	BufferedCommands.Reset();
	ProcessTimer.Invalidate();
}

/*********************************************************************************************/
/*********************************** Execution Role ******************************************/
/*********************************************************************************************/
EStulFireExecutionRole UStulWeaponFireComponent::ResolveExecutionRole() const
{
	const AStulWeapon* Weapon = GetWeapon();
	const APawn* Pawn = GetOwningPawn();
	if (!Weapon || !Pawn || !Pawn->GetController()) return EStulFireExecutionRole::PresentationOnly;

	if (Weapon->HasAuthority()) return Pawn->IsLocallyControlled() ? EStulFireExecutionRole::AuthoritativeProducer : EStulFireExecutionRole::AuthoritativeConsumer;
	return Pawn->IsLocallyControlled() ? EStulFireExecutionRole::PredictiveProducer : EStulFireExecutionRole::PresentationOnly;
}

/*********************************************************************************************/
/******************************* Ability Authorization ***************************************/
/*********************************************************************************************/
bool UStulWeaponFireComponent::RegisterAbilityAuthorization(const EStulFireSessionType SessionType, const FGameplayAbilitySpecHandle AbilityHandle, UObject* AbilityInstance, FStulAuthorizeWeaponShot AuthorizeShot, FStulFireSessionEnded SessionEnded)
{
	if (!AbilityHandle.IsValid() || !IsValid(AbilityInstance) || !AuthorizeShot.IsBound()) return false;
	if (AbilityAuthorization.IsValid() && AbilityAuthorization.AbilityInstance.Get() != AbilityInstance) InterruptActiveSession();

	AbilityAuthorization.AbilityHandle = AbilityHandle;
	AbilityAuthorization.SessionType = SessionType;
	AbilityAuthorization.AbilityInstance = AbilityInstance;
	AbilityAuthorization.AuthorizeShot = MoveTemp(AuthorizeShot);
	AbilityAuthorization.SessionEnded = MoveTemp(SessionEnded);
	UE_LOG(LogStulWeaponSystem, Log, TEXT("Fire authorization registered on '%s' (authority: %s, role: %d, type: %d, handle: %s)."), *GetNameSafe(GetOwner()), GetOwner() && GetOwner()->HasAuthority() ? TEXT("true") : TEXT("false"), static_cast<int32>(ResolveExecutionRole()), static_cast<int32>(SessionType), *AbilityHandle.ToString());
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		TryCompleteServerOpening();
	}
	return true;
}

void UStulWeaponFireComponent::UnregisterAbilityAuthorization(UObject* AbilityInstance, const bool bWasCancelled)
{
	if (AbilityAuthorization.AbilityInstance.Get() != AbilityInstance) return;
	AbilityAuthorization.Reset();
	if (bCompletingAbilityCallback) return;

	if (LocalSession.State != EStulFireSessionState::None) CloseLocalSession(true);
	if (GetOwner() && GetOwner()->HasAuthority() && ServerSession.State != EStulFireSessionState::None) FinalizeServerSession(bWasCancelled ? EStulFireSessionFinalState::Interrupted : EStulFireSessionFinalState::Rejected);
}

/*********************************************************************************************/
/************************************ Fire Lifecycle *****************************************/
/*********************************************************************************************/
bool UStulWeaponFireComponent::StartSingle()
{
	return StartSession(EStulFireSessionType::Single);
}

bool UStulWeaponFireComponent::StartBurst()
{
	return StartSession(EStulFireSessionType::Burst);
}

bool UStulWeaponFireComponent::StartAutomatic()
{
	return StartSession(EStulFireSessionType::Automatic);
}

bool UStulWeaponFireComponent::StartSession(const EStulFireSessionType SessionType)
{
	AStulWeapon* Weapon = GetWeapon();
	if (!Weapon || !Weapon->CanPredictWeaponFire() || !IsWeaponEquipped() || !IsAbilityAuthorizationValid(SessionType)) return false;

	const EStulFireExecutionRole ExecutionRole = ResolveExecutionRole();
	if (ExecutionRole == EStulFireExecutionRole::AuthoritativeConsumer) return true;
	if (ExecutionRole == EStulFireExecutionRole::PresentationOnly || LocalSession.State != EStulFireSessionState::None || NextLocalSessionId == MAX_uint32) return false;
	const UWorld* World = GetWorld();
	if (!World || World->GetTimeSeconds() + KINDA_SMALL_NUMBER < EarliestNextLocalShotTime) return false;

	LocalSession.State = EStulFireSessionState::Active;
	LocalSession.Type = SessionType;
	LocalSession.ExecutionRole = ExecutionRole;
	LocalSession.SessionId = ++NextLocalSessionId;
	LocalSession.NextSequence = 1;
	LocalSession.ShotsRemaining = SessionType == EStulFireSessionType::Burst ? GetBurstSize() : 0;
	LocalSession.FireInterval = GetFireInterval();
	LocalSession.NextShotTime = World->GetTimeSeconds();
	LatestAuthoritativeAmmoBaseline = GetAuthoritativeAmmoSnapshot();
	UE_LOG(LogStulWeaponSystem, Log, TEXT("Local fire session %u started on '%s' (role: %d, type: %d, burst shots: %d)."), LocalSession.SessionId, *GetNameSafe(Weapon), static_cast<int32>(ExecutionRole), static_cast<int32>(SessionType), LocalSession.ShotsRemaining);

	DispatchFiringStarted();
	if (ExecutionRole == EStulFireExecutionRole::PredictiveProducer)
	{
		FStulOpenFireSessionRequest Request;
		Request.SessionId = LocalSession.SessionId;
		Request.SessionType = SessionType;
		ServerOpenFireSession(Request);
	}
	ProduceLocalShot();
	return true;
}

void UStulWeaponFireComponent::ProduceLocalShot()
{
	if (LocalSession.State != EStulFireSessionState::Active || !AbilityAuthorization.IsValid()) return;

	FStulCapturedView CapturedView;
	if (!CaptureView(CapturedView))
	{
		CloseLocalSession(true);
		return;
	}

	const FStulShotId ShotId{LocalSession.SessionId, LocalSession.NextSequence};
	if (!AbilityAuthorization.AuthorizeShot.Execute(ShotId))
	{
		CloseLocalSession(false);
		return;
	}

	FStulWeaponShotExecution ShotExecution;
	const bool bAuthoritative = LocalSession.ExecutionRole == EStulFireExecutionRole::AuthoritativeProducer;
	if (!ExecuteShot(ShotId, CapturedView.Origin, CapturedView.Direction, bAuthoritative, ShotExecution))
	{
		CloseLocalSession(true);
		return;
	}

	FStulShotCommand& Command = LocalSession.CommandHistory.Emplace_GetRef();
	Command.ShotSequence = LocalSession.NextSequence;
	Command.CapturedView = CapturedView;
	if (LocalSession.ExecutionRole == EStulFireExecutionRole::PredictiveProducer)
	{
		const UStulWeaponAttributeSet* Attributes = GetWeapon() ? GetWeapon()->GetWeaponAttributeSet() : nullptr;
		const int32 PendingCost = LocalSession.Type == EStulFireSessionType::Single ? 0 : FMath::Max(0, FMath::RoundToInt(Attributes ? Attributes->GetFireCost() : 0.0f));
		PendingLocalShotCosts.Add(ShotId, PendingCost);
	}

	DispatchShotExecuted(ShotExecution);
	LocalSession.NextShotTime = AdvanceShotTime(LocalSession.NextShotTime, LocalSession.FireInterval);
	EarliestNextLocalShotTime = LocalSession.NextShotTime;
	if (LocalSession.ExecutionRole == EStulFireExecutionRole::PredictiveProducer) SendCurrentShotBatch();
	++LocalSession.NextSequence;

	if (LocalSession.Type == EStulFireSessionType::Single)
	{
		CloseLocalSession(false);
		return;
	}
	if (LocalSession.Type == EStulFireSessionType::Burst && --LocalSession.ShotsRemaining <= 0)
	{
		CloseLocalSession(false);
		return;
	}
	ScheduleLocalShot();
}

void UStulWeaponFireComponent::ScheduleLocalShot()
{
	if (UWorld* World = GetWorld()) World->GetTimerManager().SetTimer(LocalSession.ShotTimer, this, &ThisClass::ProduceLocalShot, GetShotScheduleDelay(LocalSession.NextShotTime, World->GetTimeSeconds()), false);
}

void UStulWeaponFireComponent::RequestStopAutomatic()
{
	if (LocalSession.State == EStulFireSessionState::Active && LocalSession.Type == EStulFireSessionType::Automatic) CloseLocalSession(false);
}

void UStulWeaponFireComponent::InterruptActiveSession()
{
	if (LocalSession.State != EStulFireSessionState::None) CloseLocalSession(true);
	if (GetOwner() && GetOwner()->HasAuthority() && ServerSession.State != EStulFireSessionState::None) FinalizeServerSession(EStulFireSessionFinalState::Interrupted);
}

void UStulWeaponFireComponent::PresentAuthoritativeProjectileImpact(const FHitResult& HitResult, const float Damage, AActor* EffectCauser)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	MulticastPresentHitscanImpact(HitResult, Damage, EffectCauser, false);
}

void UStulWeaponFireComponent::CloseLocalSession(const bool bWasCancelled)
{
	if (LocalSession.State == EStulFireSessionState::None) return;

	LocalSession.State = EStulFireSessionState::Closing;
	UE_LOG(LogStulWeaponSystem, Log, TEXT("Local fire session %u closed on '%s' (type: %d, final sequence: %u, cancelled: %s)."), LocalSession.SessionId, *GetNameSafe(GetOwner()), static_cast<int32>(LocalSession.Type), LocalSession.NextSequence > 1 ? LocalSession.NextSequence - 1 : 0, bWasCancelled ? TEXT("true") : TEXT("false"));
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(LocalSession.ShotTimer);
	if (LocalSession.ExecutionRole == EStulFireExecutionRole::PredictiveProducer) ServerCloseFireSession(MakeCloseRequest());

	DispatchFiringEnded();
	LocalSession.Reset();
	NotifyAbilitySessionEnded(bWasCancelled);
}

void UStulWeaponFireComponent::NotifyAbilitySessionEnded(const bool bWasCancelled)
{
	if (!AbilityAuthorization.SessionEnded.IsBound()) return;

	FStulFireSessionEnded Callback = AbilityAuthorization.SessionEnded;
	bCompletingAbilityCallback = true;
	Callback.Execute(bWasCancelled);
	bCompletingAbilityCallback = false;
}

float UStulWeaponFireComponent::GetFireInterval() const
{
	const AStulWeapon* Weapon = GetWeapon();
	const UStulWeaponAttributeSet* Attributes = Weapon ? Weapon->GetWeaponAttributeSet() : nullptr;
	return FMath::Max(Attributes ? Attributes->GetFireInterval() : 0.0f, KINDA_SMALL_NUMBER);
}

int32 UStulWeaponFireComponent::GetBurstSize() const
{
	const AStulWeapon* Weapon = GetWeapon();
	const UStulWeaponAttributeSet* Attributes = Weapon ? Weapon->GetWeaponAttributeSet() : nullptr;
	return FMath::Max(1, FMath::FloorToInt(Attributes ? Attributes->GetBurstShotCount() : 1.0f));
}

double UStulWeaponFireComponent::AdvanceShotTime(const double PreviousShotTime, const float FireInterval)
{
	return PreviousShotTime + FMath::Max(static_cast<double>(FireInterval), static_cast<double>(KINDA_SMALL_NUMBER));
}

float UStulWeaponFireComponent::GetShotScheduleDelay(const double ShotTime, const double CurrentTime)
{
	return static_cast<float>(FMath::Max(ShotTime - CurrentTime, static_cast<double>(KINDA_SMALL_NUMBER)));
}

/*********************************************************************************************/
/*********************************** Session Opening *****************************************/
/*********************************************************************************************/
void UStulWeaponFireComponent::ServerOpenFireSession_Implementation(const FStulOpenFireSessionRequest& Request)
{
	UE_LOG(LogStulWeaponSystem, Log, TEXT("Server received OpenSession %u on '%s' (last: %u, server state: %d, authorization: %s)."), Request.SessionId, *GetNameSafe(GetOwner()), LastServerSessionId, static_cast<int32>(ServerSession.State), AbilityAuthorization.IsValid() ? TEXT("valid") : TEXT("missing"));
	if (!Request.IsValid() || Request.SessionId <= LastServerSessionId)
	{
		UE_LOG(LogStulWeaponSystem, Warning, TEXT("Fire component '%s' rejected stale or invalid OpenSession %u."), *GetNameSafe(this), Request.SessionId);
		return;
	}
	if (ServerSession.State != EStulFireSessionState::None)
	{
		UE_LOG(LogStulWeaponSystem, Verbose, TEXT("Fire component '%s' ignored OpenSession %u while server session %u is still active."), *GetNameSafe(this), Request.SessionId, ServerSession.SessionId);
		return;
	}

	PendingOpenRequest = Request;
	TryCompleteServerOpening();
}

void UStulWeaponFireComponent::TryCompleteServerOpening()
{
	if (!PendingOpenRequest.IsSet() || ServerSession.State != EStulFireSessionState::None || !IsAbilityAuthorizationValid(PendingOpenRequest->SessionType)) return;
	if (!ValidateServerOpening(PendingOpenRequest.GetValue()))
	{
		PendingOpenRequest.Reset();
		NotifyAbilitySessionEnded(true);
		return;
	}

	const FStulOpenFireSessionRequest Request = PendingOpenRequest.GetValue();
	PendingOpenRequest.Reset();
	StartServerSession(Request);
}

bool UStulWeaponFireComponent::ValidateServerOpening(const FStulOpenFireSessionRequest& Request) const
{
	const AStulWeapon* Weapon = GetWeapon();
	const APawn* Pawn = GetOwningPawn();
	const UAbilitySystemComponent* AbilitySystem = Weapon ? Weapon->GetAbilitySystemComponent() : nullptr;
	const FGameplayAbilitySpec* AbilitySpec = AbilitySystem ? AbilitySystem->FindAbilitySpecFromHandle(AbilityAuthorization.AbilityHandle) : nullptr;
	const FGameplayAbilityActorInfo* ActorInfo = AbilitySystem ? AbilitySystem->AbilityActorInfo.Get() : nullptr;
	return Request.IsValid() && Weapon && Pawn && Weapon->HasAuthority() && ResolveExecutionRole() == EStulFireExecutionRole::AuthoritativeConsumer && IsWeaponEquipped()
		&& AbilitySpec && AbilitySpec->IsActive() && AbilitySpec->GetPrimaryInstance() == AbilityAuthorization.AbilityInstance.Get()
		&& ActorInfo && ActorInfo->AvatarActor.Get() == Weapon && ActorInfo->OwnerActor.Get() == Pawn;
}

void UStulWeaponFireComponent::StartServerSession(const FStulOpenFireSessionRequest& Request)
{
	ServerSession.State = EStulFireSessionState::Active;
	ServerSession.Type = Request.SessionType;
	ServerSession.SessionId = Request.SessionId;
	ServerSession.FireInterval = GetFireInterval();
	if (const UWorld* World = GetWorld()) ServerSession.NextShotTime = FMath::Max(static_cast<double>(World->GetTimeSeconds()), EarliestNextAuthoritativeShotTime);
	LastServerSessionId = Request.SessionId;
	UE_LOG(LogStulWeaponSystem, Log, TEXT("Server fire session %u became Active on '%s' (type: %d, orphan batches: %d)."), Request.SessionId, *GetNameSafe(GetOwner()), static_cast<int32>(Request.SessionType), OrphanBatches.Num());
	DispatchFiringStarted();

	for (const FStulShotBatch& Batch : OrphanBatches)
	{
		if (Batch.SessionId == Request.SessionId && Batch.SessionType == Request.SessionType) BufferServerBatch(Batch);
	}
	OrphanBatches.RemoveAll([&Request](const FStulShotBatch& Batch) { return Batch.SessionId <= Request.SessionId; });

	if (PendingCloseRequest.IsSet() && PendingCloseRequest->SessionId == Request.SessionId)
	{
		const FStulCloseFireSessionRequest CloseRequest = PendingCloseRequest.GetValue();
		PendingCloseRequest.Reset();
		ServerCloseFireSession_Implementation(CloseRequest);
	}
	ProcessServerCommands();
}

/*********************************************************************************************/
/*********************************** Shot Transport ******************************************/
/*********************************************************************************************/
void UStulWeaponFireComponent::SendCurrentShotBatch()
{
	const int32 CommandCount = FMath::Min(FMath::Max(1, RedundantCommandCount), LocalSession.CommandHistory.Num());
	if (CommandCount <= 0) return;

	FStulShotBatch Batch;
	Batch.SessionId = LocalSession.SessionId;
	Batch.SessionType = LocalSession.Type;
	Batch.Commands.Append(LocalSession.CommandHistory.GetData() + LocalSession.CommandHistory.Num() - CommandCount, CommandCount);
	Batch.FirstSequence = Batch.Commands[0].ShotSequence;
	ServerReceiveShotBatch(Batch);
}

FStulCloseFireSessionRequest UStulWeaponFireComponent::MakeCloseRequest() const
{
	FStulCloseFireSessionRequest Request;
	Request.SessionId = LocalSession.SessionId;
	Request.FinalSequence = LocalSession.NextSequence > 1 ? LocalSession.NextSequence - 1 : 0;
	const int32 CommandCount = FMath::Min(FMath::Max(1, RedundantCommandCount), LocalSession.CommandHistory.Num());
	if (CommandCount > 0) Request.TailCommands.Append(LocalSession.CommandHistory.GetData() + LocalSession.CommandHistory.Num() - CommandCount, CommandCount);
	return Request;
}

void UStulWeaponFireComponent::ServerReceiveShotBatch_Implementation(const FStulShotBatch& Batch)
{
	UE_LOG(LogStulWeaponSystem, Verbose, TEXT("Server received fire batch for session %u on '%s' (first: %u, count: %d, server session: %u, state: %d)."), Batch.SessionId, *GetNameSafe(GetOwner()), Batch.FirstSequence, Batch.Commands.Num(), ServerSession.SessionId, static_cast<int32>(ServerSession.State));
	if (!Batch.IsConsistent()) return;
	if (ServerSession.State == EStulFireSessionState::None || ServerSession.SessionId != Batch.SessionId)
	{
		if (Batch.SessionId > LastServerSessionId)
		{
			OrphanBatches.Add(Batch);
			if (OrphanBatches.Num() > 4) OrphanBatches.RemoveAt(0, OrphanBatches.Num() - 4, EAllowShrinking::No);
		}
		return;
	}
	if (Batch.SessionType != ServerSession.Type) return;

	BufferServerBatch(Batch);
	ProcessServerCommands();
}

void UStulWeaponFireComponent::BufferServerBatch(const FStulShotBatch& Batch)
{
	BufferServerCommands(Batch.Commands);
}

void UStulWeaponFireComponent::BufferServerCommands(const TArray<FStulShotCommand>& Commands)
{
	for (const FStulShotCommand& Command : Commands)
	{
		if (!Command.IsValid() || Command.ShotSequence <= ServerSession.ResolvedThroughSequence || Command.ShotSequence > ServerSession.ResolvedThroughSequence + MaxSequenceLead) continue;
		ServerSession.BufferedCommands.FindOrAdd(Command.ShotSequence, Command);
	}
}

void UStulWeaponFireComponent::ServerCloseFireSession_Implementation(const FStulCloseFireSessionRequest& Request)
{
	UE_LOG(LogStulWeaponSystem, Log, TEXT("Server received CloseSession %u on '%s' (final sequence: %u, tail: %d, server session: %u, state: %d)."), Request.SessionId, *GetNameSafe(GetOwner()), Request.FinalSequence, Request.TailCommands.Num(), ServerSession.SessionId, static_cast<int32>(ServerSession.State));
	if (!Request.IsValid()) return;
	if (ServerSession.State == EStulFireSessionState::None || ServerSession.SessionId != Request.SessionId)
	{
		if (Request.SessionId > LastServerSessionId) PendingCloseRequest = Request;
		return;
	}
	if (Request.FinalSequence < ServerSession.ResolvedThroughSequence || Request.FinalSequence > ServerSession.ResolvedThroughSequence + MaxSequenceLead)
	{
		FinalizeServerSession(EStulFireSessionFinalState::Rejected);
		return;
	}

	BufferServerCommands(Request.TailCommands);
	ServerSession.FinalSequence = Request.FinalSequence;
	ServerSession.State = EStulFireSessionState::Closing;
	ProcessServerCommands();
}

void UStulWeaponFireComponent::ProcessServerCommands()
{
	if (ServerSession.State != EStulFireSessionState::Active && ServerSession.State != EStulFireSessionState::Closing) return;
	if (ServerSession.State == EStulFireSessionState::Closing && ServerSession.ResolvedThroughSequence >= ServerSession.FinalSequence)
	{
		FinalizeServerSession(EStulFireSessionFinalState::Completed);
		return;
	}

	UWorld* World = GetWorld();
	if (!World || World->GetTimerManager().IsTimerActive(ServerSession.ProcessTimer)) return;
	const double Now = World->GetTimeSeconds();
	if (Now + KINDA_SMALL_NUMBER < ServerSession.NextShotTime)
	{
		ScheduleServerProcessing(GetShotScheduleDelay(ServerSession.NextShotTime, Now));
		return;
	}

	const uint32 NextSequence = ServerSession.ResolvedThroughSequence + 1;
	if (FStulShotCommand* Command = ServerSession.BufferedCommands.Find(NextSequence))
	{
		const FStulShotCommand CommandCopy = *Command;
		ServerSession.BufferedCommands.Remove(NextSequence);
		ServerSession.WaitingForMissingSequence = 0;
		ResolveServerCommand(CommandCopy);
		if (ServerSession.State == EStulFireSessionState::Closing && ServerSession.ResolvedThroughSequence >= ServerSession.FinalSequence) FinalizeServerSession(EStulFireSessionFinalState::Completed);
		else if (ServerSession.State != EStulFireSessionState::None) ScheduleServerProcessing(GetShotScheduleDelay(ServerSession.NextShotTime, World->GetTimeSeconds()));
		return;
	}

	if (ServerSession.State == EStulFireSessionState::Closing)
	{
		ResolveMissingServerCommand();
		ProcessServerCommands();
		return;
	}

	bool bHasLaterCommand = false;
	for (const TPair<uint32, FStulShotCommand>& Pair : ServerSession.BufferedCommands)
	{
		if (Pair.Key > NextSequence)
		{
			bHasLaterCommand = true;
			break;
		}
	}
	if (!bHasLaterCommand) return;
	if (ServerSession.WaitingForMissingSequence == NextSequence)
	{
		ResolveMissingServerCommand();
		ProcessServerCommands();
		return;
	}

	ServerSession.WaitingForMissingSequence = NextSequence;
	ScheduleServerProcessing(FMath::Max(MissingCommandGraceSeconds, KINDA_SMALL_NUMBER));
}

void UStulWeaponFireComponent::ScheduleServerProcessing(const float Delay)
{
	if (UWorld* World = GetWorld()) World->GetTimerManager().SetTimer(ServerSession.ProcessTimer, this, &ThisClass::HandleServerProcessingTimer, FMath::Max(Delay, KINDA_SMALL_NUMBER), false);
}

void UStulWeaponFireComponent::HandleServerProcessingTimer()
{
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(ServerSession.ProcessTimer);
	ProcessServerCommands();
}

bool UStulWeaponFireComponent::ResolveServerCommand(const FStulShotCommand& Command)
{
	const FStulShotId ShotId{ServerSession.SessionId, Command.ShotSequence};
	FVector ViewOrigin;
	FVector ViewDirection;
	const bool bAccepted = ValidateAndResolveView(Command.CapturedView, ViewOrigin, ViewDirection) && AbilityAuthorization.IsValid() && AbilityAuthorization.AuthorizeShot.Execute(ShotId);
	if (bAccepted)
	{
		FStulWeaponShotExecution ShotExecution;
		if (ExecuteShot(ShotId, ViewOrigin, ViewDirection, true, ShotExecution)) DispatchShotExecuted(ShotExecution);
	}

	ServerSession.AcceptedThroughMask = (ServerSession.AcceptedThroughMask << 1) | (bAccepted ? 1ull : 0ull);
	ServerSession.ResolvedThroughSequence = Command.ShotSequence;
	ServerSession.NextShotTime = AdvanceShotTime(ServerSession.NextShotTime, ServerSession.FireInterval);
	EarliestNextAuthoritativeShotTime = ServerSession.NextShotTime;
	SendServerAck();
	return bAccepted;
}

void UStulWeaponFireComponent::ResolveMissingServerCommand()
{
	++ServerSession.ResolvedThroughSequence;
	ServerSession.AcceptedThroughMask <<= 1;
	ServerSession.WaitingForMissingSequence = 0;
	ServerSession.NextShotTime = AdvanceShotTime(ServerSession.NextShotTime, ServerSession.FireInterval);
	EarliestNextAuthoritativeShotTime = ServerSession.NextShotTime;
	SendServerAck();
}

void UStulWeaponFireComponent::FinalizeServerSession(const EStulFireSessionFinalState FinalState)
{
	if (ServerSession.State == EStulFireSessionState::None) return;
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(ServerSession.ProcessTimer);

	const uint32 FinalizedSessionId = ServerSession.SessionId;
	const uint32 ResolvedThrough = ServerSession.ResolvedThroughSequence;
	UE_LOG(LogStulWeaponSystem, Log, TEXT("Server finalized fire session %u on '%s' (state: %d, resolved through: %u, final sequence: %u)."), FinalizedSessionId, *GetNameSafe(GetOwner()), static_cast<int32>(FinalState), ResolvedThrough, ServerSession.FinalSequence);
	OwnerReconciliationState.Revision++;
	OwnerReconciliationState.LatestFinalizedSessionId = FinalizedSessionId;
	OwnerReconciliationState.ResolvedThroughSequence = ResolvedThrough;
	OwnerReconciliationState.AcceptedThroughMask = ServerSession.AcceptedThroughMask;
	OwnerReconciliationState.FinalState = FinalState;
	OwnerReconciliationState.AuthoritativeAmmoSnapshot = GetAuthoritativeAmmoSnapshot();
	SendServerAck();
	DispatchFiringEnded();
	ServerSession.Reset();
	if (PendingOpenRequest.IsSet() && PendingOpenRequest->SessionId <= FinalizedSessionId) PendingOpenRequest.Reset();
	if (PendingCloseRequest.IsSet() && PendingCloseRequest->SessionId <= FinalizedSessionId) PendingCloseRequest.Reset();
	if (AActor* OwnerActor = GetOwner()) OwnerActor->ForceNetUpdate();
	NotifyAbilitySessionEnded(FinalState != EStulFireSessionFinalState::Completed);
}

/*********************************************************************************************/
/*********************************** Reconciliation ******************************************/
/*********************************************************************************************/
void UStulWeaponFireComponent::SendServerAck()
{
	if (ServerSession.SessionId == 0) return;

	FStulFireAck Ack;
	Ack.SessionId = ServerSession.SessionId;
	Ack.Revision = ++ServerSession.AckRevision;
	Ack.ResolvedThroughSequence = ServerSession.ResolvedThroughSequence;
	Ack.AcceptedThroughMask = ServerSession.AcceptedThroughMask;
	Ack.AuthoritativeAmmoSnapshot = GetAuthoritativeAmmoSnapshot();
	ClientReceiveFireAck(Ack);
}

void UStulWeaponFireComponent::ClientReceiveFireAck_Implementation(const FStulFireAck& Ack)
{
	ApplyFireAck(Ack);
}

void UStulWeaponFireComponent::ApplyFireAck(const FStulFireAck& Ack)
{
	if (!Ack.IsValid() || Ack.SessionId < LastAppliedAckSessionId || (Ack.SessionId == LastAppliedAckSessionId && Ack.Revision <= LastAppliedAckRevision)) return;
	LastAppliedAckSessionId = Ack.SessionId;
	LastAppliedAckRevision = Ack.Revision;
	LatestAuthoritativeAmmoBaseline = Ack.AuthoritativeAmmoSnapshot;

	TArray<FStulShotId> ResolvedShots;
	PendingLocalShotCosts.GenerateKeyArray(ResolvedShots);
	for (const FStulShotId& ShotId : ResolvedShots)
	{
		if (ShotId.SessionId != Ack.SessionId || ShotId.ShotSequence > Ack.ResolvedThroughSequence) continue;
		const uint32 Distance = Ack.ResolvedThroughSequence - ShotId.ShotSequence;
		const bool bAccepted = Distance < 64 && (Ack.AcceptedThroughMask & (1ull << Distance)) != 0;
		ResolvePendingShot(ShotId, bAccepted);
	}
}

void UStulWeaponFireComponent::ResolvePendingShot(const FStulShotId& ShotId, const bool bAccepted)
{
	PendingLocalShotCosts.Remove(ShotId);
	if (AStulWeapon* Weapon = GetWeapon())
	{
		if (bAccepted) Weapon->ConfirmPredictedShot(ShotId);
		else Weapon->RejectPredictedShot(ShotId);
	}
}

void UStulWeaponFireComponent::OnRep_OwnerReconciliationState()
{
	NextLocalSessionId = FMath::Max(NextLocalSessionId, OwnerReconciliationState.LatestFinalizedSessionId);
	LatestAuthoritativeAmmoBaseline = OwnerReconciliationState.AuthoritativeAmmoSnapshot;
	TArray<FStulShotId> PendingShots;
	PendingLocalShotCosts.GenerateKeyArray(PendingShots);
	for (const FStulShotId& ShotId : PendingShots)
	{
		if (ShotId.SessionId < OwnerReconciliationState.LatestFinalizedSessionId)
		{
			ResolvePendingShot(ShotId, false);
			continue;
		}
		if (ShotId.SessionId == OwnerReconciliationState.LatestFinalizedSessionId && ShotId.ShotSequence <= OwnerReconciliationState.ResolvedThroughSequence)
		{
			const uint32 Distance = OwnerReconciliationState.ResolvedThroughSequence - ShotId.ShotSequence;
			const bool bAccepted = Distance < 64 && (OwnerReconciliationState.AcceptedThroughMask & (1ull << Distance)) != 0;
			ResolvePendingShot(ShotId, bAccepted);
		}
		else if (ShotId.SessionId == OwnerReconciliationState.LatestFinalizedSessionId && OwnerReconciliationState.FinalState != EStulFireSessionFinalState::Completed)
		{
			ResolvePendingShot(ShotId, false);
		}
	}
}

int32 UStulWeaponFireComponent::GetAuthoritativeAmmoSnapshot() const
{
	const AStulWeapon* Weapon = GetWeapon();
	const UStulWeaponAttributeSet* Attributes = Weapon ? Weapon->GetWeaponAttributeSet() : nullptr;
	return FMath::Max(0, FMath::RoundToInt(Attributes ? Attributes->GetCurrentAmmo() : 0.0f));
}

int32 UStulWeaponFireComponent::GetDisplayedAmmo() const
{
	if (PendingLocalShotCosts.IsEmpty()) return GetAuthoritativeAmmoSnapshot();

	int32 PendingCost = 0;
	for (const TPair<FStulShotId, int32>& Pair : PendingLocalShotCosts) PendingCost += Pair.Value;
	return FMath::Max(0, LatestAuthoritativeAmmoBaseline - PendingCost);
}

/*********************************************************************************************/
/************************************* Shot Execution ****************************************/
/*********************************************************************************************/
bool UStulWeaponFireComponent::ExecuteShot(const FStulShotId& ShotId, const FVector& ViewOrigin, const FVector& ViewDirection, const bool bAuthoritative, FStulWeaponShotExecution& OutShotExecution)
{
	OutShotExecution = FStulWeaponShotExecution();
	AStulWeapon* Weapon = GetWeapon();
	const UStulWeaponDefinition* Definition = Weapon ? Weapon->GetWeaponDefinition() : nullptr;
	const UStulWeaponAttributeSet* Attributes = Weapon ? Weapon->GetWeaponAttributeSet() : nullptr;
	if (!ShotId.IsValid() || !Weapon || !Definition || !Attributes || ViewDirection.IsNearlyZero()) return false;

	TArray<AActor*> IgnoredActors;
	IgnoredActors.Reserve(2);
	IgnoredActors.Add(Weapon);
	if (AActor* OwnerActor = Weapon->GetOwner()) IgnoredActors.Add(OwnerActor);

	const float AimDistance = Definition->ShotType == EStulWeaponShotType::Projectile ? Definition->AimTraceDistance : Definition->MaxRange;
	FVector AimTarget;
	FHitResult AimHit;
	UStulWeaponShootingLibrary::FindAimTarget(this, ViewOrigin, ViewDirection, AimDistance, Definition->AimTraceChannel, IgnoredActors, AimTarget, AimHit);

	const FTransform MuzzleTransform = Weapon->GetMuzzleTransform();
	const int32 ProjectileCount = FMath::Max(1, FMath::FloorToInt(Attributes->GetShotsPerFire()));
	const FStulWeaponShotPattern* ShotPattern = Definition->ShotPatterns.Find(ProjectileCount);
	OutShotExecution.ShotId = ShotId;
	OutShotExecution.ShotSequence = static_cast<int32>(ShotId.ShotSequence);
	OutShotExecution.bAuthoritative = bAuthoritative;
	OutShotExecution.Results.Reserve(ProjectileCount);

	for (int32 ProjectileIndex = 0; ProjectileIndex < ProjectileCount; ++ProjectileIndex)
	{
		const FStulProjectileId ProjectileId{ShotId, static_cast<uint32>(ProjectileIndex)};
		const FVector2D PatternOffset = ShotPattern && ShotPattern->ShotPoints.IsValidIndex(ProjectileIndex) ? ShotPattern->ShotPoints[ProjectileIndex] : FVector2D::ZeroVector;
		const FVector MuzzleLocation = Definition->bUseMuzzleOffset ? UStulWeaponShootingLibrary::CalculateMuzzleOffset(MuzzleTransform, PatternOffset, Definition->MuzzleOffsetScale) : MuzzleTransform.GetLocation();

		FStulWeaponShotRequest ShotRequest;
		ShotRequest.ShotOrigin = ViewOrigin;
		ShotRequest.TargetLocation = AimTarget;
		ShotRequest.PatternOffset = PatternOffset;
		ShotRequest.PatternScale = Attributes->GetPatternScale();
		ShotRequest.SpreadHalfAngleDegrees = Attributes->GetMinSpread();
		ShotRequest.RandomSeed = MakeShotSeed(ProjectileId);

		FStulWeaponShotResult ShotResult;
		ShotResult.TraceOrigin = ViewOrigin;
		ShotResult.MuzzleLocation = MuzzleLocation;
		ShotResult.Direction = UStulWeaponShootingLibrary::CalculateShotDirection(ShotRequest);
		ShotResult.ProjectileIndex = ProjectileIndex;
		ShotResult.bAuthoritative = bAuthoritative;
		ShotResult.EndLocation = ViewOrigin + ShotResult.Direction * AimDistance;

		if (Definition->ShotType == EStulWeaponShotType::Hitscan)
		{
			ExecuteHitscanShot(*Definition, ViewOrigin, IgnoredActors, ShotResult);
		}
		else if (Definition->ShotType == EStulWeaponShotType::Projectile && bAuthoritative && !SpawnAuthoritativeProjectile(*Definition, *Weapon, ViewOrigin, ProjectileId, ShotResult))
		{
			UE_LOG(LogStulWeaponSystem, Error, TEXT("Fire component '%s' failed to spawn projectile %u for shot '%s'."), *GetNameSafe(this), ProjectileId.ProjectileIndex, *ShotId.ToString());
			continue;
		}
		else if (Definition->ShotType == EStulWeaponShotType::Projectile && !bAuthoritative && !SpawnPredictedProjectile(*Definition, *Weapon, ViewOrigin, ProjectileId, ShotResult))
		{
			UE_LOG(LogStulWeaponSystem, Warning, TEXT("Fire component '%s' failed to spawn predicted projectile %u for shot '%s'."), *GetNameSafe(this), ProjectileId.ProjectileIndex, *ShotId.ToString());
		}

		DrawShotDebug(ViewOrigin, AimTarget, AimHit, ShotResult);
		if (Definition->ShotType == EStulWeaponShotType::Hitscan && ShotResult.bBlockingHit)
		{
			if (bAuthoritative)
			{
				const bool bSkipOwningClient = ResolveExecutionRole() == EStulFireExecutionRole::AuthoritativeConsumer;
				Weapon->HandleAuthoritativeHit(ShotResult.HitResult, Attributes->GetDamage(), Weapon, Weapon->GetCurrentAmmoDefinition(), false);
				MulticastPresentHitscanImpact(ShotResult.HitResult, Attributes->GetDamage(), Weapon, bSkipOwningClient);
			}
			else Weapon->ExecuteImpactGameplayCue(ShotResult.HitResult, Attributes->GetDamage(), Weapon, Weapon->GetCurrentAmmoDefinition());
		}
		OutShotExecution.Results.Add(MoveTemp(ShotResult));
	}
	return !OutShotExecution.Results.IsEmpty();
}

void UStulWeaponFireComponent::ExecuteHitscanShot(const UStulWeaponDefinition& Definition, const FVector& ViewOrigin, const TArray<AActor*>& IgnoredActors, FStulWeaponShotResult& ShotResult) const
{
	ShotResult.bBlockingHit = UStulWeaponShootingLibrary::PerformHitscanTrace(this, ViewOrigin, ShotResult.Direction, Definition.MaxRange, Definition.HitscanTraceChannel, IgnoredActors, ShotResult.HitResult, ShotResult.EndLocation);
}

FStulWeaponProjectileInitData UStulWeaponFireComponent::MakeProjectileInitData(const UStulWeaponDefinition& Definition, const FStulProjectileId& ProjectileId, const FStulWeaponShotResult& ShotResult) const
{
	FStulWeaponProjectileInitData ProjectileInitData;
	ProjectileInitData.CosmeticOrigin = ShotResult.MuzzleLocation;
	ProjectileInitData.Direction = ShotResult.Direction;
	ProjectileInitData.Speed = Definition.BaseProjectileSpeed;
	ProjectileInitData.GravityScale = Definition.ProjectileGravityScale;
	ProjectileInitData.MaxLifeSeconds = Definition.ProjectileMaxLifeSeconds;
	ProjectileInitData.ProjectileId = ProjectileId;
	return ProjectileInitData;
}

bool UStulWeaponFireComponent::SpawnPredictedProjectile(const UStulWeaponDefinition& Definition, AStulWeapon& Weapon, const FVector& ViewOrigin, const FStulProjectileId& ProjectileId, const FStulWeaponShotResult& ShotResult) const
{
	UWorld* World = GetWorld();
	const TSubclassOf<AStulWeaponProjectile> ProjectileClass = Definition.ProjectileClass.Get();
	APawn* OwningPawn = Cast<APawn>(Weapon.GetOwner());
	if (!World || !ProjectileClass || !OwningPawn || !OwningPawn->IsLocallyControlled() || Weapon.HasAuthority()) return false;

	const FTransform SpawnTransform(ShotResult.Direction.Rotation(), ViewOrigin);
	AStulWeaponProjectile* Projectile = World->SpawnActorDeferred<AStulWeaponProjectile>(ProjectileClass, SpawnTransform, &Weapon, OwningPawn, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Projectile) return false;

	const FStulWeaponProjectileInitData ProjectileInitData = MakeProjectileInitData(Definition, ProjectileId, ShotResult);
	if (!Projectile->InitializePredictedProjectile(ProjectileInitData))
	{
		Projectile->Destroy();
		return false;
	}

	Projectile->FinishSpawning(SpawnTransform);
	Weapon.RegisterPredictedProjectile(ProjectileId, Projectile);
	return true;
}

bool UStulWeaponFireComponent::SpawnAuthoritativeProjectile(const UStulWeaponDefinition& Definition, AStulWeapon& Weapon, const FVector& ViewOrigin, const FStulProjectileId& ProjectileId, FStulWeaponShotResult& ShotResult) const
{
	if (!Weapon.HasAuthority()) return false;

	UWorld* World = GetWorld();
	const TSubclassOf<AStulWeaponProjectile> ProjectileClass = Definition.ProjectileClass.Get();
	if (!World || !ProjectileClass) return false;

	const FTransform SpawnTransform(ShotResult.Direction.Rotation(), ViewOrigin);
	AStulWeaponProjectile* Projectile = World->SpawnActorDeferred<AStulWeaponProjectile>(ProjectileClass, SpawnTransform, &Weapon, Cast<APawn>(Weapon.GetOwner()), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Projectile) return false;

	const FStulWeaponProjectileInitData ProjectileInitData = MakeProjectileInitData(Definition, ProjectileId, ShotResult);
	const UStulWeaponAttributeSet* Attributes = Weapon.GetWeaponAttributeSet();
	if (!Projectile->InitializeProjectile(ProjectileInitData, Attributes ? Attributes->GetDamage() : 0.0f, Weapon.GetCurrentAmmoDefinition()))
	{
		Projectile->Destroy();
		return false;
	}

	Projectile->FinishSpawning(SpawnTransform);
	return true;
}

void UStulWeaponFireComponent::DrawShotDebug(const FVector& ViewOrigin, const FVector& AimTarget, const FHitResult& AimHit, const FStulWeaponShotResult& ShotResult) const
{
#if ENABLE_DRAW_DEBUG
	if (!bDrawDebugTraces) return;
	UWorld* World = GetWorld();
	if (!World) return;

	const float Duration = FMath::Max(0.0f, DebugDrawDuration);
	const float Radius = FMath::Max(0.0f, DebugImpactRadius);
	const float Thickness = FMath::Max(0.0f, DebugLineThickness);
	if (ShotResult.ProjectileIndex == 0)
	{
		DrawDebugLine(World, ViewOrigin, AimTarget, FColor::Cyan, false, Duration, 0, Thickness);
		DrawDebugSphere(World, ViewOrigin, Radius, 8, FColor::Blue, false, Duration, 0, Thickness);
		if (AimHit.bBlockingHit) DrawDebugSphere(World, AimHit.ImpactPoint, Radius, 12, FColor::Yellow, false, Duration, 0, Thickness);
	}

	const FColor GameplayTraceColor = ShotResult.bAuthoritative ? FColor::Orange : FColor::Green;
	DrawDebugLine(World, ShotResult.TraceOrigin, ShotResult.EndLocation, GameplayTraceColor, false, Duration, 0, Thickness);
	DrawDebugLine(World, ShotResult.MuzzleLocation, ShotResult.EndLocation, FColor::Purple, false, Duration, 0, Thickness);
	DrawDebugSphere(World, ShotResult.MuzzleLocation, Radius, 8, FColor::Magenta, false, Duration, 0, Thickness);
	if (ShotResult.bBlockingHit) DrawDebugSphere(World, ShotResult.HitResult.ImpactPoint, Radius, 12, FColor::Red, false, Duration, 0, Thickness);
#endif
}

int32 UStulWeaponFireComponent::MakeShotSeed(const FStulProjectileId& ProjectileId) const
{
	const AStulWeapon* Weapon = GetWeapon();
	const uint32 WeaponSeed = Weapon ? Weapon->GetBallisticSeed() : 0;
	return static_cast<int32>(HashCombineFast(WeaponSeed, GetTypeHash(ProjectileId)));
}

void UStulWeaponFireComponent::DispatchFiringStarted()
{
	AStulWeapon* Weapon = GetWeapon();
	if (!Weapon) return;
	Weapon->HandleFiringStarted();

	FGameplayEventData EventData;
	EventData.EventTag = StulWeaponGameplayTags::Event_Fire_Start;
	EventData.Instigator = Weapon->GetOwner();
	EventData.OptionalObject = Weapon;
	if (UAbilitySystemComponent* AbilitySystem = Weapon->GetAbilitySystemComponent()) AbilitySystem->HandleGameplayEvent(EventData.EventTag, &EventData);
}

void UStulWeaponFireComponent::DispatchShotExecuted(const FStulWeaponShotExecution& ShotExecution)
{
	AStulWeapon* Weapon = GetWeapon();
	if (!Weapon) return;
	if (ShotExecution.bAuthoritative) MulticastPresentAuthoritativeShot(ShotExecution);
	else Weapon->HandleShotExecuted(ShotExecution);

	FGameplayEventData EventData;
	EventData.EventTag = StulWeaponGameplayTags::Event_Fire_Shot;
	EventData.Instigator = Weapon->GetOwner();
	EventData.OptionalObject = Weapon;
	EventData.EventMagnitude = Weapon->GetWeaponAttributeSet() ? Weapon->GetWeaponAttributeSet()->GetDamage() : 0.0f;
	if (UAbilitySystemComponent* AbilitySystem = Weapon->GetAbilitySystemComponent()) AbilitySystem->HandleGameplayEvent(EventData.EventTag, &EventData);
}

void UStulWeaponFireComponent::MulticastPresentAuthoritativeShot_Implementation(const FStulWeaponShotExecution& ShotExecution)
{
	AStulWeapon* Weapon = GetWeapon();
	if (!Weapon || Weapon->GetNetMode() == NM_DedicatedServer) return;
	if (!Weapon->HasAuthority() && Weapon->HasLocalNetOwner()) return;
	Weapon->HandleShotExecuted(ShotExecution);
}

void UStulWeaponFireComponent::MulticastPresentHitscanImpact_Implementation(const FHitResult& HitResult, const float Damage, AActor* EffectCauser, const bool bSkipOwningClient)
{
	AStulWeapon* Weapon = GetWeapon();
	if (!Weapon || Weapon->GetNetMode() == NM_DedicatedServer) return;
	if (bSkipOwningClient && !Weapon->HasAuthority() && Weapon->HasLocalNetOwner()) return;
	Weapon->ExecuteImpactGameplayCue(HitResult, Damage, EffectCauser, Weapon->GetCurrentAmmoDefinition());
}

void UStulWeaponFireComponent::DispatchFiringEnded()
{
	AStulWeapon* Weapon = GetWeapon();
	if (!Weapon) return;
	Weapon->HandleFiringEnded();

	FGameplayEventData EventData;
	EventData.EventTag = StulWeaponGameplayTags::Event_Fire_End;
	EventData.Instigator = Weapon->GetOwner();
	EventData.OptionalObject = Weapon;
	if (UAbilitySystemComponent* AbilitySystem = Weapon->GetAbilitySystemComponent()) AbilitySystem->HandleGameplayEvent(EventData.EventTag, &EventData);
}

/*********************************************************************************************/
/************************************** Validation *******************************************/
/*********************************************************************************************/
bool UStulWeaponFireComponent::CaptureView(FStulCapturedView& OutCapturedView) const
{
	OutCapturedView = FStulCapturedView();
	AActor* OwnerActor = GetWeapon() ? GetWeapon()->GetOwner() : nullptr;
	if (!IsValid(OwnerActor)) return false;

	FStulWeaponViewData ViewData;
	if (OwnerActor->Implements<UStulWeaponOwnerInterface>())
	{
		if (!IStulWeaponOwnerInterface::Execute_GetWeaponViewData(OwnerActor, ViewData)) return false;
	}
	else
	{
		OwnerActor->GetActorEyesViewPoint(ViewData.ViewLocation, ViewData.ViewRotation);
	}

	OutCapturedView.Origin = ViewData.ViewLocation;
	OutCapturedView.Direction = ViewData.ViewRotation.Vector().GetSafeNormal();
	return OutCapturedView.IsValid();
}

bool UStulWeaponFireComponent::ValidateAndResolveView(const FStulCapturedView& CapturedView, FVector& OutViewOrigin, FVector& OutViewDirection) const
{
	FStulCapturedView ServerView;
	if (!CapturedView.IsValid() || !CaptureView(ServerView)) return false;
	return UStulWeaponShootingLibrary::ValidateAndResolveClientView(CapturedView.Origin, CapturedView.Direction, ServerView.Origin, MaxClientViewOriginError, OutViewOrigin, OutViewDirection);
}

bool UStulWeaponFireComponent::IsWeaponEquipped() const
{
	const AStulWeapon* Weapon = GetWeapon();
	const AActor* OwnerActor = Weapon ? Weapon->GetOwner() : nullptr;
	const UStulWeaponManagerComponent* WeaponManager = OwnerActor ? OwnerActor->FindComponentByClass<UStulWeaponManagerComponent>() : nullptr;
	return WeaponManager && WeaponManager->GetEquippedWeapon() == Weapon;
}

bool UStulWeaponFireComponent::IsAbilityAuthorizationValid(const EStulFireSessionType SessionType) const
{
	return AbilityAuthorization.IsValid() && AbilityAuthorization.SessionType == SessionType;
}

/*********************************************************************************************/
/**************************************** State **********************************************/
/*********************************************************************************************/
bool UStulWeaponFireComponent::IsProducingShots() const
{
	return LocalSession.State == EStulFireSessionState::Active;
}

AStulWeapon* UStulWeaponFireComponent::GetWeapon() const
{
	return Cast<AStulWeapon>(GetOwner());
}

APawn* UStulWeaponFireComponent::GetOwningPawn() const
{
	const AStulWeapon* Weapon = GetWeapon();
	return Weapon ? Cast<APawn>(Weapon->GetOwner()) : nullptr;
}
