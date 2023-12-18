// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class EnhancedInputLiveLink : ModuleRules
{
	public EnhancedInputLiveLink(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject", 
				"Engine", 
				"InputCore",
				"LiveLinkInterface",
				"LiveLinkComponents"
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[] 
			{ 
				"LiveLink",
				"InputCore",
				"EnhancedInput",
				"Slate",
				"SlateCore",
				"KismetCompiler",
				"BlueprintGraph",
				"EditorFramework",
				"UnrealEd",
				"InputEditor",
				"LiveLinkEditor",
				"EditorSubsystem",
			});
	}
}
