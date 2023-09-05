// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics;
using System.IO;
using EpicGames.Core;
using UnrealBuildTool;
using Gauntlet;
using UnrealBuildBase;

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
			InstallWebServer();
			RunWebServer();
		}

		private string WebTestsServerDir()
		{
			return Path.Combine(Unreal.EngineDirectory.FullName, "Source", "Programs", "WebTestsServer");
		}

		private void InstallWebServer()
		{
			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.WorkingDirectory = WebTestsServerDir();
			StartInfo.FileName = RuntimePlatform.IsWindows ? "cmd.exe" : "/bind/sh";
			StartInfo.Arguments = RuntimePlatform.IsWindows ? "/c createenv.bat" : "-c 'createenv.sh'";
			StartInfo.UseShellExecute = false;
			StartInfo.CreateNoWindow = true;

			Process InstallProcess = new Process();
			InstallProcess.StartInfo = StartInfo;
			InstallProcess.Start();
			InstallProcess.WaitForExit();

			Console.WriteLine("Requirements installed.");
		}

		private void RunWebServer()
		{
			string WorkingDir = WebTestsServerDir();
			string PythonFile = Path.Combine(WorkingDir, "env", "Scripts", RuntimePlatform.IsWindows ? "python.exe" : "python");

			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.WorkingDirectory = WorkingDir;
			StartInfo.FileName = PythonFile;
			StartInfo.WindowStyle = ProcessWindowStyle.Normal;
			StartInfo.Arguments = "manage.py runserver 0.0.0.0:8000";
			StartInfo.UseShellExecute = true;
			StartInfo.CreateNoWindow = false;

			ServerProcess = new Process();
			ServerProcess.StartInfo = StartInfo;
			ServerProcess.Start();

			Console.WriteLine("Web server is now running.");
		}

		public void PostRunTests()
		{
			if (ServerProcess != null)
			{
				ServerProcess.CloseMainWindow();
				ServerProcess = null;

				Console.WriteLine("Web server killed.");
			}
		}
	}
}
