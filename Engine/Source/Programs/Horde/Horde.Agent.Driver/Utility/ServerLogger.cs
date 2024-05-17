// Copyright Epic Games, Inc. All Rights Reserved.

using System.Text;
using System.Threading.Channels;
using EpicGames.Core;
using EpicGames.Horde;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Logs;
using EpicGames.Horde.Storage;
using Google.Protobuf;
using Grpc.Core;
using Horde.Common.Rpc;
using HordeCommon.Rpc;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Utility
{
	using static Horde.Common.Rpc.LogRpc;
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
		const int FlushLength = 1024 * 1024;

		readonly IHordeClient _hordeClient;
		readonly LogId _logId;
		readonly LogBuilder _builder;
		readonly IStorageClient _store;
		readonly IBlobWriter _writer;

		int _bufferLength;

		// Tailing task
		readonly Task _tailTask;
		AsyncEvent _tailTaskStop;
		readonly AsyncEvent _newTailDataEvent = new AsyncEvent();

		readonly bool _warnings;
		readonly LogLevel _outputLevel;
		readonly ILogger _localLogger;
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
			_hordeClient = hordeClient;
			_logId = logId;
			_builder = new LogBuilder(LogFormat.Json, agentLogger);
			_store = _hordeClient.CreateStorageClient(logId);
			_writer = _store.CreateBlobWriter();

			_tailTaskStop = new AsyncEvent();
			_tailTask = Task.Run(() => TickTailAsync());

			_logId = logId;
			_warnings = warnings ?? true;
			_outputLevel = outputLevel;
			_localLogger = localLogger;
			_agentLogger = agentLogger;
			_dataChannel = Channel.CreateUnbounded<JsonLogEvent>();
			_dataWriter = Task.Run(() => RunDataWriterAsync());
		}

		/// <inheritdoc/>
		public void Log<TState>(LogLevel logLevel, EventId eventId, TState state, Exception? exception, Func<TState, Exception?, string> formatter)
		{
			_localLogger.Log(logLevel, eventId, state, exception, formatter);

			// Downgrade warnings to information if not required
			if (logLevel == LogLevel.Warning && !_warnings)
			{
				logLevel = LogLevel.Information;
			}

			JsonLogEvent jsonLogEvent = JsonLogEvent.FromLoggerState(logLevel, eventId, state, exception, formatter);
			WriteFormattedEvent(jsonLogEvent);
		}

		/// <inheritdoc/>
		public bool IsEnabled(LogLevel logLevel) => logLevel >= _outputLevel || _localLogger.IsEnabled(logLevel);

		/// <inheritdoc/>
		public IDisposable? BeginScope<TState>(TState state) where TState : notnull => _localLogger.BeginScope(state);

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

			_agentLogger.LogInformation("Disposing json log task");

			if (_tailTaskStop != null)
			{
				_tailTaskStop.Latch();
				_newTailDataEvent.Latch();

				await _tailTask;
				_tailTaskStop = null!;
			}

			if (_writer != null)
			{
				await _writer.DisposeAsync();
			}

			if (_store != null)
			{
				_store.Dispose();
			}
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
						await WriteOutputAsync(new RpcWriteOutputRequest(_logId, packetOffset, packetLineIndex, UnsafeByteOperations.UnsafeWrap(packet), false), CancellationToken.None);
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
						await WriteEventsAsync(events, CancellationToken.None);
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
						await WriteOutputAsync(new RpcWriteOutputRequest(_logId, packetOffset, packetLineIndex, ByteString.Empty, true), CancellationToken.None);
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

		async Task TickTailAsync()
		{
			for (; ; )
			{
				try
				{
					await TickTailInternalAsync();
					break;
				}
				catch (OperationCanceledException ex)
				{
					_agentLogger.LogInformation(ex, "Cancelled log tailing task");
					break;
				}
				catch (Exception ex)
				{
					_agentLogger.LogError(ex, "Exception on log tailing task ({LogId}): {Message}", _logId, ex.Message);
					await Task.Delay(TimeSpan.FromSeconds(10.0));
				}
			}
		}

		async Task TickTailInternalAsync()
		{
			int tailNext = -1;
			Task tickTask = Task.CompletedTask;
			while (!_tailTaskStop.IsSet())
			{
				Task newTailDataTask = _newTailDataEvent.Task;
				int initialTailNext = tailNext;

				// Get the data to send to the server
				ReadOnlyMemory<byte> tailData = ReadOnlyMemory<byte>.Empty;
				if (tailNext != -1)
				{
					(tailNext, tailData) = _builder.ReadTailData(tailNext, 16 * 1024);
				}

				// If we don't have any updates for the server, wait until we do. We need to ensure
				// we keep pumping the RPC with the server in case the requested tail next value changes,
				// and to make sure that we don't expire the existing tail data.
				if (tailNext != -1 && tailData.IsEmpty && tailNext == initialTailNext && !tickTask.IsCompleted)
				{
					_agentLogger.LogInformation("No tail data available for log {LogId} after line {TailNext}; waiting for more...", _logId, tailNext);
					await Task.WhenAny(newTailDataTask, tickTask);
					continue;
				}

				string start = "";
				if (tailData.Length > 0)
				{
					start = Encoding.UTF8.GetString(tailData.Slice(0, Math.Min(tailData.Length, 256)).Span);
				}

				// Update the next tailing position
				int numLines = CountLines(tailData.Span);
				_agentLogger.LogInformation("Setting log {LogId} tail = {TailNext}, data = {TailDataSize} bytes, {NumLines} lines ('{Start}')", _logId, tailNext, tailData.Length, numLines, start);

				int newTailNext = await UpdateLogTailAsync(tailNext, tailData, CancellationToken.None);
				_agentLogger.LogInformation("Log {LogId} tail next = {TailNext}", _logId, newTailNext);

				if (newTailNext != tailNext)
				{
					tailNext = newTailNext;
					_agentLogger.LogInformation("Modified tail position for log {LogId} to {TailNext}", _logId, tailNext);
				}

				tickTask = Task.Delay(TimeSpan.FromSeconds(10.0));
			}
			_agentLogger.LogInformation("Finishing log tail task");
		}

		static int CountLines(ReadOnlySpan<byte> data)
		{
			int lines = 0;
			for (int idx = 0; idx < data.Length; idx++)
			{
				if (data[idx] == '\n')
				{
					lines++;
				}
			}
			return lines;
		}

		/// <inheritdoc/>
		public async Task WriteEventsAsync(List<RpcCreateEventRequest> events, CancellationToken cancellationToken)
		{
			JobRpc.JobRpcClient jobRpc = await _hordeClient.CreateGrpcClientAsync<JobRpc.JobRpcClient>(cancellationToken);
			await jobRpc.CreateEventsAsync(new RpcCreateEventsRequest(events), cancellationToken: cancellationToken);
		}

		/// <inheritdoc/>
		public async Task WriteOutputAsync(RpcWriteOutputRequest request, CancellationToken cancellationToken)
		{
			_builder.WriteData(request.Data.Memory);
			_bufferLength += request.Data.Length;

			if (request.Flush || _bufferLength > FlushLength)
			{
				IBlobRef<LogNode> target = await _builder.FlushAsync(_writer, request.Flush, cancellationToken);
				await UpdateLogAsync(target, _builder.LineCount, request.Flush, cancellationToken);
				_bufferLength = 0;
			}

			_newTailDataEvent.Set();
		}

		#region RPC calls

		async Task UpdateLogAsync(IBlobRef target, int lineCount, bool complete, CancellationToken cancellationToken)
		{
			_agentLogger.LogInformation("Updating log {LogId} to line {LineCount}, target {Locator}", _logId, lineCount, target.GetLocator());

			UpdateLogRequest request = new UpdateLogRequest();
			request.LogId = _logId.ToString();
			request.LineCount = lineCount;
			request.TargetHash = target.Hash.ToString();
			request.TargetLocator = target.GetLocator().ToString();
			request.Complete = complete;

			LogRpcClient clientRef = await _hordeClient.CreateGrpcClientAsync<LogRpcClient>(cancellationToken);
			await clientRef.UpdateLogAsync(request, cancellationToken: cancellationToken);
		}

		async Task<int> UpdateLogTailAsync(int tailNext, ReadOnlyMemory<byte> tailData, CancellationToken cancellationToken)
		{
			DateTime deadline = DateTime.UtcNow.AddMinutes(2.0);
			try
			{
				LogRpcClient clientRef = await _hordeClient.CreateGrpcClientAsync<LogRpcClient>(cancellationToken);
				using AsyncDuplexStreamingCall<UpdateLogTailRequest, UpdateLogTailResponse> call = clientRef.UpdateLogTail(deadline: deadline, cancellationToken: cancellationToken);

				// Write the request to the server
				UpdateLogTailRequest request = new UpdateLogTailRequest();
				request.LogId = _logId.ToString();
				request.TailNext = tailNext;
				request.TailData = UnsafeByteOperations.UnsafeWrap(tailData);
				await call.RequestStream.WriteAsync(request, cancellationToken);
				_agentLogger.LogInformation("Writing log data: {LogId}, {TailNext}, {TailData} bytes", _logId, tailNext, tailData.Length);

				// Wait until the server responds or we need to trigger a new update
				Task<bool> moveNextAsync = call.ResponseStream.MoveNext();

				Task task = await Task.WhenAny(moveNextAsync, _tailTaskStop.Task, Task.Delay(TimeSpan.FromMinutes(1.0), CancellationToken.None));
				if (task == _tailTaskStop.Task)
				{
					_agentLogger.LogInformation("Cancelling long poll from client side (complete)");
				}

				// Close the request stream to indicate that we're finished
				await call.RequestStream.CompleteAsync();

				// Wait for a response or a new update to come in, then close the request stream
				UpdateLogTailResponse? response = null;
				while (await moveNextAsync)
				{
					response = call.ResponseStream.Current;
					moveNextAsync = call.ResponseStream.MoveNext();
				}
				return response?.TailNext ?? -1;
			}
			catch (RpcException ex) when (ex.StatusCode == StatusCode.DeadlineExceeded)
			{
				_agentLogger.LogDebug(ex, "Log tail deadline exceeded, ignoring.");
				return -1;
			}
		}

		#endregion
	}
}
