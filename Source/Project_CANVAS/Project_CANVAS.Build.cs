// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Project_CANVAS : ModuleRules
{
	public Project_CANVAS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"SlateCore",
			"HTTP",
			"Json",
			"JsonUtilities",
			"GameplayTasks",
			"GameplayTasks"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"Project_CANVAS",
			"Project_CANVAS/Variant_Platforming",
			"Project_CANVAS/Variant_Platforming/Animation",
			"Project_CANVAS/Variant_Combat",
			"Project_CANVAS/Variant_Combat/AI",
			"Project_CANVAS/Variant_Combat/Animation",
			"Project_CANVAS/Variant_Combat/Gameplay",
			"Project_CANVAS/Variant_Combat/Interfaces",
			"Project_CANVAS/Variant_Combat/UI",
			"Project_CANVAS/Variant_SideScrolling",
			"Project_CANVAS/Variant_SideScrolling/AI",
			"Project_CANVAS/Variant_SideScrolling/Gameplay",
			"Project_CANVAS/Variant_SideScrolling/Interfaces",
			"Project_CANVAS/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
