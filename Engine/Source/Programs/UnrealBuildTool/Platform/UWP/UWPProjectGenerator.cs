// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;
using System.Xml.Linq;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Base class for platform-specific project generators
	/// </summary>
	class UWPProjectGenerator : PlatformProjectGenerator
	{
		const string PlatformString = "UWP";

		public UWPProjectGenerator(CommandLineArguments Arguments)
			: base(Arguments)
		{

		}

		/// <summary>
		/// Enumerate all the platforms that this generator supports
		/// </summary>
		public override IEnumerable<UnrealTargetPlatform> GetPlatforms()
		{
			yield return UnrealTargetPlatform.UWP32;
			yield return UnrealTargetPlatform.UWP64;
		}

		///
		///	VisualStudio project generation functions
		///	
		/// <summary>
		/// Whether this build platform has native support for VisualStudio
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration"> The UnrealTargetConfiguration being built</param>
		/// <param name="ProjectFileFormat"></param>
		/// <returns>bool    true if native VisualStudio support (or custom VSI) is available</returns>
		public override bool HasVisualStudioSupport(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration, VCProjectFileFormat ProjectFileFormat)
		{
			return false;
		}

		/// <summary>
		/// Get whether this platform deploys
		/// </summary>
		/// <returns>bool  true if the 'Deploy' option should be enabled</returns>
		public override bool GetVisualStudioDeploymentEnabled(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			return true;
		}

		public override void GenerateGameProperties(UnrealTargetConfiguration Configuration, StringBuilder VCProjectFileContent, TargetType TargetType, DirectoryReference RootDirectory, FileReference TargetFilePath)
		{
			string MinVersion = string.Empty;
			string MaxTestedVersion = string.Empty;
			ConfigHierarchy EngineIni = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, RootDirectory, UnrealTargetPlatform.UWP64);
			if (EngineIni != null)
			{
				EngineIni.GetString("/Script/UWPPlatformEditor.UWPTargetSettings", "MinimumPlatformVersion", out MinVersion);
				EngineIni.GetString("/Script/UWPPlatformEditor.UWPTargetSettings", "MaximumPlatformVersionTested", out MaxTestedVersion);
			}
			if (!string.IsNullOrEmpty(MinVersion))
			{
				VCProjectFileContent.Append("		<WindowsTargetPlatformMinVersion>" + MinVersion + "</WindowsTargetPlatformMinVersion>" + ProjectFileGenerator.NewLine);
			}
			if (!string.IsNullOrEmpty(MaxTestedVersion))
			{
				VCProjectFileContent.Append("		<WindowsTargetPlatformVersion>" + MaxTestedVersion + "</WindowsTargetPlatformVersion>" + ProjectFileGenerator.NewLine);
			}

			WindowsCompiler Compiler = WindowsCompiler.VisualStudio2017;
			DirectoryReference PlatformWinMDLocation = HoloLens.GetCppCXMetadataLocation(Compiler, "Latest");
			if (PlatformWinMDLocation == null || !FileReference.Exists(FileReference.Combine(PlatformWinMDLocation, "platform.winmd")))
			{
				PlatformWinMDLocation = HoloLens.GetCppCXMetadataLocation(Compiler, "Latest");
			}
			string FoundationWinMDPath = HoloLens.GetLatestMetadataPathForApiContract("Windows.Foundation.FoundationContract", Compiler);
			string UniversalWinMDPath = HoloLens.GetLatestMetadataPathForApiContract("Windows.Foundation.UniversalApiContract", Compiler);
			VCProjectFileContent.Append("		<AdditionalOptions>/ZW /ZW:nostdlib</AdditionalOptions>" + ProjectFileGenerator.NewLine);
			VCProjectFileContent.Append("		<NMakePreprocessorDefinitions>$(NMakePreprocessorDefinitions);PLATFORM_UWP=1;UWP=1;</NMakePreprocessorDefinitions>" + ProjectFileGenerator.NewLine);
			if (PlatformWinMDLocation != null)
			{
				VCProjectFileContent.Append("		<NMakeAssemblySearchPath>$(NMakeAssemblySearchPath);" + PlatformWinMDLocation + "</NMakeAssemblySearchPath>" + ProjectFileGenerator.NewLine);
			}
			VCProjectFileContent.Append("		<NMakeForcedUsingAssemblies>$(NMakeForcedUsingAssemblies);" + FoundationWinMDPath + ";" + UniversalWinMDPath + ";platform.winmd</NMakeForcedUsingAssemblies>" + ProjectFileGenerator.NewLine);
		}

		public override void GetVisualStudioPreDefaultString(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration, StringBuilder ProjectFileBuilder)
		{
			// VS2017 expects WindowsTargetPlatformVersion to be set in conjunction with these other properties, otherwise the projects
			// will fail to load when the solution is in a UWP configuration.
			// Default to latest supported version.  Game projects can override this later.
			// Because this property is only required for VS2017 we can safely say that's the compiler version (whether that's actually true
			// or not)
			string SDKFolder = "";
			string SDKVersion = "";

			DirectoryReference folder;
			VersionNumber version;
			if (WindowsPlatform.TryGetWindowsSdkDir("Latest", out version, out folder))
			{
				SDKFolder = folder.FullName;
				SDKVersion = version.ToString();
			}

			ProjectFileBuilder.AppendLine("    <AppContainerApplication>true</AppContainerApplication>");
			ProjectFileBuilder.AppendLine("    <ApplicationType>Windows Store</ApplicationType>");
			ProjectFileBuilder.AppendLine("    <ApplicationTypeRevision>10.0</ApplicationTypeRevision>");
			ProjectFileBuilder.AppendLine("    <WindowsAppContainer>true</WindowsAppContainer>");
			ProjectFileBuilder.AppendLine("    <AppxPackage>true</AppxPackage>");
			ProjectFileBuilder.AppendLine("    <WindowsTargetPlatformVersion>" + SDKVersion.ToString() + "</WindowsTargetPlatformVersion>");
		}

		public override string GetVisualStudioLayoutDirSection(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration, string InConditionString, TargetType TargetType, FileReference TargetRulesPath, FileReference ProjectFilePath, FileReference NMakeOutputPath, VCProjectFileFormat InProjectFileFormat)
		{
			string LayoutDirString = "";

			if (IsValidUWPTarget(InPlatform, TargetType, TargetRulesPath))
			{
				LayoutDirString += "	<PropertyGroup " + InConditionString + ">" + ProjectFileGenerator.NewLine;
				LayoutDirString += "		<RemoveExtraDeployFiles>false</RemoveExtraDeployFiles>" + ProjectFileGenerator.NewLine;
				LayoutDirString += "		<LayoutDir>" + DirectoryReference.Combine(NMakeOutputPath.Directory, "AppX").FullName + "</LayoutDir>" + ProjectFileGenerator.NewLine;
				LayoutDirString += "		<AppxPackageRecipe>" + FileReference.Combine(NMakeOutputPath.Directory, ProjectFilePath.GetFileNameWithoutExtension() + ".build.appxrecipe").FullName + "</AppxPackageRecipe>" + ProjectFileGenerator.NewLine;
				LayoutDirString += "	</PropertyGroup>" + ProjectFileGenerator.NewLine;

				// another hijack - this is a convenient point to make sure that UWP-appropriate debuggers are available
				// in the project property pages.
				LayoutDirString += "    <ItemGroup " + InConditionString + ">" + ProjectFileGenerator.NewLine;
				LayoutDirString += "		<PropertyPageSchema Include=\"$(VCTargetsPath)$(LangID)\\AppHostDebugger_Local.xml\" />" + ProjectFileGenerator.NewLine;
				LayoutDirString += "		<PropertyPageSchema Include=\"$(VCTargetsPath)$(LangID)\\AppHostDebugger_Simulator.xml\" />" + ProjectFileGenerator.NewLine;
				LayoutDirString += "		<PropertyPageSchema Include=\"$(VCTargetsPath)$(LangID)\\AppHostDebugger_Remote.xml\" />" + ProjectFileGenerator.NewLine;
				LayoutDirString += "    </ItemGroup>" + ProjectFileGenerator.NewLine;
			}

			return LayoutDirString;
		}

		private bool IsValidUWPTarget(UnrealTargetPlatform InPlatform, TargetType InTargetType, FileReference InTargetFilePath)
		{
			if ((InPlatform == UnrealTargetPlatform.UWP64 || InPlatform == UnrealTargetPlatform.UWP32) &&
				(InTargetType == TargetRules.TargetType.Client || InTargetType == TargetRules.TargetType.Game ) &&
				InTargetType != TargetRules.TargetType.Editor &&
				InTargetType != TargetRules.TargetType.Server
				)
			{
				// We do not want to include any Templates targets
				// Not a huge fan of doing it via path name comparisons... but it works
				string TempTargetFilePath = InTargetFilePath.FullName.Replace("\\", "/");
				if (TempTargetFilePath.Contains("/Templates/"))
				{
					// TODO: TEST?
					string AbsoluteEnginePath = UnrealBuildTool.EngineDirectory.FullName;
					AbsoluteEnginePath = AbsoluteEnginePath.Replace("\\", "/");
					if (AbsoluteEnginePath.EndsWith("/") == false)
					{
						AbsoluteEnginePath += "/";
					}
					string CheckPath = AbsoluteEnginePath.Replace("/Engine/", "/Templates/");
					if (TempTargetFilePath.StartsWith(CheckPath))
					{
						return false;
					}
				}
				return true;
			}

			return false;
		}

		public override bool RequiresVSUserFileGeneration()
		{
			return true;
		}

		public override string GetVisualStudioUserFileStrings(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration,
			string InConditionString, TargetRules InTargetRules, FileReference TargetRulesPath, FileReference ProjectFilePath)
		{
			string UserFileEntry = "";
			if (IsValidUWPTarget(InPlatform, InTargetRules.Type, TargetRulesPath))
			{
				UserFileEntry += "<PropertyGroup " + InConditionString + ">\n";
				UserFileEntry += "	<DebuggerFlavor>AppHostLocalDebugger</DebuggerFlavor>\n";
				UserFileEntry += "</PropertyGroup>\n";
			}
			return UserFileEntry;
		}
	}
}

