// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Security.Claims;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Agents.Sessions;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Logs;
using Horde.Server.Acls;
using Horde.Server.Storage;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using MongoDB.Bson;
using OpenTelemetry.Trace;

namespace Horde.Server.Logs
{
	/// <summary>
	/// Interface for the log file service
	/// </summary>
	public interface ILogService
	{
		/// <summary>
		/// Creates a new log
		/// </summary>
		/// <param name="jobId">Unique id of the job that owns this log file</param>
		/// <param name="leaseId">Lease allowed to update the log</param>
		/// <param name="sessionId">Agent session allowed to update the log</param>
		/// <param name="type">Type of events to be stored in the log</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <param name="logId">ID of the log file (optional)</param>
		/// <returns>The new log file document</returns>
		Task<ILog> CreateLogAsync(JobId jobId, LeaseId? leaseId, SessionId? sessionId, LogType type, LogId? logId = null, CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets a log by ID
		/// </summary>
		/// <param name="logId">Unique id of the log file</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>The log  document</returns>
		Task<ILog?> GetLogAsync(LogId logId, CancellationToken cancellationToken);

		/// <summary>
		/// Creates new log events
		/// </summary>
		/// <param name="newEvents">List of events</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Async task</returns>
		Task CreateEventsAsync(List<NewLogEventData> newEvents, CancellationToken cancellationToken);

		/// <summary>
		/// Find events for a particular log file
		/// </summary>
		/// <param name="log">The log file instance</param>
		/// <param name="spanId">Issue span to return events for</param>
		/// <param name="index">Index of the first event to retrieve</param>
		/// <param name="count">Number of events to retrieve</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>List of log events</returns>
		Task<List<ILogEvent>> FindEventsAsync(ILog log, ObjectId? spanId = null, int? index = null, int? count = null, CancellationToken cancellationToken = default);

		/// <summary>
		/// Adds events to a log span
		/// </summary>
		/// <param name="events">The events to add</param>
		/// <param name="spanId">The span id</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Async task</returns>
		Task AddSpanToEventsAsync(IEnumerable<ILogEvent> events, ObjectId spanId, CancellationToken cancellationToken = default);

		/// <summary>
		/// Find events for an issue
		/// </summary>
		/// <param name="spanIds">The span ids</param>
		/// <param name="logIds">Log ids to include</param>
		/// <param name="index">Index within the events for results to return</param>
		/// <param name="count">Number of results to return</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Async task</returns>
		Task<List<ILogEvent>> FindEventsForSpansAsync(IEnumerable<ObjectId> spanIds, LogId[]? logIds, int index, int count, CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets the data for an event
		/// </summary>
		/// <param name="log">The log file instance</param>
		/// <param name="lineIndex">Index of the line in the file</param>
		/// <param name="lineCount">Number of lines in the event</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>New event data instance</returns>
		Task<ILogEventData> GetEventDataAsync(ILog log, int lineIndex, int lineCount, CancellationToken cancellationToken = default);
	}

	/// <summary>
	/// Wraps functionality for manipulating logs
	/// </summary>
	public sealed class LogService : ILogService, IDisposable
	{
		private readonly Tracer _tracer;
		private readonly ILogger<LogService> _logger;
		private readonly ILogCollection _logCollection;
		private readonly ILogEventCollection _logEventCollection;
		private readonly ILogStorage _storage;
		private readonly StorageService _storageService;
		private readonly IOptions<ServerSettings> _settings;

		readonly LogTailService _logTailService;

		/// <summary>
		/// Constructor
		/// </summary>
		public LogService(ILogCollection logCollection, ILogEventCollection logEventCollection, ILogStorage storage, LogTailService logTailService, StorageService storageService, IOptions<ServerSettings> settings, Tracer tracer, ILogger<LogService> logger)
		{
			_logCollection = logCollection;
			_logEventCollection = logEventCollection;
			_storage = storage;
			_logTailService = logTailService;
			_storageService = storageService;
			_settings = settings;
			_tracer = tracer;
			_logger = logger;
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			_storage.Dispose();
		}

		/// <inheritdoc/>
		public Task<ILog> CreateLogAsync(JobId jobId, LeaseId? leaseId, SessionId? sessionId, LogType type, LogId? logId, CancellationToken cancellationToken)
		{
			return _logCollection.AddAsync(jobId, leaseId, sessionId, type, logId, cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<ILog?> GetLogAsync(LogId logId, CancellationToken cancellationToken)
		{
			return await _logCollection.GetAsync(logId, cancellationToken);
		}

		/// <inheritdoc/>
		public Task CreateEventsAsync(List<NewLogEventData> newEvents, CancellationToken cancellationToken)
		{
			return _logEventCollection.AddManyAsync(newEvents);
		}

		/// <inheritdoc/>
		public Task<List<ILogEvent>> FindEventsAsync(ILog log, ObjectId? spanId = null, int? index = null, int? count = null, CancellationToken cancellationToken = default)
		{
			return _logEventCollection.FindAsync(log.Id, spanId, index, count);
		}

		class LogEventData : ILogEventData
		{
			public string? _message;
			public IReadOnlyList<JsonLogEvent> Lines { get; }

			EventId? ILogEventData.EventId => (Lines.Count > 0) ? Lines[0].EventId : null;
			LogEventSeverity ILogEventData.Severity => (Lines.Count == 0) ? LogEventSeverity.Information : (Lines[0].Level == LogLevel.Warning) ? LogEventSeverity.Warning : LogEventSeverity.Error;

			public LogEventData(IReadOnlyList<JsonLogEvent> lines)
			{
				Lines = lines;
			}

			string ILogEventData.Message
			{
				get
				{
					_message ??= String.Join("\n", Lines.Select(x => x.GetRenderedMessage().ToString()));
					return _message;
				}
			}
		}

		/// <inheritdoc/>
		public Task AddSpanToEventsAsync(IEnumerable<ILogEvent> events, ObjectId spanId, CancellationToken cancellationToken)
		{
			return _logEventCollection.AddSpanToEventsAsync(events, spanId);
		}

		/// <inheritdoc/>
		public Task<List<ILogEvent>> FindEventsForSpansAsync(IEnumerable<ObjectId> spanIds, LogId[]? logIds, int index, int count, CancellationToken cancellationToken)
		{
			return _logEventCollection.FindEventsForSpansAsync(spanIds, logIds, index, count);
		}

		/// <inheritdoc/>
		public async Task<ILogEventData> GetEventDataAsync(ILog log, int lineIndex, int lineCount, CancellationToken cancellationToken)
		{
			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(LogService)}.{nameof(GetEventDataAsync)}");
			span.SetAttribute("logId", log.Id.ToString());
			span.SetAttribute("lineIndex", lineIndex);
			span.SetAttribute("lineCount", lineCount);

			List<Utf8String> lines = await log.ReadLinesAsync(lineIndex, lineCount, cancellationToken);
			List<JsonLogEvent> jsonLines = new List<JsonLogEvent>(lines.Count);

			foreach (Utf8String line in lines)
			{
				try
				{
					jsonLines.Add(JsonLogEvent.Parse(line.Memory));
				}
				catch (JsonException ex)
				{
					_logger.LogWarning(ex, "Unable to parse line from log file: {Line}", line);
				}
			}

			return new LogEventData(jsonLines);
		}

		/// <summary>
		/// Determines if the user is authorized to perform an action on a particular template
		/// </summary>
		/// <param name="log">The template to check</param>
		/// <param name="user">The principal to authorize</param>
		/// <returns>True if the action is authorized</returns>
		public static bool AuthorizeForSession(ILog log, ClaimsPrincipal user)
		{
			if (log.SessionId != null && user.HasSessionClaim(log.SessionId.Value))
			{
				return true;
			}
			if (log.LeaseId != null && user.HasLeaseClaim(log.LeaseId.Value))
			{
				return true;
			}
			return false;
		}
	}
}

