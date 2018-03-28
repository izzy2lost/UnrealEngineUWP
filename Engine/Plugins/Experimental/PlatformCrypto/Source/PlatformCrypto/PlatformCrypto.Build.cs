// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class PlatformCrypto : ModuleRules
	{
		public PlatformCrypto(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
					"Experimental/PlatformCrypto/Private",
				}
				);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				}
				);

			// @ATG_CHANGE : BEGIN - allow more platforms to use AES-GCM
			if (Target.Platform == UnrealTargetPlatform.XboxOne ||
				Target.Platform == UnrealTargetPlatform.Win64 ||
				Target.Platform == UnrealTargetPlatform.Win32 ||
				Target.Platform == UnrealTargetPlatform.UWP64 ||
				Target.Platform == UnrealTargetPlatform.UWP32)
			// @ATG_CHANGE : END
			{
				PublicDependencyModuleNames.AddRange(
					new string[]
					{
						"PlatformCryptoBCrypt",
					}
					);

				PublicIncludePathModuleNames.AddRange(
					new string[]
					{
						"PlatformCryptoBCrypt"
					}
					);
			}
			else
			{
				PublicDependencyModuleNames.AddRange(
					new string[]
					{
						"PlatformCryptoOpenSSL",
					}
					);

				PublicIncludePathModuleNames.AddRange(
					new string[]
					{
						"PlatformCryptoOpenSSL"
					}
					);
			}
		}
	}
}
