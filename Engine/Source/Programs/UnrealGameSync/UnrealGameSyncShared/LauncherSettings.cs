// Copyright Epic Games, Inc. All Rights Reserved.

using Microsoft.Win32;
using System;
using System.Linq;
using System.Runtime.InteropServices;
using System.Runtime.Versioning;
using System.Text.RegularExpressions;

namespace UnrealGameSync
{
	public class LauncherSettings
	{
		public string? PerforceServerAndPort { get; set; }
		public string? PerforceUserName { get; set; }
		public string? PerforceDepotPath { get; set; }

		public bool PreviewBuild { get; set; }

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

				if (String.IsNullOrEmpty(PerforceServerAndPort))
				{
					Utility.DeleteRegistryKey(key, "ServerAndPort");
				}
				else
				{
					key.SetValue("ServerAndPort", PerforceServerAndPort);
				}

				if (String.IsNullOrEmpty(PerforceUserName))
				{
					Utility.DeleteRegistryKey(key, "UserName");
				}
				else
				{
					key.SetValue("UserName", PerforceUserName);
				}

				if (String.IsNullOrEmpty(PerforceDepotPath) || (DeploymentSettings.Instance.DefaultDepotPath != null && String.Equals(PerforceDepotPath, DeploymentSettings.Instance.DefaultDepotPath, StringComparison.OrdinalIgnoreCase)))
				{
					Utility.DeleteRegistryKey(key, "DepotPath");
				}
				else
				{
					key.SetValue("DepotPath", PerforceDepotPath);
				}

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
	}
}
