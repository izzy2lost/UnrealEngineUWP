// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Horde.Server.Configuration;
using HordeCommon;
using Microsoft.Extensions.Diagnostics.HealthChecks;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Server;

/// <summary>
/// A specific subsystem inside Horde
/// </summary>
public enum Subsystem
{
	/// <summary>
	/// Global config
	/// </summary>
	GlobalConfig,
	
	/// <summary>
	/// MongoDB (database)
	/// </summary>
	MongoDb,
	
	/// <summary>
	/// Redis (database/cache)
	/// </summary>
	Redis,
	
	/// <summary>
	/// Perforce (version control)
	/// </summary>
	Perforce,
}

/// <summary>
/// Represents the latest status of a subsystem inside Horde
/// </summary>
/// <param name="Category"></param>
/// <param name="Name"></param>
/// <param name="Updates"></param>
public record SubsystemStatus(string Category, string Name, List<SubsystemStatusUpdate> Updates)
{
	internal SubsystemStatus Copy()
	{
		return this with { Updates = [..Updates] };
	}

	/// <inheritdoc/>
	public override string ToString()
	{
		string updates = Updates.Count > 0 ? Updates.First().ToString() : "<no updates>";
		return $"Subsystem({Category} {Name} LastUpdate: {updates})";
	}
}

/// <summary>
/// An individual status update for a subsystem
/// </summary>
/// <param name="Result"></param>
/// <param name="Message"></param>
/// <param name="UpdatedAt"></param>
public record SubsystemStatusUpdate(HealthStatus Result, string? Message, DateTimeOffset UpdatedAt);
	
/// <summary>
/// Tracks health and status of the Horde server itself
/// Such as connectivity to external systems (MongoDB, Redis, Perforce etc).
/// </summary>
public class ServerStatusService : IHostedService
{
	/// <summary>
	/// Max historical status updates to keep
	/// </summary>
	public const int MaxHistoryLength = 10;
	
	private const string CategoryDefault = "default";
	private readonly IClock _clock;
	private readonly ConfigService _configService;
	private readonly MongoService _mongoService;
	private readonly RedisService _redisService;
	private readonly object _lock = new();
	private readonly Dictionary<(string category, string name), SubsystemStatus> _subsystemStatuses = new();
	private readonly ITicker _mongoDbHealthTicker;
	private readonly ITicker _redisHealthTicker;
	private readonly ITicker _perforceHealthTicker;

	/// <summary>
	/// Constructor
	/// </summary>
	/// <param name="configService"></param>
	/// <param name="redisService"></param>
	/// <param name="clock"></param>
	/// <param name="mongoService"></param>
	/// <param name="logger"></param>
	public ServerStatusService(ConfigService configService, MongoService mongoService, RedisService redisService, IClock clock, ILogger<ServerStatusService> logger)
	{
		_configService = configService;
		_mongoService = mongoService;
		_redisService = redisService;
		_clock = clock;
		_configService.OnConfigUpdate += exception =>
		{
			if (exception != null)
			{
				Report(Subsystem.GlobalConfig, HealthStatus.Unhealthy, exception.Message);
			}
			else
			{
				Report(Subsystem.GlobalConfig, HealthStatus.Healthy);
			}
		};
		
		_mongoDbHealthTicker = clock.AddTicker($"{nameof(ServerStatusService)}.{nameof(Subsystem.MongoDb)}", TimeSpan.FromSeconds(30.0), UpdateMongoDbHealthAsync, logger);
		_redisHealthTicker = clock.AddTicker($"{nameof(ServerStatusService)}.{nameof(Subsystem.Redis)}", TimeSpan.FromSeconds(30.0), UpdateRedisHealthAsync, logger);
		_perforceHealthTicker = clock.AddTicker($"{nameof(ServerStatusService)}.{nameof(Subsystem.Perforce)}", TimeSpan.FromSeconds(30.0), UpdatePerforceHealthAsync, logger);
		
		foreach (Subsystem s in Enum.GetValues<Subsystem>())
		{
			_subsystemStatuses[(CategoryDefault, s.ToString())] = new SubsystemStatus(CategoryDefault, s.ToString(), []);
		}
	}

	/// <inheritdoc/>
	public async Task StartAsync(CancellationToken cancellationToken)
	{
		await _mongoDbHealthTicker.StartAsync();
		await _redisHealthTicker.StartAsync();
		await _perforceHealthTicker.StartAsync();
	}

	/// <inheritdoc/>
	public async Task StopAsync(CancellationToken cancellationToken)
	{
		await _mongoDbHealthTicker.StopAsync();
		await _redisHealthTicker.StopAsync();
		await _perforceHealthTicker.StopAsync();
	}

	/// <summary>
	/// Checks health and connectivity to MongoDB database
	/// </summary>
	/// <param name="cancellationToken">Cancellation token for the async task</param>
	internal async ValueTask UpdateMongoDbHealthAsync(CancellationToken cancellationToken)
	{
		HealthCheckResult result = await _mongoService.CheckHealthAsync(new HealthCheckContext(), cancellationToken);
		Report(Subsystem.MongoDb, result.Status, result.Description);
	}

	/// <summary>
	/// Checks health and connectivity to Redis database
	/// </summary>
	/// <param name="cancellationToken">Cancellation token for the async task</param>
	internal async ValueTask UpdateRedisHealthAsync(CancellationToken cancellationToken)
	{
		HealthCheckResult result = await _redisService.CheckHealthAsync(new HealthCheckContext(), cancellationToken);
		Report(Subsystem.Redis, result.Status, result.Description);
	}

	/// <summary>
	/// Checks health and connectivity to Perforce servers
	/// </summary>
	/// <param name="cancellationToken">Cancellation token for the async task</param>
	internal ValueTask UpdatePerforceHealthAsync(CancellationToken cancellationToken)
	{
		return ValueTask.CompletedTask;
	}

	/// <summary>
	/// Report a status update for a given subsystem
	/// </summary>
	/// <param name="subsystem">Which subsystem to report for</param>
	/// <param name="result">Result of the update</param>
	/// <param name="message">Human-readable message</param>
	/// <param name="timestamp">Optional timestamp to be associated with the report. Defaults to UtcNow</param>
	public void Report(Subsystem subsystem, HealthStatus result, string? message = null, DateTimeOffset? timestamp = null)
	{
		ReportInternal(CategoryDefault, subsystem.ToString(), result, message, timestamp);
	}

	private void ReportInternal(string category, string name, HealthStatus result, string? message, DateTimeOffset? timestamp)
	{
		(string category, string name) id = (category, name);
		SubsystemStatusUpdate update = new (result, message, timestamp ?? _clock.UtcNow);
		lock (_lock)
		{
			if (!_subsystemStatuses.TryGetValue(id, out SubsystemStatus? status))
			{
				status = new SubsystemStatus(category, name, []);
				_subsystemStatuses[id] = status;
			}
			
			status.Updates.Add(update);
			status.Updates.Sort((a, b) => b.UpdatedAt.CompareTo(a.UpdatedAt));
			_subsystemStatuses[id] = new SubsystemStatus(category, name, status.Updates.Take(MaxHistoryLength).ToList());
		}
	}

	/// <summary>
	/// Get a list of status and updates for each subsystem
	/// </summary>
	/// <returns>A list of statuses</returns>
	public IReadOnlyList<SubsystemStatus> GetSubsystemStatuses()
	{
		lock (_lock)
		{
			return _subsystemStatuses.Values.Select(status => status.Copy()).ToList();
		}
	}
}
