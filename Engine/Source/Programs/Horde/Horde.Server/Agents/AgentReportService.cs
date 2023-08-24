// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Api;
using Horde.Server.Jobs;
using Horde.Server.Notifications;
using Horde.Server.Server;
using Horde.Server.Utilities;
using HordeCommon;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Server.Agents
{
	/// <summary>
	/// Report for the state of agents in the farm
	/// </summary>
	public class AgentReport
	{
		/// <summary>
		/// List of agents stuck in a conform loop, plus the number of times they've attempted to conform
		/// </summary>
		public List<(AgentId, int)> ConformLoop { get; } = new List<(AgentId, int)>();
	}

	[SingletonDocument("agent-report-state", "6268871c211d05611b3e4fd8")]
	class AgentReportState : SingletonBase
	{
		public DateTime LastUpdateUtc { get; set; }
	}

	/// <summary>
	/// Posts summaries for all the open issues in different streams to Slack channels
	/// </summary>
	public sealed class AgentReportService : IHostedService, IDisposable
	{
		readonly SingletonDocument<AgentReportState> _state;
		readonly IAgentCollection _agentCollection;
		readonly INotificationService _notificationService;
		readonly IClock _clock;
		readonly ITicker _ticker;
		readonly IOptionsMonitor<GlobalConfig> _globalConfig;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public AgentReportService(MongoService mongoService, IAgentCollection agentCollection, INotificationService notificationService, IClock clock, IOptionsMonitor<GlobalConfig> globalConfig, ILogger<AgentReportService> logger)
		{
			_state = new SingletonDocument<AgentReportState>(mongoService);
			_agentCollection = agentCollection;
			_notificationService = notificationService;
			_clock = clock;
			_ticker = clock.AddSharedTicker<AgentReportService>(TimeSpan.FromMinutes(5.0), TickAsync, logger);
			_globalConfig = globalConfig;
			_logger = logger;
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			_ticker.Dispose();
		}

		/// <inheritdoc/>
		public async Task StartAsync(CancellationToken cancellationToken)
		{
			await _ticker.StartAsync();
		}

		/// <inheritdoc/>
		public async Task StopAsync(CancellationToken cancellationToken)
		{
			await _ticker.StopAsync();
		}

		async ValueTask TickAsync(CancellationToken cancellationToken)
		{
			AgentReportState state = await _state.GetAsync();
			DateTime utcNow = _clock.UtcNow;

			TimeSpan agentReportTime = TimeSpan.FromHours(9.0); // 9am

			DateTime nextUpdateTime = _clock.TimeZone.GetStartOfDayUtc(state.LastUpdateUtc + TimeSpan.FromHours(24.0)) + agentReportTime;
			if (utcNow > nextUpdateTime)
			{
				AgentReport report = new AgentReport();

				List<IAgent> agents = await _agentCollection.FindAsync();
				foreach (IAgent agent in agents)
				{
					if (agent.ConformAttemptCount.HasValue && agent.ConformAttemptCount.Value > 3)
					{
						report.ConformLoop.Add((agent.Id, agent.ConformAttemptCount.Value));
					}
				}

				await _notificationService.SendAgentReportAsync(report);

				DateTime updateTime = _clock.TimeZone.GetStartOfDayUtc(utcNow) + agentReportTime;
				if (updateTime > utcNow)
				{
					updateTime -= TimeSpan.FromDays(1.0);
				}
				await _state.UpdateAsync(x => x.LastUpdateUtc = updateTime);
			}
		}
	}
}
