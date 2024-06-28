// Copyright Epic Games, Inc. All Rights Reserved.

using Amazon;
using Amazon.AutoScaling;
using Amazon.CloudWatch;
using Amazon.EC2;
using Amazon.Extensions.NETCore.Setup;
using Amazon.SQS;
using HordeServer.Agents.Fleet;
using HordeServer.Agents.Relay;
using HordeServer.Aws;
using HordeServer.Compute;
using HordeServer.Plugins;
using HordeServer.Tasks;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;

namespace HordeServer
{
	/// <summary>
	/// Entry point for the storage plugin
	/// </summary>
	[Plugin("Compute", GlobalConfigType = typeof(ComputeConfig), ServerConfigType = typeof(StaticComputeConfig))]
	public class ComputePlugin : IPluginStartup
	{
		readonly IServerInfo _serverInfo;
		readonly StaticComputeConfig _staticComputeConfig;

		/// <summary>
		/// Constructor
		/// </summary>
		public ComputePlugin(IServerInfo serverInfo, StaticComputeConfig staticComputeConfig)
		{
			_serverInfo = serverInfo;
			_staticComputeConfig = staticComputeConfig;
		}

		/// <inheritdoc/>
		public void ConfigureServices(IServiceCollection services)
		{
			services.AddSingleton<AwsAutoScalingLifecycleService>();
			services.AddSingleton<FleetService>();
			services.AddSingleton<IFleetManagerFactory, FleetManagerFactory>();
			services.AddSingleton<IPoolSizeStrategyFactory, NoOpPoolSizeStrategyFactory>();
			services.AddSingleton<IPoolSizeStrategyFactory, LeaseUtilizationStrategyFactory>();

			// Associate IFleetManager interface with the default implementation from config for convenience
			// Though most fleet managers are created on a per-pool basis
			services.AddSingleton<IFleetManager>(ctx => ctx.GetRequiredService<IFleetManagerFactory>().CreateFleetManager(FleetManagerType.Default));

			// Run the tunnel service for all run modes
			services.AddSingleton<TunnelService>();
			services.AddHostedService<TunnelService>(sp => sp.GetRequiredService<TunnelService>());

			// Runs the agent relay service for all run modes to notify long-polling requests
			services.AddSingleton<AgentRelayService>();
			services.AddHostedService(provider => provider.GetRequiredService<AgentRelayService>());

			if (!_serverInfo.ReadOnlyMode)
			{
				services.AddSingleton<ComputeTaskSource>();
				services.AddSingleton<ITaskSource, ComputeTaskSource>(provider => provider.GetRequiredService<ComputeTaskSource>());

				services.AddSingleton<ITaskSource, UpgradeTaskSource>();
				services.AddSingleton<ITaskSource, ShutdownTaskSource>();
				services.AddSingleton<ITaskSource, RestartTaskSource>();
			}

			if (_staticComputeConfig.WithAws)
			{
				AWSOptions awsOptions = _serverInfo.Configuration.GetAWSOptions();
				services.AddDefaultAWSOptions(awsOptions);
				if (awsOptions.Region == null && Environment.GetEnvironmentVariable("AWS_REGION") == null)
				{
					awsOptions.Region = RegionEndpoint.USEast1;
				}

				services.AddAWSService<IAmazonCloudWatch>();
				services.AddAWSService<IAmazonAutoScaling>();
				services.AddAWSService<IAmazonSQS>();
				services.AddAWSService<IAmazonEC2>();

				services.AddSingleton<AwsCloudWatchMetricExporter>();

				services.AddSingleton<IPoolSizeStrategyFactory, LeaseUtilizationAwsMetricStrategyFactory>();
				services.AddSingleton<IPoolSizeStrategyFactory, ComputeQueueAwsMetricStrategyFactory>();

				if (_serverInfo.IsRunModeActive(RunMode.Worker))
				{
					services.AddHostedService(provider => provider.GetRequiredService<AwsAutoScalingLifecycleService>());
					services.AddHostedService(provider => provider.GetRequiredService<AwsCloudWatchMetricExporter>());
				}
			}
		}
	}
}
