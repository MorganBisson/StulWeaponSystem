#include "Weapons/Shooting/StulWeaponShootingTypes.h"

bool FGameplayAbilityTargetData_StulWeaponShot::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	bool bOriginSuccess = false;
	bool bDirectionSuccess = false;
	ViewOrigin.NetSerialize(Ar, Map, bOriginSuccess);
	ViewDirection.NetSerialize(Ar, Map, bDirectionSuccess);
	Ar << ShotSequence;
	bOutSuccess = bOriginSuccess && bDirectionSuccess && !Ar.IsError();
	return true;
}
