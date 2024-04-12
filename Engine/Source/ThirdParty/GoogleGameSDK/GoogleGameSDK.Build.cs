// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
using EpicGames.Core;

public class GoogleGameSDK : ModuleRules
{
	public GoogleGameSDK(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		string GoogleGameSDKPath = Target.UEThirdPartySourceDirectory + "GoogleGameSDK";

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			string Arm64GameSDKPath = GoogleGameSDKPath + "/gamesdk/libs/arm64-v8a_API27_NDK23_cpp_static_Release/";
			string x86_64GameSDKPath = GoogleGameSDKPath + "/gamesdk/libs/x86_64_API27_NDK23_cpp_static_Release/";

			PublicAdditionalLibraries.Add(Arm64GameSDKPath + "libswappy_static.a");
			PublicAdditionalLibraries.Add(x86_64GameSDKPath + "libswappy_static.a");

			PublicSystemIncludePaths.Add(GoogleGameSDKPath + "/gamesdk/include");
        }
    }
}
