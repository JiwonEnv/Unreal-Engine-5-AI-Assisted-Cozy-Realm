// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CozyRealm : ModuleRules
{
	public CozyRealm(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"UMG",
			"Slate",
			"DeveloperSettings"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "SlateCore", "Landscape", "AssetRegistry", "ApplicationCore", "RenderCore", "RHI", "ProceduralMeshComponent" });

		PublicIncludePaths.AddRange(new string[] {
			"CozyRealm"
		});
	}
}
