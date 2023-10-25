// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
    public class GLTFCore : ModuleRules
    {
        public GLTFCore(ReadOnlyTargetRules Target) : base(Target)
        {
            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "CoreUObject",
                }
                );

            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "Engine",
                    "MeshDescription",
					"StaticMeshDescription",
                    "Json",
                    "RenderCore",
					"HTTP",
					"InterchangeCore",
				}
                );

			string DracoLibsDir = Path.Combine(ModuleDirectory, "ThirdParty", "Draco", "lib");
			string DracoIncDir = Path.Combine(ModuleDirectory, "ThirdParty", "Draco", "include");
			
			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				PublicSystemIncludePaths.Add(DracoIncDir);
				PublicSystemLibraryPaths.Add(DracoLibsDir);
				foreach (string DracoLib in Directory.EnumerateFiles(DracoLibsDir, "*.lib", SearchOption.AllDirectories))
				{
					PublicAdditionalLibraries.Add(DracoLib);
				}

				PrivateDefinitions.Add("USE_DRACO_LIBRARY=1");
			}
			else if (Target.Platform == UnrealTargetPlatform.Linux)
			{
				PrivateDefinitions.Add("USE_DRACO_LIBRARY=0"); //update to =1 once the support for the platform is added
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				PrivateDefinitions.Add("USE_DRACO_LIBRARY=0"); //update to =1 once the support for the platform is added
			}
			else 
			{
				PrivateDefinitions.Add("USE_DRACO_LIBRARY=0");
			}
		}
	}
}
