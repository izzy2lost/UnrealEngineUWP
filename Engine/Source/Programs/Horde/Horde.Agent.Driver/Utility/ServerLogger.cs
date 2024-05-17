// Copyright Epic Games, Inc. All Rights Reserved.

using System.Text;
using System.Threading.Channels;
using EpicGames.Core;
using EpicGames.Horde;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Logs;
using Google.Protobuf;
using Horde.Common.Rpc;
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
		/// Flushes the logger with the server and stops the background work
		/// </summary>
		Task StopAsync();
	}

	/// <summary>
	/// Class to handle uploading log data to the server in the background
	/// </summary>
	sealed class ServerLogger : IServerLogger
	{
		readonly JsonRpcAndStorageLogSink _sink;
		readonly LogId _logId;
		readonly bool _warnings;
		readonly LogLevel _outputLevel;
		readonly ILogger _forwardLogger;
		readonly ILogger _agentLogger;
		readonly Channel<JsonLogEvent> _dataChannel;
		Task? _dataWriter;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="hordeClient">Horde instance to write to</param>
		/// <param name="logId">The log id to write to</param>
		/// <param name="warnings">Whether to include warnings in the output</param>
		/// <param name="outputLevel">Minimum level for output</param>
		/// <param name="localLogger">Logger to forward log messages to</param>
		/// <param name="agentLogger">Logger for systemic messages</param>
		public ServerLogger(IHordeClient hordeClient, LogId logId, bool? warnings, LogLevel outputLevel, ILogger localLogger, ILogger agentLogger)
		{
			_sink = new JsonRpcAndStorageLogSink(hordeClient, logId, agentLogger); 
			_logId = logId;
			_warnings = warnings ?? true;
			_outputLevel = outputLevel;
			_forwardLogger = localLogger;
			_agentLogger = agentLogger;
			_dataChannel = Channel.CreateUnbounded<JsonLogEvent>();
			_dataWriter = Task.Run(() => RunDataWriterAsync());
		}

		/// <inheritdoc/>
		public void Log<TState>(LogLevel logLevel, EventId eventId, TState state, Exception? exception, Func<TState, Exception?, string> formatter)
		{
			_forwardLogger.Log(logLevel, eventId, state, exception, formatter);

			// Downgrade warnings to information if not required
			if (logLevel == LogLevel.Warning && !_warnings)
			{
				logLevel = LogLevel.Information;
			}

			JsonLogEvent jsonLogEvent = JsonLogEvent.FromLoggerState(logLevel, eventId, state, exception, formatter);
			WriteFormattedEvent(jsonLogEvent);
		}

		/// <inheritdoc/>
		public bool IsEnabled(LogLevel logLevel) => logLevel >= _outputLevel || _forwardLogger.IsEnabled(logLevel);

		/// <inheritdoc/>
		public IDisposable? BeginScope<TState>(TState state) where TState : notnull => _forwardLogger.BeginScope(state);

		private void WriteFormattedEvent(JsonLogEvent jsonLogEvent)
		{
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
			await _sink.DisposeAsync();
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
			List<RpcCreateEventRequest> events = new List<RpcCreateEventRequest>();

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
								AddEvent(jsonLogEvent.Data.Span, nextLineIndex, Math.Max(lineCount, jsonLogEvent.LineCount), LogEventSeverity.Warning, events);
							}
							else if ((jsonLogEvent.Level == LogLevel.Error || jsonLogEvent.Level == LogLevel.Critical) && ++numErrors <= MaxErrors)
							{
								AddEvent(jsonLogEvent.Data.Span, nextLineIndex, Math.Max(lineCount, jsonLogEvent.LineCount), LogEventSeverity.Error, events);
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
						await _sink.WriteOutputAsync(new RpcWriteOutputRequest(_logId, packetOffset, packetLineIndex, UnsafeByteOperations.UnsafeWrap(packet), false), CancellationToken.None);
						packetOffset += packet.Length;
						packetLineIndex += packetLineCount;
					}
					catch (Exception ex)
					{
						_agentLogger.LogWarning(ex, "Unable to write data to server (log {LogId}, offset {Offset}, length {Length}, lines {StartLine}-{EndLine})", _logId, packetOffset, packet.Length, packetLineIndex, packetLineIndex + packetLineCount);
					}
				}

				// Write all the events
				if (events.Count > 0)
				{
					try
					{
						await _sink.WriteEventsAsync(events, CancellationToken.None);
					}
					catch (Exception ex)
					{
						_agentLogger.LogWarning(ex, "Unable to create events");
					}
				}

				// Wait for more data to be available
				if (writer.PacketLength <= 0 && !await _dataChannel.Reader.WaitToReadAsync())
				{
					try
					{
						await _sink.WriteOutputAsync(new RpcWriteOutputRequest(_logId, packetOffset, packetLineIndex, ByteString.Empty, true), CancellationToken.None);
					}
					catch (Exception ex)
					{
						_agentLogger.LogWarning(ex, "Unable to flush data to server (log {LogId}, offset {Offset})", _logId, packetOffset);
					}
					break;
				}
			}
		}

		void AddEvent(ReadOnlySpan<byte> span, int lineIndex, int lineCount, LogEventSeverity severity, List<RpcCreateEventRequest> events)
		{
			try
			{
				events.Add(new RpcCreateEventRequest(severity, _logId, lineIndex, lineCount));
			}
			catch (Exception ex)
			{
				_agentLogger.LogError(ex, "Exception while trying to parse line count from data ({Message})", Encoding.UTF8.GetString(span));
			}
		}
	}
}
