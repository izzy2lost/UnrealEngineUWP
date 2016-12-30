// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

// @ATG_CHANGE : BEGIN UWP support
using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;
using System.Xml.Linq;

// @todo UWP: this file is a work in progress and is not yet complete for the F5 scenario for UWP

namespace UnrealBuildTool
{
	/// <summary>
	/// Base class for platform-specific project generators
	/// </summary>
	public class UWPProjectGenerator : UEPlatformProjectGenerator
	{
		const string PlatformString = "UWP";

		/// <summary>
		/// Register the platform with the UEPlatformProjectGenerator class
		/// </summary>
		public override void RegisterPlatformProjectGenerator()
		{
			// Register this project generator for UWP
			Log.TraceVerbose("		Registering for {0}", UnrealTargetPlatform.UWP64.ToString());
			UEPlatformProjectGenerator.RegisterPlatformProjectGenerator(UnrealTargetPlatform.UWP64, this);
			Log.TraceVerbose("		Registering for {0}", UnrealTargetPlatform.UWP32.ToString());
			UEPlatformProjectGenerator.RegisterPlatformProjectGenerator(UnrealTargetPlatform.UWP32, this);
		}

		///
		///	VisualStudio project generation functions
		///	
		/// <summary>
		/// Whether this build platform has native support for VisualStudio
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration"> The UnrealTargetConfiguration being built</param>
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

		public override void GenerateGameProperties(UnrealTargetConfiguration Configuration, StringBuilder VCProjectFileContent, TargetRules.TargetType TargetType, DirectoryReference RootDirectory, FileReference TargetFilePath)
		{
			string FoundationWinMDPath = VCEnvironment.GetLatestMetadataPathForApiContract("Windows.Foundation.FoundationContract");
			string UniversalWinMDPath = VCEnvironment.GetLatestMetadataPathForApiContract("Windows.Foundation.UniversalApiContract");
			string PlatformWinMDPath = string.Empty;
			DirectoryReference VCInstallPath;
			if (WindowsPlatform.TryGetVCInstallDir(WindowsPlatform.Compiler, out VCInstallPath))
			{
				PlatformWinMDPath = DirectoryReference.Combine(VCInstallPath, "vcpackages", "platform.winmd").FullName;
			}
			VCProjectFileContent.Append("		<AdditionalOptions>/ZW</AdditionalOptions>" + ProjectFileGenerator.NewLine);
			VCProjectFileContent.Append("		<NMakePreprocessorDefinitions>$(NMakePreprocessorDefinitions);PLATFORM_UWP=1;UWP=1;</NMakePreprocessorDefinitions>" + ProjectFileGenerator.NewLine);
			VCProjectFileContent.Append("       <NMakeForcedUsingAssemblies>$(NMakeForcedUsingAssemblies);" + FoundationWinMDPath + ";" + UniversalWinMDPath + ";" + PlatformWinMDPath + "</NMakeForcedUsingAssemblies>" + ProjectFileGenerator.NewLine);
		}

		public override string GetVisualStudioPreDefaultString(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			return "		<AppContainerApplication>true</AppContainerApplication>" + ProjectFileGenerator.NewLine +
					"		<ApplicationType>Windows Store</ApplicationType>" + ProjectFileGenerator.NewLine +
					"		<ApplicationTypeRevision>10.0</ApplicationTypeRevision>" + ProjectFileGenerator.NewLine +
					"		<WindowsAppContainer>true</WindowsAppContainer>" + ProjectFileGenerator.NewLine +
					"		<AppxPackage>true</AppxPackage>" + ProjectFileGenerator.NewLine;
		}

		public override string GetVisualStudioLayoutDirSection(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration, string InConditionString, TargetRules.TargetType TargetType, FileReference TargetRulesPath, FileReference ProjectFilePath, FileReference NMakeOutputPath, VCProjectFileFormat InProjectFileFormat)
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

		private bool IsValidUWPTarget(UnrealTargetPlatform InPlatform, TargetRules.TargetType InTargetType, FileReference InTargetFilePath)
		{
			if ((InPlatform == UnrealTargetPlatform.UWP64 || InPlatform == UnrealTargetPlatform.UWP32) &&
				(TargetRules.IsGameType(InTargetType)) &&
				(TargetRules.IsEditorType(InTargetType) == false) &&
				(InTargetType != TargetRules.TargetType.Server) &&
				(InTargetType != TargetRules.TargetType.Client)
				)
			{
				// We do not want to include any Templates targets
				// Not a huge fan of doing it via path name comparisons... but it works
				string TempTargetFilePath = InTargetFilePath.FullName.Replace("\\", "/");
				if (TempTargetFilePath.Contains("/Templates/"))
				{
					string AbsoluteEnginePath = Path.GetFullPath(ProjectFileGenerator.EngineRelativePath);
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
// @ATG_CHANGE : END