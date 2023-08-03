// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Text;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
	/// <summary>
	/// Base class for platform-specific project generators
	/// </summary>
	class WindowsProjectGenerator : PlatformProjectGenerator
	{
		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Arguments">Command line arguments passed to the project generator</param>
		/// <param name="Logger">Logger for output</param>
		public WindowsProjectGenerator(CommandLineArguments Arguments, ILogger Logger)
			: base(Arguments, Logger)
		{
		}

		/// <inheritdoc/>
		public override IEnumerable<UnrealTargetPlatform> GetPlatforms()
		{
			yield return UnrealTargetPlatform.Win64;
		}

		/// <inheritdoc/>
		public override string GetVisualStudioPlatformName(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration, DirectoryReference InProjectDir, UnrealArch? InArch)
		{
			if (InPlatform == UnrealTargetPlatform.Win64)
			{
				if (InArch == UnrealArch.Arm64)
				{
					return "arm64";
				}
				else if (InArch == UnrealArch.Arm64ec)
				{
					return "arm64ec";
				}
				return "x64";
			}
			return InPlatform.ToString();
		}

		/// <inheritdoc/>
		public override string GetVisualStudioUserFileStrings(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration, string InConditionString, TargetRules InTargetRules, FileReference TargetRulesPath, FileReference ProjectFilePath, string ProjectName, string? ForeignUProjectPath)
		{
			StringBuilder VCUserFileContent = new StringBuilder();

			VCUserFileContent.AppendLine("  <PropertyGroup {0}>", InConditionString);
			if (InTargetRules.Type != TargetType.Game)
			{
				string DebugOptions = "";

				if (ForeignUProjectPath != null)
				{
					DebugOptions += ForeignUProjectPath;
					DebugOptions += " -skipcompile";
				}
				else if (InTargetRules.Type == TargetType.Editor && InTargetRules.ProjectFile != null)
				{
					DebugOptions += ProjectName;
				}

				VCUserFileContent.AppendLine("    <LocalDebuggerCommandArguments>{0}</LocalDebuggerCommandArguments>", DebugOptions);
			}
			VCUserFileContent.AppendLine("    <DebuggerFlavor>WindowsLocalDebugger</DebuggerFlavor>");
			VCUserFileContent.AppendLine("  </PropertyGroup>");

			return VCUserFileContent.ToString();
		}

		/// <inheritdoc/>
		public override bool RequiresVSUserFileGeneration()
		{
			return true;
		}
	}
}
