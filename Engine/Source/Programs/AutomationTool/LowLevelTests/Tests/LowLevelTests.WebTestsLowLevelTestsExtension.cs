// Copyright Epic Games, Inc. All Rights Reserved.

using JetBrains.Annotations;
using System;
using System.Diagnostics;
using System.IO;
using System.Text;
using EpicGames.Core;
using AutomationTool;
using UnrealBuildTool;
using Gauntlet;
using UnrealBuildBase;
using EpicGames.Horde.Storage;
using System.Net.Sockets;

namespace LowLevelTests
{
	public class WebTestsLowLevelTestsExtension : ILowLevelTestsExtension
	{
		private Process ServerProcess;
		private StringBuilder logBuilder = new StringBuilder();

		public bool IsSupported(UnrealTargetPlatform InPlatform, string InTestApp)
		{
			return InTestApp == "WebTests";
		}

		public string ExtraCommandLine(UnrealTargetPlatform InPlatform, string InTestApp, string InBuildPath)
		{
			return string.Format("--web_server_ip{0}", UnrealHelpers.GetHostIpAddress());
		}

		private bool IsServerPortOpen(string ipAddress, int port)
		{
			using (TcpClient client = new TcpClient())
			{
				try
				{
					client.Connect(ipAddress, port);
					return true;
				}
				catch
				{
					return false;
				}
			}
		}

		public void PreRunTests()
		{
			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.WorkingDirectory = Path.Combine(Unreal.EngineDirectory.FullName, "Source", "Programs", "WebTestsServer");
			StartInfo.FileName = RuntimePlatform.IsWindows ? "cmd.exe" : "/bind/sh";
			StartInfo.Arguments = RuntimePlatform.IsWindows ? "/c runserver.bat" : "-c 'runserver.sh'";
			StartInfo.UseShellExecute = false;
			StartInfo.CreateNoWindow = true;
			StartInfo.RedirectStandardInput = true;
			StartInfo.RedirectStandardOutput = true;
			StartInfo.RedirectStandardError = true;

			ServerProcess = new Process();
			ServerProcess.StartInfo = StartInfo;

			ServerProcess.OutputDataReceived += (sender, e) =>
			{
				if (!string.IsNullOrEmpty(e.Data))
				{
					logBuilder.AppendLine(e.Data);
				}
			};

			ServerProcess.ErrorDataReceived += (sender, e) =>
			{
				if (!string.IsNullOrEmpty(e.Data))
				{
					logBuilder.AppendLine(e.Data);
				}
			};

			ServerProcess.Start();
			ServerProcess.BeginOutputReadLine();
			ServerProcess.BeginErrorReadLine();

			Stopwatch sw = new Stopwatch();
			sw.Start();

			while (!IsServerPortOpen(UnrealHelpers.GetHostIpAddress(), 8000))
			{
				if (sw.ElapsedMilliseconds > 60000)
				{
					sw.Stop();
					throw new TimeoutException("Server port did not open within the specified time.");
				}
				System.Threading.Thread.Sleep(1000);
			}

			sw.Stop();

			Console.WriteLine("server port is now open.");
		}

		public void PostRunTests()
		{
			if (ServerProcess != null)
			{
				ServerProcess.Kill();
				ServerProcess = null;

				string serverOutput = logBuilder.ToString();
				Console.WriteLine("Server Output:");
				Console.WriteLine(serverOutput);

				string serverOutputLog = Path.Combine(Path.GetFullPath(Globals.LogDir).Replace("GauntletTemp", "Engine\\Programs\\AutomationTool\\Saved"), "ServerOutput.log");
				File.WriteAllText(serverOutputLog, serverOutput);
			}
		}
	}
}
