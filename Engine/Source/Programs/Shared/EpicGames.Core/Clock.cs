// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.Extensions.Logging;

namespace EpicGames.Core;

/// <summary>
/// Base interface for a scheduled event
/// </summary>
public interface ITicker : IAsyncDisposable
{
	/// <summary>
	/// Start the ticker
	/// </summary>
	Task StartAsync();

	/// <summary>
	/// Stop the ticker
	/// </summary>
	Task StopAsync();
}

/// <summary>
/// Interface representing time and scheduling events which is pluggable during testing. In normal use, the Clock implementation below is used. 
/// </summary>
public interface IClock
{
	/// <summary>
	/// Return time expressed as the Coordinated Universal Time (UTC)
	/// </summary>
	DateTime UtcNow { get; }

	/// <summary>
	/// Time zone for schedules etc...
	/// </summary>
	TimeZoneInfo TimeZone { get; }

	/// <summary>
	/// Create an event that will trigger after the given time
	/// </summary>
	/// <param name="name">Name of the event</param>
	/// <param name="interval">Time after which the event will trigger</param>
	/// <param name="tickAsync">Callback for the tick. Returns the time interval until the next tick, or null to cancel the tick.</param>
	/// <param name="logger">Logger for error messages</param>
	/// <returns>Handle to the event</returns>
	ITicker AddTicker(string name, TimeSpan interval, Func<CancellationToken, ValueTask<TimeSpan?>> tickAsync, ILogger logger);

	/// <summary>
	/// Create a ticker shared between all server processes.
	/// Callback can be run inside any available process but will still only be called once per tick.
	/// </summary>
	/// <param name="name">Name of the event</param>
	/// <param name="interval">Time after which the event will trigger</param>
	/// <param name="tickAsync">Callback for the tick. Returns the time interval until the next tick, or null to cancel the tick.</param>
	/// <param name="logger">Logger for error messages</param>
	/// <returns>New ticker instance</returns>
	ITicker AddSharedTicker(string name, TimeSpan interval, Func<CancellationToken, ValueTask> tickAsync, ILogger logger);
}

/// <summary>
/// A default implementation of IClock for normal production use
/// </summary>
public class DefaultClock : IClock
{
	/// <inheritdoc/>
	public DateTime UtcNow => DateTime.UtcNow;
	
	/// <inheritdoc/>
	public TimeZoneInfo TimeZone => TimeZoneInfo.Local;
	
	/// <inheritdoc/>
	public ITicker AddTicker(string name, TimeSpan interval, Func<CancellationToken, ValueTask<TimeSpan?>> tickAsync, ILogger logger)
	{
		throw new NotImplementedException("Not available in default implementation");
	}

	/// <inheritdoc/>
	public ITicker AddSharedTicker(string name, TimeSpan interval, Func<CancellationToken, ValueTask> tickAsync, ILogger logger)
	{
		throw new NotImplementedException("Not available in default implementation");
	}
}

/// <summary>
/// A stub implementation of IClock. Intended for testing to override time.
/// </summary>
public class StubCoreClock : IClock
{
	private DateTime _utcNow = DateTime.UtcNow;

	/// <inheritdoc/>
	public DateTime UtcNow
	{
		get => _utcNow;
		set => _utcNow = value.ToUniversalTime();
	}
	
	/// <inheritdoc/>
	public TimeZoneInfo TimeZone { get; set; } = TimeZoneInfo.Local;
	
	/// <inheritdoc/>
	public ITicker AddTicker(string name, TimeSpan interval, Func<CancellationToken, ValueTask<TimeSpan?>> tickAsync, ILogger logger)
	{
		throw new NotImplementedException("Not available in stub implementation");
	}

	/// <inheritdoc/>
	public ITicker AddSharedTicker(string name, TimeSpan interval, Func<CancellationToken, ValueTask> tickAsync, ILogger logger)
	{
		throw new NotImplementedException("Not available in stub implementation");
	}
}

