// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Bundles;
using EpicGames.Horde.Storage.ObjectStores;
using HordeServer.Plugins;
using HordeServer.Storage;
using Microsoft.Extensions.DependencyInjection;

namespace HordeServer.Tests
{
	public static class StorageTestExtensions
	{
		public static void AddStorageTestConfig(this PluginConfigCollection configCollection, StorageConfig config)
			=> configCollection[new PluginName("Storage")] = config;
	}
}
