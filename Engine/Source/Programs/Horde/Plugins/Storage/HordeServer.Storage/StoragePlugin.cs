// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Bundles;
using EpicGames.Horde.Storage.ObjectStores;
using HordeServer.Plugins;
using HordeServer.Storage.ObjectStores;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace HordeServer.Storage
{
	/// <summary>
	/// Entry point for the storage plugin
	/// </summary>
	[Plugin("Storage", GlobalConfigType = typeof(StorageConfig), ServerConfigType = typeof(StaticStorageConfig))]
	public class StoragePlugin : IPluginStartup
	{
		readonly StaticStorageConfig _staticConfig;

		public StoragePlugin(StaticStorageConfig staticConfig)
		{
			_staticConfig = staticConfig;
		}

		/// <inheritdoc/>
		public void ConfigureServices(IServiceCollection services)
		{
			services.AddSingleton<StorageService>();
			services.AddSingleton<IStorageService>(sp => sp.GetRequiredService<StorageService>());
			services.AddScoped(sp => sp.GetRequiredService<StorageService>().CreateStorageClientFactory(sp.GetRequiredService<IOptionsSnapshot<StorageConfig>>().Value));

			services.AddSingleton<IObjectStoreFactory, ObjectStoreFactory>();
			services.AddSingleton<AwsObjectStoreFactory>();
			services.AddSingleton<FileObjectStoreFactory>();

			services.AddSingleton<BundleCache>();
			services.AddSingleton<StorageBackendCache>(CreateStorageBackendCache);
		}

		StorageBackendCache CreateStorageBackendCache(IServiceProvider serviceProvider)
		{
			IServerInfo serverInfo = serviceProvider.GetRequiredService<IServerInfo>();
			DirectoryReference cacheDir = DirectoryReference.Combine(serverInfo.DataDir, String.IsNullOrEmpty(_staticConfig.BundleCacheDir) ? "Cache" : _staticConfig.BundleCacheDir);
			return new StorageBackendCache(cacheDir, _staticConfig.BundleCacheSizeBytes, serviceProvider.GetRequiredService<ILogger<StorageBackendCache>>());
		}
	}
}
