// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Linq;
using System.IO;
using System.Collections.Generic;
using EpicGames.Core;
using AutomationTool;
using System.Threading;
using UnrealBuildBase;
using Microsoft.Extensions.Logging;
using UnrealBuildTool;

class ExportAppleBuild : BuildCommand
{
	public override ExitCode Execute()
	{
		FileReference ProjectFile = ParseProjectParam();
		if (ProjectFile == null)
		{
			Logger.LogError("Project not specified, of found, with -project param");
			return ExitCode.Error_Arguments;
		}

		string PlistParam = ParseParamValue("Plist");
		string XcArchivePathParam = ParseParamValue("XcArchive");
		string LatestXcArchiveParam = ParseParamValue("LatestXcArchiveForTarget");
		string LatestXcArchiveSearch = ParseParamValue("LatestXcArchiveSearchPath");
		string ExportPath = ParseParamValue("ExportPath");

		DirectoryReference XcArchive;
		if (!string.IsNullOrEmpty(LatestXcArchiveParam))
		{
			// find the latest archive for the given project name
			DirectoryReference SearchPath = null;
			if (!string.IsNullOrEmpty(LatestXcArchiveSearch))
			{
				SearchPath = new DirectoryReference(LatestXcArchiveSearch);
			}
			XcArchive = AppleExports.FindLatestXcArchive(LatestXcArchiveParam, SearchPath);
		}
		else
		{
			XcArchive = new DirectoryReference(XcArchivePathParam);			
		}

		if (XcArchive == null || !DirectoryReference.Exists(XcArchive))
		{
			Logger.LogError("No XCArchive found, with -XCArchive or -LatestXCArchiveForTarget params");
			return ExitCode.Error_Arguments;
		}

		string PlistPath;
		if (!string.IsNullOrEmpty(PlistParam) && PlistParam[0] == '/')
		{
			PlistPath = PlistParam;
		}
		else
		{
			// look in project and engine for named plist
			PlistPath = Path.Combine(ProjectFile.Directory.FullName, "Build", "Xcode", PlistParam + ".plist");
			if (!File.Exists(PlistPath))
			{
				PlistPath = Path.Combine(Unreal.EngineDirectory.FullName, "Build", "Xcode", PlistParam + ".plist");
			}
		}
		if (!File.Exists(PlistPath))
		{
			Logger.LogError("No plist options file found, with -plist param");
			return ExitCode.Error_Arguments;
		}

		Logger.LogInformation("Project: {Project}, XCArchive: {XC}, Plist {Plist}", ProjectFile, XcArchive, PlistPath);

		// build commandline
		string CommandLine = $"-exportArchive";
		CommandLine += $" -archivePath \"{XcArchive}\"";
		CommandLine += $" -exportOptionsPlist \"{PlistPath}\"";
		CommandLine += $" -allowProvisioningUpdates";
		CommandLine += AppleExports.GetXcodeBuildAuthOptions(ProjectFile);

		if (!string.IsNullOrEmpty(ExportPath))
		{
			CommandLine += $" -exportPath {ExportPath}";
		}

		Logger.LogInformation($"Running 'xcodebuild {CommandLine}'...");

		int Return;
		string Output = Utils.RunLocalProcessAndReturnStdOut("/usr/bin/xcodebuild", CommandLine, null, out Return);
		Logger.LogInformation(Output);

		return (ExitCode)Return;
	}
}
