#include "Weapons/Shooting/StulWeaponFireSessionTypes.h"

FString FStulShotId::ToString() const
{
	return FString::Printf(TEXT("%u:%u"), SessionId, ShotSequence);
}

FString FStulProjectileId::ToString() const
{
	return FString::Printf(TEXT("%s:%u"), *ShotId.ToString(), ProjectileIndex);
}

bool FStulShotBatch::IsConsistent() const
{
	if (SessionId == 0 || FirstSequence == 0 || Commands.IsEmpty() || Commands.Num() > 16) return false;
	if (static_cast<uint64>(FirstSequence) + static_cast<uint64>(Commands.Num()) - 1ull > MAX_uint32) return false;

	for (int32 Index = 0; Index < Commands.Num(); ++Index)
	{
		if (!Commands[Index].IsValid() || Commands[Index].ShotSequence != FirstSequence + static_cast<uint32>(Index)) return false;
	}
	return true;
}

bool FStulCloseFireSessionRequest::IsValid() const
{
	if (SessionId == 0 || TailCommands.Num() > 16) return false;
	if (TailCommands.IsEmpty()) return FinalSequence == 0;
	if (TailCommands.Last().ShotSequence != FinalSequence) return false;

	for (int32 Index = 0; Index < TailCommands.Num(); ++Index)
	{
		if (!TailCommands[Index].IsValid()) return false;
		if (Index > 0 && TailCommands[Index].ShotSequence != TailCommands[Index - 1].ShotSequence + 1) return false;
	}
	return true;
}
