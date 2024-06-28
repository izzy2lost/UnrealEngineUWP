// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Core;
using HordeServer.Agents;
using HordeServer.Agents.Fleet;
using HordeServer.Agents.Pools;
using HordeServer.Agents.Utilization;
using HordeServer.Artifacts;
using HordeServer.Commits;
using HordeServer.Devices;
using HordeServer.Issues;
using HordeServer.Issues.External;
using HordeServer.Jobs;
using HordeServer.Jobs.Bisect;
using HordeServer.Jobs.Graphs;
using HordeServer.Jobs.Schedules;
using HordeServer.Jobs.Templates;
using HordeServer.Jobs.TestData;
using HordeServer.Jobs.Timing;
using HordeServer.Logs;
using HordeServer.Notifications;
using HordeServer.Notifications.Sinks;
using HordeServer.Perforce;
using HordeServer.Plugins;
using HordeServer.Replicators;
using HordeServer.Streams;
using HordeServer.Ugs;
using HordeServer.Users;
using Microsoft.Extensions.DependencyInjection;

namespace HordeServer
{
	/// <summary>
	/// Entry point for the storage plugin
	/// </summary>
	[Plugin("Build", GlobalConfigType = typeof(BuildConfig), ServerConfigType = typeof(StaticBuildConfig))]
	public class BuildPlugin : IPluginStartup
	{
		readonly IServerInfo _serverInfo;
		readonly StaticBuildConfig _staticConfig;

		public BuildPlugin(IServerInfo serverInfo, StaticBuildConfig staticConfig)
		{
			_serverInfo = serverInfo;
			_staticConfig = staticConfig;
		}

		/// <inheritdoc/>
		public void ConfigureServices(IServiceCollection services)
		{
			services.AddSingleton<IArtifactCollection, ArtifactCollection>();
			services.AddSingleton<IGraphCollection, GraphCollection>();
			services.AddSingleton<IssueCollection>();
			services.AddSingleton<IIssueCollection>(sp => sp.GetRequiredService<IssueCollection>());
			services.AddSingleton<ILogExtIssueProvider>(sp => sp.GetRequiredService<IssueCollection>());
			services.AddSingleton<IJobCollection, JobCollection>();
			services.AddSingleton<IJobStepRefCollection, JobStepRefCollection>();
			services.AddSingleton<IJobTimingCollection, JobTimingCollection>();
			services.AddSingleton<IBisectTaskCollection, BisectTaskCollection>();
			services.AddSingleton<IReplicatorCollection, ReplicatorCollection>();
			services.AddSingleton<IUgsMetadataCollection, UgsMetadataCollection>();
			services.AddSingleton<ISubscriptionCollection, SubscriptionCollection>();
			services.AddSingleton<IStreamCollection, StreamCollection>();
			services.AddSingleton<ITemplateCollection, TemplateCollection>();
			services.AddSingleton<ITestDataCollection, TestDataCollection>();
			services.AddSingleton<IUtilizationDataCollection, UtilizationDataCollection>();
			services.AddSingleton<ITemplateCollection, TemplateCollection>();

			services.AddSingleton<IPoolSizeStrategyFactory, JobQueueStrategyFactory>();
			services.AddHostedService<ArtifactExpirationService>();
			services.AddSingleton<ICommitService, CommitService>();

			services.AddSingleton<DeviceService>();
			services.AddSingleton<IBlockCache>(sp => CreateBlockCache(sp));
			services.AddSingleton<TestDataService>();

			if (_staticConfig.Commits.ReplicateMetadata)
			{
				services.AddSingleton<PerforceServiceCache>();
				services.AddSingleton<IPerforceService>(sp => sp.GetRequiredService<PerforceServiceCache>());
			}
			else
			{
				services.AddSingleton<PerforceService>();
				services.AddSingleton<IPerforceService>(sp => sp.GetRequiredService<PerforceService>());
			}
			services.AddSingleton<PerforceReplicator>();

			services.AddSingleton<PerforceLoadBalancer>();
			services.AddSingleton<ReplicationService>();

			if (_staticConfig.SlackToken != null)
			{
				services.AddSingleton<SlackNotificationSink>();
				services.AddSingleton<IAvatarService, SlackNotificationSink>(sp => sp.GetRequiredService<SlackNotificationSink>());
				services.AddSingleton<INotificationSink, SlackNotificationSink>(sp => sp.GetRequiredService<SlackNotificationSink>());
			}
			else
			{
				services.AddSingleton<IAvatarService, NullAvatarService>();
			}

			if (_staticConfig.JiraUrl != null)
			{
				services.AddSingleton<IExternalIssueService, JiraService>();
			}
			else
			{
				services.AddSingleton<IExternalIssueService, DefaultExternalIssueService>();
			}

			if (_serverInfo.IsRunModeActive(RunMode.Worker) && !_serverInfo.ReadOnlyMode)
			{
				services.AddHostedService<AgentReportService>();
				services.AddHostedService<BisectService>();
				services.AddHostedService(provider => provider.GetRequiredService<IssueService>());
				services.AddHostedService<IssueReportService>();
				services.AddHostedService<IssueTagService>();
				services.AddHostedService<JobExpirationService>();
				services.AddHostedService(provider => provider.GetRequiredService<PerforceLoadBalancer>());
				services.AddHostedService<PoolUpdateService>();
				services.AddHostedService<UtilizationDataService>();
				services.AddHostedService(provider => provider.GetRequiredService<DeviceService>());
				services.AddHostedService<DeviceReportService>();
				services.AddHostedService(provider => provider.GetRequiredService<TestDataService>());

				if (_staticConfig.Commits.ReplicateMetadata)
				{
					services.AddHostedService(provider => provider.GetRequiredService<PerforceServiceCache>());
				}

				if (_staticConfig.Commits.ReplicateContent)
				{
					services.AddHostedService(provider => provider.GetRequiredService<ReplicationService>());
				}

				if (!_staticConfig.DisableSchedules)
				{
					services.AddHostedService(provider => provider.GetRequiredService<ScheduleService>());
				}

				if (_staticConfig.SlackToken != null)
				{
					services.AddHostedService(provider => provider.GetRequiredService<SlackNotificationSink>());
				}
			}
		}

