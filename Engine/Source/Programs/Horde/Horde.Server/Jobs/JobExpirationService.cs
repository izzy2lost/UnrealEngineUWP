// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Server.Server;
using Horde.Server.Streams;
using HordeCommon;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Server.Jobs
{
	/// <summary>
	/// Expires jobs a certain amount of time after they have run
	/// </summary>
	public sealed class JobExpirationService : IHostedService, IAsyncDisposable
	{
		readonly IJobCollection _jobCollection;
		readonly IClock _clock;
		readonly IOptionsMonitor<GlobalConfig> _globalConfig;
		readonly ITicker _ticker;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public JobExpirationService(IJobCollection jobCollection, IClock clock, IOptionsMonitor<GlobalConfig> globalConfig, ILogger<JobExpirationService> logger)
		{
			_clock = clock;
			_globalConfig = globalConfig;
			_jobCollection = jobCollection;
			_ticker = clock.AddSharedTicker<JobExpirationService>(TimeSpan.FromHours(1.0), TickAsync, logger);
			_logger = logger;
		}

		/// <inheritdoc/>
		public ValueTask DisposeAsync() => _ticker.DisposeAsync();

		/// <inheritdoc/>
		public Task StartAsync(CancellationToken cancellationToken) => _ticker.StartAsync();

		/// <inheritdoc/>
		public Task StopAsync(CancellationToken cancellationToken) => _ticker.StopAsync();

		async ValueTask TickAsync(CancellationToken cancellationToken)
		{
			DateTime currentTime = _clock.UtcNow;

			GlobalConfig globalConfig = _globalConfig.CurrentValue;
			foreach (StreamConfig streamConfig in globalConfig.Streams)
			{
				int expireAfterDays = streamConfig.JobOptions.ExpireAfterDays ?? 0;
				if (expireAfterDays != 0)
				{
					DateTimeOffset maxCreateTime = new DateTimeOffset(currentTime) - TimeSpan.FromDays(expireAfterDays);
					for (; ; )
					{
						IReadOnlyList<IJob> jobs = await _jobCollection.FindAsync(streamId: streamConfig.Id, maxCreateTime: maxCreateTime, count: 50, cancellationToken: cancellationToken);
						if (jobs.Count == 0)
						{
							break;
						}

						foreach (IJob job in jobs)
						{
							if (await job.TryDeleteAsync(cancellationToken))
							{
								int age = (int)(currentTime - job.CreateTimeUtc).TotalDays;
								_logger.LogDebug("Deleted job {JobId} ({NumDays}d >= {ExpireDays}d)", job.Id, age, expireAfterDays);
							}
						}
					}
				}
			}
		}
	}
}
