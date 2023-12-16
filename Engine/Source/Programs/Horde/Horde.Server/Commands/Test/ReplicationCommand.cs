// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Server.Server;
using Microsoft.Extensions.Logging;
using Horde.Server.Perforce;
using System;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Horde.Server.Streams;
using EpicGames.Horde.Streams;
using Horde.Server.Configuration;
using EpicGames.Horde.Replicators;

namespace Horde.Server.Commands.Test
{
	[Command("test", "replication", "Replicates commits for a particular change of changes from Perforce")]
	class TestReplicationCommand : Command
	{
		[CommandLine("-Stream=", Required = true)]
		public string StreamId { get; set; } = String.Empty;

		[CommandLine("-Replicator=", Required = true)]
		public string ReplicatorId { get; set; } = String.Empty;

		[CommandLine(Required = true)]
		public int Change { get; set; }

		[CommandLine]
		public bool Clean { get; set; }

		readonly IConfiguration _configuration;
		readonly ILoggerProvider _loggerProvider;

		public TestReplicationCommand(IConfiguration configuration, ILoggerProvider loggerProvider)
		{
			_configuration = configuration;
			_loggerProvider = loggerProvider;
		}

		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			ServiceCollection serviceCollection = Startup.CreateServiceCollection(_configuration, _loggerProvider);
			await using ServiceProvider serviceProvider = serviceCollection.BuildServiceProvider();

			ConfigService configService = serviceProvider.GetRequiredService<ConfigService>();
			GlobalConfig globalConfig = await configService.WaitForInitialConfigAsync();

			PerforceReplicator replicator = serviceProvider.GetRequiredService<PerforceReplicator>();
			IStreamCollection streamCollection = serviceProvider.GetRequiredService<IStreamCollection>();

			StreamConfig? streamConfig;
			if (!globalConfig.TryGetStream(new StreamId(StreamId), out streamConfig))
			{
				throw new FatalErrorException($"Stream '{StreamId}' not found");
			}

			ReplicatorId id = new ReplicatorId(new StreamId(StreamId), new StreamReplicatorId(ReplicatorId));

			PerforceReplicationOptions options = new PerforceReplicationOptions();
			options.Clean = Clean;
			await replicator.WriteAsync(id, streamConfig, Change, options, default);

			return 0;
		}
	}
}
