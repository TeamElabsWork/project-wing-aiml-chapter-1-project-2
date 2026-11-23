// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AeroSentineal : ModuleRules
{
	public AeroSentineal(ReadOnlyTargetRules Target) : base(Target)
	{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

			// This module organises code in logical sub-folders (Core/, Drone/, Sensors/, ...) instead of
			// a Public/Private split, so the module root itself has to be on the include path for
			// path-qualified includes such as "Core/AeroTypes.h".
			PublicIncludePaths.Add(ModuleDirectory);
		
			// GeoReferencing: the WGS84 <-> Unreal transform contract (AGeoReferencingSystem).
			// EnhancedInput / GameplayTags: input + tag vocabulary used by the drone and disaster layers.
			PublicDependencyModuleNames.AddRange(new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"GeoReferencing",
				"EnhancedInput",
				"GameplayTags"
			});

			// DeveloperSettings: UAeroSimSettings project-settings surface.
			// Json/JsonUtilities: dataset records. ImageWrapper: PNG encode for captured frames.
			// UMG/Slate/SlateCore: developer HUD. RenderCore: scene-capture render targets.
			PrivateDependencyModuleNames.AddRange(new string[] {
				"DeveloperSettings",
				"Json",
				"JsonUtilities",
				"ImageWrapper",
				"RenderCore",
				"UMG",
				"Slate",
				"SlateCore"
			});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
