// Copyright Epic Games, Inc. All Rights Reserved.

using JetBrains.Annotations;
using System;
using System.Diagnostics;
using System.IO;
using EpicGames.Core;
using AutomationTool;
using UnrealBuildTool;
using Gauntlet;
using UnrealBuildBase;
using EpicGames.Horde.Storage;

namespace LowLevelTests
{
	public class WebTestsLowLevelTestsExtension : ILowLevelTestsExtension
	{
		private Process ServerProcess;

		public bool IsSupported(UnrealTargetPlatform InPlatform, string InTestApp)
		{
			return InTestApp == "WebTests";
		}

		public string ExtraCommandLine(UnrealTargetPlatform InPlatform, string InTestApp, string InBuildPath)
		{
			return string.Format("--web_server_ip{0}", UnrealHelpers.GetHostIpAddress());
		}

		public void PreRunTests()
		{
			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.WorkingDirectory = Path.Combine(Unreal.EngineDirectory.FullName, "Source", "Programs", "WebTests", "WebServer");
			StartInfo.FileName = RuntimePlatform.IsWindows ? "cmd.exe" : "/bind/sh";
			StartInfo.Arguments = RuntimePlatform.IsWindows ? "/c runserver.bat" : "-c 'runserver.sh'";
			StartInfo.UseShellExecute = false;
			StartInfo.CreateNoWindow = true;
			StartInfo.RedirectStandardInput = true;
			StartInfo.RedirectStandardOutput = true;
			StartInfo.RedirectStandardError = true;

			ServerProcess = new Process();
			ServerProcess.StartInfo = StartInfo;
			ServerProcess.Start();
		}

		public void PostRunTests()
		{
			if (ServerProcess != null)
			{
				ServerProcess.Kill();
				ServerProcess = null;
			}
		}
	}
}
