// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class StulWeaponSystem : ModuleRules
{
	public StulWeaponSystem(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayAbilities",
				"GameplayTags",
				"GameplayTasks",
				"Niagara",
				"NetCore",
				"PhysicsCore"
			}
		);
	}
}
