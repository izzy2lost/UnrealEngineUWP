// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using HordeServer.Plugins;
using HordeServer.Server;

namespace HordeServer.Tests
{
	static class PluginConfigExtensions
	{
		public static void AddAnalytics(this PluginConfigCollection configCollection, AnalyticsGlobalConfig config)
			=> configCollection[new PluginName("Analytics")] = config;
	}
}
