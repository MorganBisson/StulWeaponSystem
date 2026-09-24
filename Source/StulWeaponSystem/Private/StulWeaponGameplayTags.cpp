// Fill out your copyright notice in the Description page of Project Settings.


#include "StulWeaponGameplayTags.h"

namespace StulWeaponGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Root, "Stul.Weapon.Ability", "Root tag for weapon gameplay abilities.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Fire, "Stul.Weapon.Ability.Fire", "Identifies a weapon firing ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Aim, "Stul.Weapon.Ability.Aim", "Identifies a weapon aiming ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Reload, "Stul.Weapon.Ability.Reload", "Identifies a weapon reload ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ChangeFireMode, "Stul.Weapon.Ability.ChangeFireMode", "Identifies a weapon fire mode change ability.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Root, "Stul.Weapon.Input", "Root tag for semantic weapon inputs.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Fire, "Stul.Weapon.Input.Fire", "Starts or stops firing the equipped weapon.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Reload, "Stul.Weapon.Input.Reload", "Requests a reload of the equipped weapon.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Aim, "Stul.Weapon.Input.Aim", "Starts or stops aiming the equipped weapon.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_ChangeFireMode, "Stul.Weapon.Input.ChangeFireMode", "Cycles the equipped weapon fire mode.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(FireMode_Root, "Stul.Weapon.FireMode", "Root tag for weapon fire modes.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(FireMode_Single, "Stul.Weapon.FireMode.Single", "One shot is fired for each input press.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(FireMode_Burst, "Stul.Weapon.FireMode.Burst", "A configured burst is fired for each trigger.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(FireMode_Automatic, "Stul.Weapon.FireMode.Automatic", "Shots continue while the input is held.");

	TConstArrayView<FGameplayTag> GetSupportedFireModeCycle()
	{
		static const TArray<FGameplayTag> SupportedFireModes = {FireMode_Single, FireMode_Burst, FireMode_Automatic};
		return SupportedFireModes;
	}

	bool IsSupportedFireMode(const FGameplayTag FireMode)
	{
		return GetSupportedFireModeCycle().Contains(FireMode);
	}

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Firing, "Stul.Weapon.State.Firing", "The weapon is currently firing.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Reloading, "Stul.Weapon.State.Reloading", "The weapon is currently reloading.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Aiming, "Stul.Weapon.State.Aiming", "The weapon owner is currently aiming.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_ChangingFireMode, "Stul.Weapon.State.ChangingFireMode", "The weapon is waiting to commit a fire mode change.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Fire_Start, "Stul.Weapon.Event.Fire.Start", "Firing has started.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Fire_Shot, "Stul.Weapon.Event.Fire.Shot", "A weapon shot has been emitted.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Fire_End, "Stul.Weapon.Event.Fire.End", "Firing has ended.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Hit, "Stul.Weapon.Event.Hit", "A shot emitted by the weapon produced a hit.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Fire, "GameplayCue.Stul.Weapon.Fire", "One weapon discharge for muzzle flash, sound and animation presentation.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Tracer, "GameplayCue.Stul.Weapon.Tracer", "One hitscan projectile path from its cosmetic muzzle origin to its endpoint.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Impact, "GameplayCue.Stul.Weapon.Impact", "One authoritative or predicted weapon impact.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Aim, "GameplayCue.Stul.Weapon.Aim", "Persistent public presentation state for aiming.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Reload, "GameplayCue.Stul.Weapon.Reload", "Persistent public presentation state for reloading.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_ReloadCommit, "GameplayCue.Stul.Weapon.Reload.Commit", "One public ammunition insertion or reload commit.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_ReloadCompleted, "GameplayCue.Stul.Weapon.Reload.Completed", "A public reload completed successfully.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_ReloadCancelled, "GameplayCue.Stul.Weapon.Reload.Cancelled", "A public reload was interrupted or cancelled.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_ChangeFireMode, "GameplayCue.Stul.Weapon.ChangeFireMode", "Persistent public presentation state for a fire mode change.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Damage, "Stul.Weapon.SetByCaller.Damage", "Damage magnitude passed to a Gameplay Effect.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Ammo, "Stul.Weapon.SetByCaller.Ammo", "Ammo magnitude passed to a Gameplay Effect.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_AmmoRestore, "Stul.Weapon.SetByCaller.AmmoRestore", "Positive ammo magnitude restored by a reload Gameplay Effect.");
}
