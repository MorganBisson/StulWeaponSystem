#pragma once

#include "CoreMinimal.h"
#include "StulWeaponFireSessionTypes.generated.h"

/** Network responsibility captured by a fire session when it starts. */
UENUM()
enum class EStulFireExecutionRole : uint8
{
	PredictiveProducer,
	AuthoritativeProducer,
	AuthoritativeConsumer,
	PresentationOnly
};

/** Gameplay lifecycle requested by the concrete fire ability. */
UENUM(BlueprintType)
enum class EStulFireSessionType : uint8
{
	Single,
	Burst,
	Automatic
};

/** Temporal state of one fire session, independently from weapon availability. */
UENUM()
enum class EStulFireSessionState : uint8
{
	None,
	Opening,
	Active,
	Closing
};

/** Terminal server decision for a fire session. */
UENUM()
enum class EStulFireSessionFinalState : uint8
{
	None,
	Completed,
	Interrupted,
	Rejected
};

/** Stable identity of one logical shot inside a weapon fire session. */
USTRUCT(BlueprintType)
struct STULWEAPONSYSTEM_API FStulShotId
{
	GENERATED_BODY()

	FStulShotId() = default;
	FStulShotId(const uint32 InSessionId, const uint32 InShotSequence) : SessionId(InSessionId), ShotSequence(InShotSequence) {}

	/** Kept C++-only because Blueprint does not expose unsigned 32-bit integers. */
	UPROPERTY()
	uint32 SessionId = 0;

	UPROPERTY()
	uint32 ShotSequence = 0;

	bool IsValid() const { return SessionId != 0; }
	FString ToString() const;

	bool operator==(const FStulShotId& Other) const
	{
		return SessionId == Other.SessionId && ShotSequence == Other.ShotSequence;
	}

	friend uint32 GetTypeHash(const FStulShotId& ShotId)
	{
		return HashCombineFast(GetTypeHash(ShotId.SessionId), GetTypeHash(ShotId.ShotSequence));
	}
};

/** Stable identity of one projectile or pellet produced by a logical shot. */
USTRUCT(BlueprintType)
struct STULWEAPONSYSTEM_API FStulProjectileId
{
	GENERATED_BODY()

	FStulProjectileId() = default;
	FStulProjectileId(const FStulShotId& InShotId, const uint32 InProjectileIndex) : ShotId(InShotId), ProjectileIndex(InProjectileIndex) {}

	UPROPERTY(BlueprintReadOnly, Category = "Fire Session")
	FStulShotId ShotId;

	/** Kept C++-only because Blueprint does not expose unsigned 32-bit integers. */
	UPROPERTY()
	uint32 ProjectileIndex = 0;

	bool IsValid() const { return ShotId.IsValid(); }
	FString ToString() const;

	bool operator==(const FStulProjectileId& Other) const
	{
		return ShotId == Other.ShotId && ProjectileIndex == Other.ProjectileIndex;
	}

	friend uint32 GetTypeHash(const FStulProjectileId& ProjectileId)
	{
		return HashCombineFast(GetTypeHash(ProjectileId.ShotId), GetTypeHash(ProjectileId.ProjectileIndex));
	}
};

/** Client-captured intent. It is never considered an authoritative or validated view. */
USTRUCT(BlueprintType)
struct STULWEAPONSYSTEM_API FStulCapturedView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Fire Session")
	FVector_NetQuantize10 Origin = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Fire Session")
	FVector_NetQuantizeNormal Direction = FVector::ForwardVector;

	bool IsValid() const { return !FVector(Direction).IsNearlyZero(); }
};

/** One idempotent shot intention transported inside a fire batch. */
USTRUCT()
struct STULWEAPONSYSTEM_API FStulShotCommand
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 ShotSequence = 0;

	UPROPERTY()
	FStulCapturedView CapturedView;

	bool IsValid() const { return ShotSequence != 0 && CapturedView.IsValid(); }
};

/** Unreliable redundant transport unit for a contiguous window of shot commands. */
USTRUCT()
struct STULWEAPONSYSTEM_API FStulShotBatch
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 SessionId = 0;

	UPROPERTY()
	EStulFireSessionType SessionType = EStulFireSessionType::Single;

	UPROPERTY()
	uint32 FirstSequence = 0;

	UPROPERTY()
	TArray<FStulShotCommand> Commands;

	bool IsConsistent() const;
};

/** Reliable formal request to open a server fire session. */
USTRUCT()
struct STULWEAPONSYSTEM_API FStulOpenFireSessionRequest
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 SessionId = 0;

	UPROPERTY()
	EStulFireSessionType SessionType = EStulFireSessionType::Single;

	bool IsValid() const { return SessionId != 0; }
};

/** Reliable session closure carrying a redundant tail of the final commands. */
USTRUCT()
struct STULWEAPONSYSTEM_API FStulCloseFireSessionRequest
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 SessionId = 0;

	UPROPERTY()
	uint32 FinalSequence = 0;

	UPROPERTY()
	TArray<FStulShotCommand> TailCommands;

	bool IsValid() const;
};

/** Atomic server snapshot used to reconcile shot decisions and owner ammunition. */
USTRUCT()
struct STULWEAPONSYSTEM_API FStulFireAck
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 SessionId = 0;

	UPROPERTY()
	uint32 Revision = 0;

	UPROPERTY()
	uint32 ResolvedThroughSequence = 0;

	/** bit 0 describes ResolvedThroughSequence; older contiguous decisions occupy subsequent bits. */
	UPROPERTY()
	uint64 AcceptedThroughMask = 0;

	UPROPERTY()
	uint64 ResolvedAheadMask = 0;

	UPROPERTY()
	uint64 AcceptedAheadMask = 0;

	UPROPERTY()
	int32 AuthoritativeAmmoSnapshot = 0;

	bool IsValid() const { return SessionId != 0 && (AcceptedAheadMask & ~ResolvedAheadMask) == 0; }
};

/** Persistent owner-only truth for the latest session finalized by this weapon instance. */
USTRUCT()
struct STULWEAPONSYSTEM_API FStulOwnerReconciliationState
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 Revision = 0;

	UPROPERTY()
	uint32 LatestFinalizedSessionId = 0;

	UPROPERTY()
	uint32 ResolvedThroughSequence = 0;

	UPROPERTY()
	uint64 AcceptedThroughMask = 0;

	UPROPERTY()
	EStulFireSessionFinalState FinalState = EStulFireSessionFinalState::None;

	UPROPERTY()
	int32 AuthoritativeAmmoSnapshot = 0;
};
