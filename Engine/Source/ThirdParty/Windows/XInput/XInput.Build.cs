// Copyright Epic Games, Inc. All Rights Reserved.
using EpicGames.Core;
using System;
using System.IO;
using UnrealBuildTool;

public class XInput : ModuleRules
{
	protected string DirectXSDKDir { get => Target.WindowsPlatform.DirectXDir; }

	public XInput(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		// Ensure correct include and link paths for xinput so the correct dll is loaded (xinput1_3.dll)
		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			PublicSystemLibraries.Add("xinput.lib");
		}
	}
}

