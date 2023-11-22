// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;
using EpicGames.Core;
using System;

public class NNERuntimeIREE : ModuleRules
{
	protected static readonly string SharedLibName = "NNERuntimeIREEModels";

	protected void SetupNNERuntimeIREE(string PlatformName, string PlatformDisplayName, string SharedLibExtension)
	{
		PublicDefinitions.Add("WITH_NNE_RUNTIME_IREE");

		PrivateDefinitions.Add("NNE_RUNTIME_IREE_PLATFORM_NAME=\"" + PlatformName + "\"");
		PrivateDefinitions.Add("NNE_RUNTIME_IREE_PLATFORM_DISPLAY_NAME=\"" + PlatformDisplayName + "\"");
		PrivateDefinitions.Add("NNE_RUNTIME_IREE_PLATFORM_SHARED_LIB_EXTENSION=\"" + SharedLibExtension + "\"");

		// Use the non-remapping RuntimeDependencies.Add function so that the file does not get copied immediately but at staging time
		string SharedLibPath = Path.Combine(PluginDirectory, "Binaries", PlatformName, SharedLibName + "." + SharedLibExtension);
		Directory.CreateDirectory(Path.GetDirectoryName(SharedLibPath));
		File.Create(SharedLibPath).Close();
		RuntimeDependencies.Add(SharedLibPath, StagedFileType.NonUFS);
	}

	public NNERuntimeIREE(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.Add("Core");

		PrivateDependencyModuleNames.AddRange
		(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"IREE",
				"Json",
				"NNE",
				"Slate",
				"SlateCore",
				"Projects"
			}
		);

		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.Add("DerivedDataCache");
		}

		PrivateDefinitions.Add("NNE_RUNTIME_IREE_SHARED_LIB_NAME=\"" + SharedLibName + "\"");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			SetupNNERuntimeIREE("Win64", "Windows", "dll");

			PrivateDefinitions.Add("NNE_RUNTIME_IREE_WIN_TOOLCHAIN_PATH=\"" + Target.WindowsPlatform.ToolChainDir.Replace("\\", "/") + "/\"");
			PrivateDefinitions.Add("NNE_RUNTIME_IREE_WIN_SDK_LIB_PATH=\"" + Target.WindowsPlatform.WindowsSdkDir.Replace("\\", "/") + "/Lib/" + Target.WindowsPlatform.WindowsSdkVersion + "/\"");
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			SetupNNERuntimeIREE("Linux", "Linux", "so");

			PrivateDefinitions.Add("NNE_RUNTIME_IREE_LINUX_COMPILER=\"" + UEBuildPlatformSDK.GetSDKForPlatform("Linux").GetInternalSDKPath().Replace("\\", "/") + "/bin/clang++\"");
			PrivateDefinitions.Add("NNE_RUNTIME_IREE_USE_COMBINED_LIB_PATH");
		}
	}
}
