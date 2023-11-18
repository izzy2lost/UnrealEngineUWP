// Copyright Epic Games, Inc. All Rights Reserved.

using System.Text;
using System.Threading.Channels;
using EpicGames.Core;
using Google.Protobuf;
using HordeCommon;
using HordeCommon.Rpc;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Utility
{
	using ByteString = Google.Protobuf.ByteString;

	/// <summary>
	/// Interface for a log device
	/// </summary>
	public interface IServerLogger : ILogger, IAsyncDisposable
	{
		/// <summary>
		/// Outcome of the job step, including any warnings/errors
		/// </summary>
		JobStepOutcome Outcome { get; }

		/// <summary>
		/// Flushes the logger with the server and stops the background work
		/// </summary>
		Task StopAsync();
	}

	/// <summary>
	/// Class to handle uploading log data to the server in the background
	/// </summary>
	sealed class JsonRpcLogger : IServerLogger
	{
		internal readonly IJsonRpcLogSink Sink;
		internal readonly string LogId;
		internal readonly bool Warnings;
		internal readonly LogLevel OutputLevel;
		internal readonly ILogger Inner;
		readonly Channel<JsonLogEvent> _dataChannel;
		Task? _dataWriter;

		/// <summary>
		/// The current outcome for this step. Updated to reflect any errors and warnings that occurred.
		/// </summary>
		public JobStepOutcome Outcome
		{
			get;
			private set;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="sink">Sink for log events</param>
		/// <param name="logId">The log id to write to</param>
		/// <param name="warnings">Whether to include warnings in the output</param>
		/// <param name="outputLevel">Minimum level for output</param>
		/// <param name="inner">Additional logger to write to</param>
		public JsonRpcLogger(IJsonRpcLogSink sink, string logId, bool? warnings, LogLevel outputLevel, ILogger inner)
		{
			Sink = sink;
			LogId = logId;
			Warnings = warnings ?? true;
			OutputLevel = outputLevel;
			Inner = inner;
			_dataChannel = Channel.CreateUnbounded<JsonLogEvent>();
			_dataWriter = Task.Run(() => RunDataWriterAsync());

			Outcome = JobStepOutcome.Success;
		}

		/// <inheritdoc/>
		public void Log<TState>(LogLevel logLevel, EventId eventId, TState state, Exception? exception, Func<TState, Exception?, string> formatter)
		{
			// Downgrade warnings to information if not required
			if (logLevel == LogLevel.Warning && !Warnings)
			{
				logLevel = LogLevel.Information;
			}

			JsonLogEvent jsonLogEvent = JsonLogEvent.FromLoggerState(logLevel, eventId, state, exception, formatter);
			WriteFormattedEvent(jsonLogEvent);
		}

		/// <inheritdoc/>
		public bool IsEnabled(LogLevel logLevel) => logLevel >= OutputLevel;

		/// <inheritdoc/>
		public IDisposable? BeginScope<TState>(TState state) where TState : notnull => Inner.BeginScope(state);

		private void WriteFormattedEvent(JsonLogEvent jsonLogEvent)
		{
			// Update the state of this job if this is an error status
			LogLevel level = jsonLogEvent.Level;
			if (level == LogLevel.Error || level == LogLevel.Critical)
			{
				Outcome = JobStepOutcome.Failure;
			}
			else if (level == LogLevel.Warning && Outcome != JobStepOutcome.Failure)
			{
				Outcome = JobStepOutcome.Warnings;
			}

			// Write the event
			if (!_dataChannel.Writer.TryWrite(jsonLogEvent))
			{
				throw new InvalidOperationException("Expected unbounded writer to complete immediately");
			}
		}

		/// <summary>
		/// Stops the log writer's background task
		/// </summary>
		/// <returns>Async task</returns>
		public async Task StopAsync()
		{
			if (_dataWriter != null)
			{
				_dataChannel.Writer.TryComplete();
				await _dataWriter;
				_dataWriter = null;
			}
		}

		/// <summary>
		/// Dispose of this object. Call StopAsync() to stop asynchronously.
		/// </summary>
		public async ValueTask DisposeAsync()
		{
			await StopAsync();
			await Sink.DisposeAsync();
		}

		/// <summary>
		/// Upload the log data to the server in the background
		/// </summary>
		/// <returns>Async task</returns>
		async Task RunDataWriterAsync()
		{
			// Current position and line number in the log file
			long packetOffset = 0;
			int packetLineIndex = 0;

			// Index of the next line to write to the log
			int nextLineIndex = 0;

			// Total number of errors and warnings
			const int MaxErrors = 200;
			int numErrors = 0;
			const int MaxWarnings = 200;
			int numWarnings = 0;

			// Buffer for events read in a single iteration
			JsonRpcLogWriter writer = new JsonRpcLogWriter();
			List<CreateEventRequest> events = new List<CreateEventRequest>();

			// The current jobstep outcome
			JobStepOutcome postedOutcome = JobStepOutcome.Success;

			// Whether we've written the flush command
			for (; ; )
			{
				events.Clear();

				// Get the next data
				Task waitTask = Task.Delay(TimeSpan.FromSeconds(2.0));
				while (writer.PacketLength < writer.MaxPacketLength)
				{
					JsonLogEvent jsonLogEvent;
					if (_dataChannel.Reader.TryRead(out jsonLogEvent))
					{
						int lineCount = writer.SanitizeAndWriteEvent(jsonLogEvent);
						if (jsonLogEvent.LineIndex == 0)
						{
							if (jsonLogEvent.Level == LogLevel.Warning && ++numWarnings <= MaxWarnings)
							{
								AddEvent(jsonLogEvent.Data.Span, nextLineIndex, Math.Max(lineCount, jsonLogEvent.LineCount), EventSeverity.Warning, events);
							}
							else if ((jsonLogEvent.Level == LogLevel.Error || jsonLogEvent.Level == LogLevel.Critical) && ++numErrors <= MaxErrors)
							{
								AddEvent(jsonLogEvent.Data.Span, nextLineIndex, Math.Max(lineCount, jsonLogEvent.LineCount), EventSeverity.Error, events);
							}
						}
						nextLineIndex += lineCount;
					}
					{
						Task<bool> readTask = _dataChannel.Reader.WaitToReadAsync().AsTask();
						if (await Task.WhenAny(readTask, waitTask) == waitTask)
						{
							break;
						}
						if (!await readTask)
						{
							break;
						}
					}
				}

				// Upload it to the server
				if (writer.PacketLength > 0)
				{
					(ReadOnlyMemory<byte> packet, int packetLineCount) = writer.CreatePacket();
					try
					{
						await Sink.WriteOutputAsync(new WriteOutputRequest(LogId, packetOffset, packetLineIndex, UnsafeByteOperations.UnsafeWrap(packet), false), CancellationToken.None);
						packetOffset += packet.Length;
						packetLineIndex += packetLineCount;
					}
					catch (Exception ex)
					{
						Inner.LogWarning(ex, "Unable to write data to server (log {LogId}, offset {Offset}, length {Length}, lines {StartLine}-{EndLine})", LogId, packetOffset, packet.Length, packetLineIndex, packetLineIndex + packetLineCount);
					}
				}

				// Write all the events
				if (events.Count > 0)
				{
					try
					{
						await Sink.WriteEventsAsync(events, CancellationToken.None);
					}
					catch (Exception ex)
					{
						Inner.LogWarning(ex, "Unable to create events");
					}
				}

				// Update the outcome of this jobstep
				if (Outcome != postedOutcome)
				{
					try
					{
						await Sink.SetOutcomeAsync(Outcome, CancellationToken.None);
					}
					catch (Exception ex)
					{
						Inner.LogWarning(ex, "Unable to update step outcome to {NewOutcome}", Outcome);
					}
					postedOutcome = Outcome;
				}

				// Wait for more data to be available
				if (writer.PacketLength <= 0 && !await _dataChannel.Reader.WaitToReadAsync())
				{
					try
					{
						await Sink.WriteOutputAsync(new WriteOutputRequest(LogId, packetOffset, packetLineIndex, ByteString.Empty, true), CancellationToken.None);
					}
					catch (Exception ex)
					{
						Inner.LogWarning(ex, "Unable to flush data to server (log {LogId}, offset {Offset})", LogId, packetOffset);
					}
					break;
				}
			}
		}

		void AddEvent(ReadOnlySpan<byte> span, int lineIndex, int lineCount, EventSeverity severity, List<CreateEventRequest> events)
		{
			try
			{
				events.Add(new CreateEventRequest(severity, LogId, lineIndex, lineCount));
			}
			catch (Exception ex)
			{
				Inner.LogError(ex, "Exception while trying to parse line count from data ({Message})", Encoding.UTF8.GetString(span));
			}
		}
	}
}
