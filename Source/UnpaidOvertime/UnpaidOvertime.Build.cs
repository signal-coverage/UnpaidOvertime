// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnpaidOvertime : ModuleRules
{
	public UnpaidOvertime(ReadOnlyTargetRules Target) : base(Target)
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
			"SlateCore"
		});

		// RenderCore: white texture used by the canvas HUD to draw filled shapes (stars)
		PrivateDependencyModuleNames.AddRange(new string[] { "RenderCore" });

		PublicIncludePaths.AddRange(new string[] {
			"UnpaidOvertime",
			"UnpaidOvertime/Core",
			"UnpaidOvertime/Gameplay",
			"UnpaidOvertime/AI",
			"UnpaidOvertime/UI",
			"UnpaidOvertime/Tests",
			"UnpaidOvertime/Variant_Horror",
			"UnpaidOvertime/Variant_Horror/UI",
			"UnpaidOvertime/Variant_Shooter",
			"UnpaidOvertime/Variant_Shooter/AI",
			"UnpaidOvertime/Variant_Shooter/UI",
			"UnpaidOvertime/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