		BlockCache CreateBlockCache(IServiceProvider serviceProvider)
		{
			DirectoryReference cacheDir = DirectoryReference.Combine(_serverInfo.DataDir, String.IsNullOrEmpty(_staticConfig.BlockCacheDir) ? "BlockCache" : _staticConfig.BlockCacheDir);
			return BlockCache.Create(cacheDir, (int)(_staticConfig.BlockCacheSizeBytes / (1024 * 1024 * 1024)));
		}
	}

	/// <summary>
	/// Helper methods for storage config
	/// </summary>
	public static class BuildPluginExtensions
	{
		/// <summary>
		/// Configures the storage plugin
		/// </summary>
		public static void AddBuildConfig(this IDictionary<PluginName, IPluginConfig> dictionary, BuildConfig buildConfig)
			=> dictionary[new PluginName("Build")] = buildConfig;

		/// <summary>
		/// Configures the storage plugin
		/// </summary>
		public static BuildConfig GetBuildConfig(this IDictionary<PluginName, IPluginConfig> dictionary)
			=> (BuildConfig)dictionary[new PluginName("Build")];

		/// <summary>
		/// Configures the storage plugin
		/// </summary>
		public static bool TryGetBuildConfig(this IDictionary<PluginName, IPluginConfig> dictionary, [NotNullWhen(true)] out BuildConfig? buildConfig)
		{
			IPluginConfig? pluginConfig;
			if (dictionary.TryGetValue(new PluginName("Build"), out pluginConfig) && pluginConfig is BuildConfig newBuildConfig)
			{
				buildConfig = newBuildConfig;
				return true;
			}
			else
			{
				buildConfig = null;
				return false;
			}
		}
	}
}
