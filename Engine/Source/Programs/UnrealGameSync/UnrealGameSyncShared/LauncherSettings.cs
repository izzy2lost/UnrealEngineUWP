// Copyright Epic Games, Inc. All Rights Reserved.

using Microsoft.Win32;
using System;
using System.Runtime.InteropServices;
using System.Runtime.Versioning;
using System.Text.RegularExpressions;

namespace UnrealGameSync
{
	public enum LauncherUpdateSource
	{
		Perforce = 0,
		Horde = 1,
	}

	public class LauncherSettings
	{
		public LauncherUpdateSource UpdateSource { get; set; }

		public string? HordeServer { get; set; }

		public string? PerforceServerAndPort { get; set; }
		public string? PerforceUserName { get; set; }
		public string? PerforceDepotPath { get; set; }

		public bool PreviewBuild { get; set; }

		public LauncherSettings()
		{
			HordeServer = DeploymentSettings.Instance.HordeUrl;
			PerforceDepotPath = DeploymentSettings.Instance.DefaultDepotPath;
		}

		public LauncherSettings(LauncherSettings other)
		{
			PerforceServerAndPort = other.PerforceServerAndPort;
			PerforceUserName = other.PerforceUserName;
			PerforceDepotPath = other.PerforceDepotPath;

			PreviewBuild = other.PreviewBuild;
		}

		public void Read()
		{
			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				ReadFromRegistry();
			}
		}

		[SupportedOSPlatform("windows")]
		void ReadFromRegistry()
		{
			using (RegistryKey? key = Registry.CurrentUser.OpenSubKey("SOFTWARE\\Epic Games\\UnrealGameSync", false))
			{
				if (key != null)
				{
					LauncherUpdateSource updateSource;
					if (Enum.TryParse(key.GetValue("UpdateSource", UpdateSource) as string, out updateSource))
					{
						UpdateSource = updateSource;
					}

					HordeServer = key.GetValue("HordeServer", HordeServer) as string;
					PerforceServerAndPort = key.GetValue("ServerAndPort", PerforceServerAndPort) as string;
					PerforceUserName = key.GetValue("UserName", PerforceUserName) as string;
					PerforceDepotPath = key.GetValue("DepotPath", PerforceDepotPath) as string;
					PreviewBuild = ((key.GetValue("Preview", PreviewBuild? 1 : 0) as int?) ?? 0) != 0;

					// Fix corrupted depot path string
					if (PerforceDepotPath != null)
					{
						Match match = Regex.Match(PerforceDepotPath, "^(.*)/(Release|UnstableRelease)/\\.\\.\\.@.*$");
						if (match.Success)
						{
							PerforceDepotPath = match.Groups[1].Value;
							Save();
						}
					}
				}
			}
		}

		public bool Save()
		{
			try
			{
				if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
				{
					SaveToRegistry();
				}
				return true;
			}
			catch
			{
				return false;
			}
		}

		[SupportedOSPlatform("windows")]
		void SaveToRegistry()
		{
			using (RegistryKey key = Registry.CurrentUser.CreateSubKey("SOFTWARE\\Epic Games\\UnrealGameSync"))
			{
				// Delete this legacy setting
				Utility.DeleteRegistryKey(key, "Server");

				SaveRegistryValue(key, "Source", UpdateSource.ToString(), LauncherUpdateSource.Perforce.ToString());
				SaveRegistryValue(key, "HordeServer", HordeServer, DeploymentSettings.Instance.HordeUrl);
				SaveRegistryValue(key, "ServerAndPort", PerforceServerAndPort, null);
				SaveRegistryValue(key, "UserName", PerforceUserName, null);
				SaveRegistryValue(key, "DepotPath", PerforceDepotPath, DeploymentSettings.Instance.DefaultDepotPath);

				if (PreviewBuild)
				{
					key.SetValue("Preview", 1);
				}
				else
				{
					Utility.DeleteRegistryKey(key, "Preview");
				}
			}
		}

		[SupportedOSPlatform("windows")]
		static void SaveRegistryValue(RegistryKey key, string name, string? value, string? defaultValue)
		{
			if (String.IsNullOrEmpty(value) || String.Equals(value, defaultValue, StringComparison.OrdinalIgnoreCase))
			{
				Utility.DeleteRegistryKey(key, name);
			}
			else
			{
				key.SetValue(name, value);
			}
		}
	}
}
