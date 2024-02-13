// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using Horde.Server.Configuration;
using HordeCommon;

namespace Horde.Server.Server;

/// <summary>
/// Status update result for a subsystem
/// </summary>
public enum SubsystemStatusResult
{
	/// <summary>
	/// Success
	/// </summary>
	Ok = 0,
	
	/// <summary>
	/// Error or problem
	/// </summary>
	Error = 1,
}

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
/// <param name="LastResult"></param>
/// <param name="LastMessage"></param>
/// <param name="LastUpdatedAt"></param>
/// <param name="Updates"></param>
public record SubsystemStatus(
	string Category,
	string Name,
	SubsystemStatusResult LastResult,
	string? LastMessage,
	DateTimeOffset LastUpdatedAt,
	LinkedList<SubsystemStatusUpdate> Updates)
{
	internal SubsystemStatus Copy()
	{
		return this with { Updates = new LinkedList<SubsystemStatusUpdate>(Updates) };
	}
}

/// <summary>
/// An individual status update for a subsystem
/// </summary>
/// <param name="Result"></param>
/// <param name="Message"></param>
/// <param name="UpdatedAt"></param>
public record SubsystemStatusUpdate(SubsystemStatusResult Result, string? Message, DateTimeOffset UpdatedAt);
	
/// <summary>
/// Tracks health and status of the Horde server itself
/// Such as connectivity to external systems (MongoDB, Redis, Perforce etc).
/// </summary>
public class ServerStatusService
{
	/// <summary>
	/// Max historical status updates to keep
	/// </summary>
	private const int MaxHistoryLength = 10;
	
	private readonly IClock _clock;
	private readonly ConfigService _configService;
	private readonly object _lock = new();
	private readonly Dictionary<string, SubsystemStatus> _subsystemStatuses = new();

	/// <summary>
	/// Constructor
	/// </summary>
	/// <param name="configService"></param>
	/// <param name="clock"></param>
	public ServerStatusService(ConfigService configService, IClock clock)
	{
		_configService = configService;
		_clock = clock;

		_configService.OnConfigUpdate += exception =>
		{
			if (exception != null)
			{
				Report(Subsystem.GlobalConfig, SubsystemStatusResult.Error, exception.Message);
			}
			else
			{
				Report(Subsystem.GlobalConfig, SubsystemStatusResult.Ok);
			}
		};
	}

	/// <summary>
	/// Report a status update for a given subsystem
	/// </summary>
	/// <param name="subsystem">Which subsystem to report for</param>
	/// <param name="result">Result of the update</param>
	/// <param name="message">Human-readable message</param>
	/// <param name="timestamp">Optional timestamp to be associated with the report. Defaults to UtcNow</param>
	public void Report(Subsystem subsystem, SubsystemStatusResult result, string? message = null, DateTimeOffset? timestamp = null)
	{
		ReportInternal("default", subsystem.ToString(), result, message, timestamp);
	}

	private void ReportInternal(string category, string name, SubsystemStatusResult result, string? message, DateTimeOffset? timestamp)
	{
		if (category.Contains(':', StringComparison.OrdinalIgnoreCase) || name.Contains(':', StringComparison.OrdinalIgnoreCase))
		{
			throw new ArgumentException("Category or name cannot contain ':' char");
		}
		
		string id = $"{category}:{name}";
		SubsystemStatusUpdate update = new (result, message, timestamp ?? _clock.UtcNow);
		lock (_lock)
		{
			if (!_subsystemStatuses.TryGetValue(id, out SubsystemStatus? status))
			{
				LinkedList<SubsystemStatusUpdate> updates = new();
				status = new SubsystemStatus(category, name, update.Result, update.Message, update.UpdatedAt, updates);
				_subsystemStatuses[id] = status;
			}
			
			status.Updates.AddFirst(update);
			if (status.Updates.Count > MaxHistoryLength)
			{
				status.Updates.RemoveLast();
			}
			
			_subsystemStatuses[id] = new SubsystemStatus(category, name, update.Result, update.Message, update.UpdatedAt, status.Updates);
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
