// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using UnrealBuildTool;
using EpicGames.Core;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.IO;

namespace Gauntlet
{
	enum EConfigurationNamespace
	{
		Engine,
		Snapshot,
		Project
	}

	/// <summary>
	/// Base class for a platform configuration profile.
	/// This class needs to be implemented for each configurable platform
	/// </summary>
	public abstract class PlatformConfigurationBase
	{
		/// <summary>
		/// Name of the config profile.
		/// This name doesn't need to be unique across namespaces.
		/// </summary>
		public string ProfileName { get; set; }

		/// <summary>
		/// Namespace of the profile. Profiles have to have a unique name inside a namespace.
		/// </summary>
		public string Namespace { get; set; }

		/// <summary>
		/// Platform this configuration profile is for
		/// </summary>
		public UnrealTargetPlatform Platform { get; set; }
	}

	/// <summary>
	/// Base interface for a platform specific configuration reader
	/// </summary>
	public interface IPlatformConfigurationReader
	{
		bool SupportsPlatform(UnrealTargetPlatform? Platform);

		/// <summary>
		/// Returns the supported platform config file extension
		/// </summary>
		/// <returns></returns>
		string ConfigFileExtension();

		/// <summary>
		/// Reads the configuration from the passed in file location
		/// </summary>
		/// <param name="Location">An absolute path the to a file with platform configuration</param>
		/// <returns></returns>
		PlatformConfigurationBase ReadConfiguration(FileReference Location);
	}

	/// <summary>
	/// A singleton class that encapsulates a cache of device config profiles 
	/// </summary>
	public class DeviceConfigurationCache
	{

		/// <summary>
		/// Key by which a configuration profile is identified in the cache.
		/// </summary>
		class ConfigurationCacheKey
		{
			public UnrealTargetPlatform Platform;
			public string Namespace;
			public string ProfileName;

			public ConfigurationCacheKey(UnrealTargetPlatform Platform, string ProjectName, string ProfileName)
			{
				this.Platform = Platform;
				this.Namespace = ProjectName;
				this.ProfileName = ProfileName;
			}

			public ConfigurationCacheKey(PlatformConfigurationBase Configuration)
			{
				this.Platform = Configuration.Platform;
				this.Namespace = Configuration.Namespace;
				this.ProfileName = Configuration.ProfileName;
			}

			public override bool Equals(object Other)
			{
				ConfigurationCacheKey OtherKey = Other as ConfigurationCacheKey;
				return OtherKey != null &&
					OtherKey.Platform == Platform &&
					OtherKey.ProfileName == ProfileName &&
					OtherKey.Namespace == Namespace;

			}

			public override int GetHashCode()
			{
				return (Platform, Namespace, ProfileName).GetHashCode();
			}
		}

		Object LockObject = new Object();

		Dictionary<ConfigurationCacheKey, PlatformConfigurationBase> ConfigurationCache = new();

		private static DeviceConfigurationCache _Instance;

		protected DeviceConfigurationCache()
		{
			if (_Instance == null)
			{
				_Instance = this;
			}
		}

		public static DeviceConfigurationCache Instance
		{
			get
			{
				if (_Instance == null)
				{
					_Instance = new DeviceConfigurationCache();
				}

				return _Instance;
			}
		}

		/// <summary>
		/// Scans the SettingDir for configuration profiles
		/// </summary>
		/// <param name="Platform">Platform for which to look for config files</param>
		/// <param name="Namespace">The namespace in which to put the configuration profiles</param>
		/// <param name="SettingDir"></param>
		/// <returns></returns>
		public bool DiscoverConfigurationProfiles(UnrealTargetPlatform? Platform, string Namespace, string SettingDir)
		{
			if (Platform == null || !Directory.Exists(SettingDir))
			{
				return false;
			}

			IPlatformConfigurationReader ConfigReader = Gauntlet.Utils.InterfaceHelpers.FindImplementations<IPlatformConfigurationReader>(true).ToList().Find(D => D.SupportsPlatform(Platform));
			if (ConfigReader == null)
			{
				Log.Info("Couldn't find a configuration reader for {0}", Platform);
				return false;
			}

			foreach (string File in Directory.EnumerateFiles(SettingDir, ConfigReader.ConfigFileExtension(), SearchOption.AllDirectories))
			{
				FileReference FileRef = new FileReference(File);
				PlatformConfigurationBase DeviceProfile = ConfigReader.ReadConfiguration(FileRef);
				if (DeviceProfile != null)
				{
					DeviceProfile.Platform = Platform.Value;
					DeviceProfile.Namespace = Namespace;
					DeviceProfile.ProfileName = FileRef.GetFileNameWithoutAnyExtensions();

					lock (LockObject)
					{
						ConfigurationCacheKey Key = new ConfigurationCacheKey(Platform.Value, Namespace, DeviceProfile.ProfileName);
						if (ConfigurationCache.ContainsKey(Key))
						{
							Log.Verbose("Device configuration profile {0} already exists", FileRef.FullName);
							continue;
						}

						ConfigurationCache.Add(Key, DeviceProfile);
					}
				}
			}

			return true;
		}

		public void CacheConfigurationSnapshot(PlatformConfigurationBase Configuration, bool Overwrite = false)
		{
			lock (LockObject)
			{
				ConfigurationCacheKey Key = new ConfigurationCacheKey(Configuration.Platform, "Snapshot", Configuration.ProfileName);
				if (ConfigurationCache.ContainsKey(Key) && !Overwrite)
				{
					return;
				}

				ConfigurationCache.Add(Key, Configuration);
			}
		}

		public PlatformConfigurationBase GetConfigurationSnapshot(UnrealTargetPlatform? Platform, string DeviceName)
		{
			return GetConfiguration(Platform, "Snapshot", DeviceName);
		}

		public void ClearSnapshot(PlatformConfigurationBase Snapshot)
		{
			lock (LockObject)
			{
				ConfigurationCacheKey Key = new ConfigurationCacheKey(Snapshot);
				ConfigurationCache.Remove(Key);
			}
		}

		public PlatformConfigurationBase GetConfiguration(UnrealTargetPlatform? Platform, string Namespace, string ProfileName)
		{
			if (Platform == null)
			{
				return null;
			}

			PlatformConfigurationBase Res = null;

			lock (LockObject)
			{
				ConfigurationCacheKey Key = new ConfigurationCacheKey(Platform.Value, Namespace, ProfileName);
				ConfigurationCache.TryGetValue(Key, out Res);
			}

			return Res;
		}
	}
}
