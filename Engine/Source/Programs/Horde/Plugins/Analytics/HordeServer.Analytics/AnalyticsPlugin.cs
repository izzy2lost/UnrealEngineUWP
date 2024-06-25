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
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;

namespace HordeServer
{
	/// <summary>
	/// Main entry point for the analytics plugin
	/// </summary>
	[Plugin("Analytics", ServerConfigType = typeof(AnalyticsServerConfig), GlobalConfigType = typeof(AnalyticsGlobalConfig))]
	public class AnalyticsPlugin : IPluginStartup
	{
		/// <inheritdoc/>
		public void ConfigureServices(IServiceCollection services)
		{
			services.AddSingleton<TelemetryManager>();
			services.AddSingleton<ITelemetryWriter>(sp => sp.GetRequiredService<TelemetryManager>());
			services.AddHostedService(sp => sp.GetRequiredService<TelemetryManager>());
			services.AddSingleton<MongoTelemetrySink>();
			services.AddHostedService(sp => sp.GetRequiredService<MongoTelemetrySink>());
			services.AddSingleton<MetricTelemetrySink>();

			services.AddSingleton<MetricCollection>();
			services.AddHostedService(sp => sp.GetRequiredService<MetricCollection>());
			services.AddSingleton<IMetricCollection, MetricCollection>(sp => sp.GetRequiredService<MetricCollection>());

			services.AddHttpClient(EpicTelemetrySink.HttpClientName, client => { });
		}
	}
}
