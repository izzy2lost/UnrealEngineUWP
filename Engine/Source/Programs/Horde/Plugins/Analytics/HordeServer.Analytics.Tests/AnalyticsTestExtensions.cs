// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using HordeServer.Plugins;
using HordeServer.Telemetry;
using HordeServer.Telemetry.Metrics;
using HordeServer.Telemetry.Sinks;
using Microsoft.Extensions.DependencyInjection;

namespace HordeServer.Analytics.Tests
{
	public static class AnalyticsTestExtensions
	{
		public static void AddAnalyticsTestConfig(this PluginConfigCollection configCollection, AnalyticsConfig config)
			=> configCollection[new PluginName("Analytics")] = config;
	}
}
