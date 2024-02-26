// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using HordeCommon;
using Microsoft.Extensions.Diagnostics.HealthChecks;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Server;

/// <summary>
/// Allows reporting the health of a subsystem
/// </summary>
public interface IHealthMonitor
{
	/// <summary>
	/// Sets the name used for reporting status of this service
	/// </summary>
	void SetName(string name);

	/// <summary>
	/// Updates the current health of a system
	/// </summary>
	void Update(HealthStatus result, string? message = null, DateTimeOffset? timestamp = null);
}

/// <summary>
/// Typed implementation of <see cref="IHealthMonitor"/>
/// </summary>
/// <typeparam name="T">Type of the subsystem</typeparam>
public interface IHealthMonitor<T> : IHealthMonitor
{
}
	
class HealthMonitor<T> : IHealthMonitor<T>
{
	readonly ServerStatusService _statusService;

	public HealthMonitor(ServerStatusService statusService)
		: this(statusService, typeof(T).Name)
	{
	}

	public HealthMonitor(ServerStatusService statusService, string name)
	{
		_statusService = statusService;
		_statusService.Register(typeof(T), name);
	}

	public void SetName(string name)
	{
		_statusService.Register(typeof(T), name);
	}

	public void Update(HealthStatus result, string? message, DateTimeOffset? timestamp)
	{
		_statusService.Report(typeof(T), result, message, timestamp);
	}
}

/// <summary>
/// Represents the latest status of a subsystem inside Horde
/// </summary>
/// <param name="Type"></param>
/// <param name="Name"></param>
/// <param name="Updates"></param>
public record SubsystemStatus(Type Type, string Name, List<SubsystemStatusUpdate> Updates)
{
	internal SubsystemStatus Copy()
	{
		return this with { Updates = [..Updates] };
	}

	/// <inheritdoc/>
	public override string ToString()
	{
		string updates = Updates.Count > 0 ? Updates.First().ToString() : "<no updates>";
		return $"Subsystem({Type.Name} LastUpdate: {updates})";
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
	
	private readonly IClock _clock;
	private readonly MongoService _mongoService;
	private readonly RedisService _redisService;
	private readonly object _lock = new();
	private readonly Dictionary<Type, SubsystemStatus> _subsystemStatuses = new();

	private readonly IHealthMonitor<MongoService> _mongoDbHealth;
	private readonly ITicker _mongoDbHealthTicker;

	private readonly IHealthMonitor<RedisService> _redisHealth;
	private readonly ITicker _redisHealthTicker;

	/// <summary>
	/// Constructor
	/// </summary>
	/// <param name="redisService"></param>
	/// <param name="clock"></param>
	/// <param name="mongoService"></param>
	/// <param name="logger"></param>
	public ServerStatusService(MongoService mongoService, RedisService redisService, IClock clock, ILogger<ServerStatusService> logger)
	{
		_mongoService = mongoService;
		_redisService = redisService;
		_clock = clock;

		_mongoDbHealth = new HealthMonitor<MongoService>(this, "MongoDB");
		_mongoDbHealthTicker = clock.AddTicker($"{nameof(ServerStatusService)}.MongoDb", TimeSpan.FromSeconds(30.0), UpdateMongoDbHealthAsync, logger);

		_redisHealth = new HealthMonitor<RedisService>(this, "Redis");
		_redisHealthTicker = clock.AddTicker($"{nameof(ServerStatusService)}.Redis", TimeSpan.FromSeconds(30.0), UpdateRedisHealthAsync, logger);
	}

	/// <inheritdoc/>
	public async Task StartAsync(CancellationToken cancellationToken)
	{
		await _mongoDbHealthTicker.StartAsync();
		await _redisHealthTicker.StartAsync();
	}

	/// <inheritdoc/>
	public async Task StopAsync(CancellationToken cancellationToken)
	{
		await _mongoDbHealthTicker.StopAsync();
		await _redisHealthTicker.StopAsync();
	}

	/// <summary>
	/// Checks health and connectivity to MongoDB database
	/// </summary>
	internal async ValueTask UpdateMongoDbHealthAsync(CancellationToken cancellationToken)
	{
		HealthCheckResult result = await _mongoService.CheckHealthAsync(new HealthCheckContext(), cancellationToken);
		_mongoDbHealth.Update(result.Status, result.Description);
	}

	/// <summary>
	/// Checks health and connectivity to Redis database
	/// </summary>
	internal async ValueTask UpdateRedisHealthAsync(CancellationToken cancellationToken)
	{
		HealthCheckResult result = await _redisService.CheckHealthAsync(new HealthCheckContext(), cancellationToken);
		_redisHealth.Update(result.Status, result.Description);
	}

	/// <summary>
	/// Register a new type for status reporting
	/// </summary>
	public void Register(Type type, string name)
	{
		lock (_lock)
		{
			SubsystemStatus? status;
			if (_subsystemStatuses.TryGetValue(type, out status))
			{
				_subsystemStatuses[type] = status with { Name = name };
			}
			else
			{
				_subsystemStatuses[type] = new SubsystemStatus(type, name, []);
			}
		}
	}

	/// <summary>
	/// Report a status update for a given subsystem
	/// </summary>
	/// <param name="type">Service type reporting health</param>
	/// <param name="result">Result of the update</param>
	/// <param name="message">Human-readable message</param>
	/// <param name="timestamp">Optional timestamp to be associated with the report. Defaults to UtcNow</param>
	public void Report(Type type, HealthStatus result, string? message = null, DateTimeOffset? timestamp = null)
	{
		SubsystemStatusUpdate update = new (result, message, timestamp ?? _clock.UtcNow);
		lock (_lock)
		{
			SubsystemStatus status = _subsystemStatuses[type];
			status.Updates.Add(update);
			status.Updates.Sort((a, b) => b.UpdatedAt.CompareTo(a.UpdatedAt));
			if (status.Updates.Count > MaxHistoryLength)
			{
				status.Updates.RemoveRange(MaxHistoryLength, status.Updates.Count - MaxHistoryLength);
			}
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
