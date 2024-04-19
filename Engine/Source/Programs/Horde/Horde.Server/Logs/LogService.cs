// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Security.Claims;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Agents.Sessions;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Logs;
using EpicGames.Horde.Storage;
using Horde.Server.Acls;
using Horde.Server.Logs.Data;
using Horde.Server.Storage;
using Horde.Server.Utilities;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using MongoDB.Bson;
using OpenTelemetry.Trace;

namespace Horde.Server.Logs
{
	using Stream = System.IO.Stream;

	/// <summary>
	/// Metadata about a log file
	/// </summary>
	public class LogMetadata
	{
		/// <summary>
		/// Length of the log file
		/// </summary>
		public long Length { get; set; }

		/// <summary>
		/// Number of lines in the log file
		/// </summary>
		public int MaxLineIndex { get; set; }
	}

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
		/// Read a set of lines from the given log file
		/// </summary>
		/// <param name="log">Log file to read</param>
		/// <param name="index">Index of the first line to read</param>
		/// <param name="count">Maximum number of lines to return</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of lines</returns>
		Task<List<Utf8String>> ReadLinesAsync(ILog log, int index, int count, CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets metadata about the log file
		/// </summary>
		/// <param name="log">The log file to query</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Metadata about the log file</returns>
		Task<LogMetadata> GetMetadataAsync(ILog log, CancellationToken cancellationToken);

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

		/// <summary>
		/// Gets lines from the given log 
		/// </summary>
		/// <param name="log">The log file</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Data for the requested range</returns>
		Task<Stream> OpenRawStreamAsync(ILog log, CancellationToken cancellationToken = default);

		/// <summary>
		/// Parses a stream of json text and outputs plain text
		/// </summary>
		/// <param name="log">The log file to query</param>
		/// <param name="outputStream">Output stream to receive the text data</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Async text</returns>
		Task CopyPlainTextStreamAsync(ILog log, Stream outputStream, CancellationToken cancellationToken = default);

		/// <summary>
		/// Search for the specified text in a log file
		/// </summary>
		/// <param name="log">The log file to search</param>
		/// <param name="text">Text to search for</param>
		/// <param name="firstLine">Line to start search from</param>
		/// <param name="count">Number of results to return</param>
		/// <param name="stats">Receives stats for the search</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>List of line numbers containing the given term</returns>
		Task<List<int>> SearchLogDataAsync(ILog log, string text, int firstLine, int count, SearchStats stats, CancellationToken cancellationToken);
	}

	/// <summary>
	/// Extension methods for dealing with log files
	/// </summary>
	public static class LogServiceExtensions
	{
		/// <summary>
		/// Parses a stream of json text and outputs plain text
		/// </summary>
		/// <param name="logService">The log file service</param>
		/// <param name="log">The log file to query</param>
		/// <param name="outputStream">Output stream to receive the text data</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Async text</returns>
		public static async Task CopyRawStreamAsync(this ILogService logService, ILog log, Stream outputStream, CancellationToken cancellationToken)
		{
			await using Stream stream = await logService.OpenRawStreamAsync(log, cancellationToken);
			await stream.CopyToAsync(outputStream, cancellationToken);
		}
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
		private readonly IMemoryCache _logCache;

		/// <summary>
		/// Streams log data to a caller
		/// </summary>
		class ResponseStream : Stream
		{
			/// <summary>
			/// The log file service that created this stream
			/// </summary>
			readonly LogService _logService;

			/// <summary>
			/// The log file being read
			/// </summary>
			readonly ILog _log;

			/// <summary>
			/// Starting offset within the file of the data to return 
			/// </summary>
			readonly long _responseOffset;

			/// <summary>
			/// Length of data to return
			/// </summary>
			readonly long _responseLength;

			/// <summary>
			/// Current offset within the stream
			/// </summary>
			long _currentOffset;

			/// <summary>
			/// The current chunk index
			/// </summary>
			int _chunkIdx;

			/// <summary>
			/// Buffer containing a message for missing data
			/// </summary>
			ReadOnlyMemory<byte> _sourceBuffer;

			/// <summary>
			/// Offset within the source buffer
			/// </summary>
			int _sourcePos;

			/// <summary>
			/// Length of the source buffer being copied from
			/// </summary>
			int _sourceEnd;

			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="logService">The log file service, for q</param>
			/// <param name="log"></param>
			/// <param name="offset"></param>
			/// <param name="length"></param>
			public ResponseStream(LogService logService, ILog log, long offset, long length)
			{
				_logService = logService;
				_log = log;

				_responseOffset = offset;
				_responseLength = length;

				_currentOffset = offset;

				_chunkIdx = log.Chunks.GetChunkForOffset(offset);
				_sourceBuffer = null!;
			}

			/// <inheritdoc/>
			public override bool CanRead => true;

			/// <inheritdoc/>
			public override bool CanSeek => false;

			/// <inheritdoc/>
			public override bool CanWrite => false;

			/// <inheritdoc/>
			public override long Length => _responseLength;

			/// <inheritdoc/>
			public override long Position
			{
				get => _currentOffset - _responseOffset;
				set => throw new NotImplementedException();
			}

			/// <inheritdoc/>
			public override void Flush()
			{
			}

			/// <inheritdoc/>
			public override int Read(byte[] buffer, int offset, int count)
			{
#pragma warning disable VSTHRD002
				return ReadAsync(buffer, offset, count, CancellationToken.None).Result;
#pragma warning restore VSTHRD002
			}

			/// <inheritdoc/>
			public override async Task<int> ReadAsync(byte[] buffer, int offset, int length, CancellationToken cancellationToken)
			{
				return await ReadAsync(buffer.AsMemory(offset, length), cancellationToken);
			}

			/// <inheritdoc/>
			public override async ValueTask<int> ReadAsync(Memory<byte> buffer, CancellationToken cancellationToken)
			{
				int readBytes = 0;
				while (readBytes < buffer.Length)
				{
					if (_sourcePos < _sourceEnd)
					{
						// Try to copy from the current buffer
						int blockSize = Math.Min(_sourceEnd - _sourcePos, buffer.Length - readBytes);
						_sourceBuffer.Slice(_sourcePos, blockSize).Span.CopyTo(buffer.Slice(readBytes).Span);
						_currentOffset += blockSize;
						readBytes += blockSize;
						_sourcePos += blockSize;
					}
					else if (_currentOffset < _responseOffset + _responseLength)
					{
						// Move to the right chunk
						while (_chunkIdx + 1 < _log.Chunks.Count && _currentOffset >= _log.Chunks[_chunkIdx + 1].Offset)
						{
							_chunkIdx++;
						}

						// Get the chunk data
						ILogChunk chunk = _log.Chunks[_chunkIdx];
						LogChunkData chunkData = await _logService.ReadChunkAsync(_log, _chunkIdx);

						// Figure out which sub-chunk to use
						int subChunkIdx = chunkData.GetSubChunkForOffsetWithinChunk((int)(_currentOffset - chunk.Offset));
						LogSubChunkData subChunkData = chunkData.SubChunks[subChunkIdx];

						// Get the source data
						long subChunkOffset = chunk.Offset + chunkData.SubChunkOffset[subChunkIdx];
						_sourceBuffer = subChunkData.InflateText().Data;
						_sourcePos = (int)(_currentOffset - subChunkOffset);
						_sourceEnd = (int)Math.Min(_sourceBuffer.Length, (_responseOffset + _responseLength) - subChunkOffset);
					}
					else
					{
						// End of the log
						break;
					}
				}
				return readBytes;
			}

			/// <inheritdoc/>
			public override long Seek(long offset, SeekOrigin origin) => throw new NotImplementedException();

			/// <inheritdoc/>
			public override void SetLength(long value) => throw new NotImplementedException();

			/// <inheritdoc/>
			public override void Write(byte[] buffer, int offset, int count) => throw new NotImplementedException();
		}

		/// <summary>
		/// Streams log data to a caller
		/// </summary>
		class NewLoggerResponseStream : Stream
		{
			readonly LogNode _rootNode;

			/// <summary>
			/// Starting offset within the file of the data to return 
			/// </summary>
			readonly long _responseOffset;

			/// <summary>
			/// Length of data to return
			/// </summary>
			readonly long _responseLength;

			/// <summary>
			/// Current offset within the stream
			/// </summary>
			long _currentOffset;

			/// <summary>
			/// The current chunk index
			/// </summary>
			int _chunkIdx;

			/// <summary>
			/// Buffer containing a message for missing data
			/// </summary>
			ReadOnlyMemory<byte> _sourceBuffer;

			/// <summary>
			/// Offset within the source buffer
			/// </summary>
			int _sourcePos;

			/// <summary>
			/// Length of the source buffer being copied from
			/// </summary>
			int _sourceEnd;

			/// <summary>
			/// Constructor
			/// </summary>
			public NewLoggerResponseStream(LogNode rootNode, long offset, long length)
			{
				_rootNode = rootNode;

				_responseOffset = offset;
				_responseLength = length;

				_currentOffset = offset;

				_chunkIdx = rootNode.TextChunkRefs.GetChunkForOffset(offset);
				_sourceBuffer = null!;
			}

			/// <inheritdoc/>
			public override bool CanRead => true;

			/// <inheritdoc/>
			public override bool CanSeek => false;

			/// <inheritdoc/>
			public override bool CanWrite => false;

			/// <inheritdoc/>
			public override long Length => _responseLength;

			/// <inheritdoc/>
			public override long Position
			{
				get => _currentOffset - _responseOffset;
				set => throw new NotImplementedException();
			}

			/// <inheritdoc/>
			public override void Flush()
			{
			}

			/// <inheritdoc/>
			public override int Read(byte[] buffer, int offset, int count)
			{
#pragma warning disable VSTHRD002
				return ReadAsync(buffer, offset, count, CancellationToken.None).Result;
#pragma warning restore VSTHRD002
			}

			/// <inheritdoc/>
			public override async Task<int> ReadAsync(byte[] buffer, int offset, int length, CancellationToken cancellationToken)
			{
				return await ReadAsync(buffer.AsMemory(offset, length), cancellationToken);
			}

			/// <inheritdoc/>
			public override async ValueTask<int> ReadAsync(Memory<byte> buffer, CancellationToken cancellationToken)
			{
				int readBytes = 0;
				while (readBytes < buffer.Length)
				{
					if (_sourcePos < _sourceEnd)
					{
						// Try to copy from the current buffer
						int blockSize = Math.Min(_sourceEnd - _sourcePos, buffer.Length - readBytes);
						_sourceBuffer.Slice(_sourcePos, blockSize).Span.CopyTo(buffer.Slice(readBytes).Span);
						_currentOffset += blockSize;
						readBytes += blockSize;
						_sourcePos += blockSize;
					}
					else if (_currentOffset < _responseOffset + _responseLength)
					{
						// Move to the right chunk
						while (_chunkIdx + 1 < _rootNode.TextChunkRefs.Count && _currentOffset >= _rootNode.TextChunkRefs[_chunkIdx + 1].Offset)
						{
							_chunkIdx++;
						}

						// Get the chunk data
						LogChunkRef chunk = _rootNode.TextChunkRefs[_chunkIdx];
						LogChunkNode chunkNode = await chunk.Target.ReadBlobAsync(cancellationToken: cancellationToken);

						// Get the source data
						_sourceBuffer = chunkNode.Data;
						_sourcePos = (int)(_currentOffset - chunk.Offset);
						_sourceEnd = (int)Math.Min(_sourceBuffer.Length, (_responseOffset + _responseLength) - chunk.Offset);
					}
					else
					{
						// End of the log
						break;
					}
				}
				return readBytes;
			}

			/// <inheritdoc/>
			public override long Seek(long offset, SeekOrigin origin) => throw new NotImplementedException();

			/// <inheritdoc/>
			public override void SetLength(long value) => throw new NotImplementedException();

			/// <inheritdoc/>
			public override void Write(byte[] buffer, int offset, int count) => throw new NotImplementedException();
		}

		readonly LogTailService _logTailService;

		/// <summary>
		/// Constructor
		/// </summary>
		public LogService(ILogCollection logCollection, ILogEventCollection logEventCollection, ILogStorage storage, LogTailService logTailService, StorageService storageService, IOptions<ServerSettings> settings, Tracer tracer, ILogger<LogService> logger)
		{
			_logCollection = logCollection;
			_logEventCollection = logEventCollection;
			_logCache = new MemoryCache(new MemoryCacheOptions());
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
			_logCache.Dispose();
			_storage.Dispose();
		}

		/// <inheritdoc/>
		public Task<ILog> CreateLogAsync(JobId jobId, LeaseId? leaseId, SessionId? sessionId, LogType type, LogId? logId, CancellationToken cancellationToken)
		{
			return _logCollection.CreateLogAsync(jobId, leaseId, sessionId, type, logId, cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<ILog?> GetLogAsync(LogId logId, CancellationToken cancellationToken)
		{
			return await _logCollection.GetLogAsync(logId, cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<List<Utf8String>> ReadLinesAsync(ILog log, int index, int count, CancellationToken cancellationToken)
		{
			List<Utf8String> lines = new List<Utf8String>();

			if (log.UseNewStorageBackend)
			{
				using IStorageClient storageClient = _storageService.CreateClient(log.NamespaceId);

				int maxIndex = index + count;
				bool complete = log.Complete;

				LogNode? root = await storageClient.TryReadRefTargetAsync<LogNode>(log.RefName, cancellationToken: cancellationToken);
				if (root != null)
				{
					int chunkIdx = root.TextChunkRefs.GetChunkForLine(index);
					for (; index < maxIndex && chunkIdx < root.TextChunkRefs.Count; chunkIdx++)
					{
						LogChunkRef chunk = root.TextChunkRefs[chunkIdx];
						LogChunkNode chunkData = await chunk.Target.ReadBlobAsync(cancellationToken: cancellationToken);

						for (; index < maxIndex && index < chunk.LineIndex; index++)
						{
							lines.Add(new Utf8String($"Internal error; missing data for line {index}\n"));
						}

						for (; index < maxIndex && index < chunk.LineIndex + chunk.LineCount; index++)
						{
							lines.Add(chunkData.GetLine(index - chunk.LineIndex));
						}
					}
					complete |= root.Complete;
				}

				if (!complete)
				{
					await _logTailService.EnableTailingAsync(log.Id, root?.LineCount ?? 0, cancellationToken);
					if (index < maxIndex)
					{
						await _logTailService.ReadAsync(log.Id, index, maxIndex - index, lines);
					}
				}
			}
			else
			{
				(_, long minOffset) = await GetLineOffsetAsync(log, index, cancellationToken);
				(_, long maxOffset) = await GetLineOffsetAsync(log, index + Math.Min(count, Int32.MaxValue - index), cancellationToken);

				byte[] result;
				using (System.IO.Stream stream = await OpenRawStreamAsync(log, minOffset, maxOffset - minOffset, cancellationToken))
				{
					result = new byte[stream.Length];
					await stream.ReadFixedSizeDataAsync(result, 0, result.Length, cancellationToken);
				}

				int offset = 0;
				for (int idx = 0; idx < result.Length; idx++)
				{
					if (result[idx] == (byte)'\n')
					{
						lines.Add(new Utf8String(result.AsMemory(offset, idx - offset)));
						offset = idx + 1;
					}
				}
			}

			return lines;
		}

		/// <inheritdoc/>
		public async Task<LogMetadata> GetMetadataAsync(ILog log, CancellationToken cancellationToken)
		{
			LogMetadata metadata = new LogMetadata();
			if (log.UseNewStorageBackend)
			{
				if (log.Complete)
				{
					metadata.MaxLineIndex = log.LineCount;
				}
				else
				{
					metadata.MaxLineIndex = await _logTailService.GetFullLineCountAsync(log.Id, log.LineCount, cancellationToken);
				}
			}
			else
			{
				if (log.Chunks.Count > 0)
				{
					ILogChunk chunk = log.Chunks[log.Chunks.Count - 1];
					if (log.MaxLineIndex == null || chunk.Length == 0)
					{
						LogChunkData chunkData = await ReadChunkAsync(log, log.Chunks.Count - 1);
						metadata.Length = chunk.Offset + chunkData.Length;
						metadata.MaxLineIndex = chunk.LineIndex + chunkData.LineCount;
					}
					else
					{
						metadata.Length = chunk.Offset + chunk.Length;
						metadata.MaxLineIndex = log.MaxLineIndex.Value;
					}
				}
			}
			return metadata;
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

			List<Utf8String> lines = await ReadLinesAsync(log, lineIndex, lineCount, cancellationToken);
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

		/// <inheritdoc/>
		public Task<Stream> OpenRawStreamAsync(ILog log, CancellationToken cancellationToken)
		{
			return OpenRawStreamAsync(log, 0, Int64.MaxValue, cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<Stream> OpenRawStreamAsync(ILog log, long offset, long length, CancellationToken cancellationToken)
		{
			if (log.UseNewStorageBackend)
			{
				using IStorageClient storageClient = _storageService.CreateClient(log.NamespaceId);

				LogNode? root = await storageClient.TryReadRefTargetAsync<LogNode>(log.RefName, cancellationToken: cancellationToken);
				if (root == null || root.TextChunkRefs.Count == 0)
				{
					return new MemoryStream(Array.Empty<byte>(), false);
				}
				else
				{
					int lastChunkIdx = root.TextChunkRefs.Count - 1;

					// Clamp the length of the request
					LogChunkRef lastChunk = root.TextChunkRefs[lastChunkIdx];
					if (length > lastChunk.Offset)
					{
						long lastChunkLength = lastChunk.Length;
						if (lastChunkLength <= 0)
						{
							LogChunkNode lastChunkNode = await lastChunk.Target.ReadBlobAsync(cancellationToken: cancellationToken);
							lastChunkLength = lastChunkNode.Length;
						}
						length = Math.Min(length, (lastChunk.Offset + lastChunkLength) - offset);
					}

					// Create the new stream
					return new NewLoggerResponseStream(root, offset, length);
				}
			}
			else
			{
				if (log.Chunks.Count == 0)
				{
					return new MemoryStream(Array.Empty<byte>(), false);
				}
				else
				{
					int lastChunkIdx = log.Chunks.Count - 1;

					// Clamp the length of the request
					ILogChunk lastChunk = log.Chunks[lastChunkIdx];
					if (length > lastChunk.Offset)
					{
						long lastChunkLength = lastChunk.Length;
						if (lastChunkLength <= 0)
						{
							LogChunkData lastChunkData = await ReadChunkAsync(log, lastChunkIdx);
							lastChunkLength = lastChunkData.Length;
						}
						length = Math.Min(length, (lastChunk.Offset + lastChunkLength) - offset);
					}

					// Create the new stream
					return new ResponseStream(this, log, offset, length);
				}
			}
		}

		/// <summary>
		/// Helper method for catching exceptions in <see cref="LogText.ConvertToPlainText(ReadOnlySpan{Byte}, Byte[], Int32)"/>
		/// </summary>
		public static int GuardedConvertToPlainText(ReadOnlySpan<byte> input, byte[] output, int outputOffset, ILogger logger)
		{
			try
			{
				return LogText.ConvertToPlainText(input, output, outputOffset);
			}
			catch (Exception ex)
			{
				logger.LogWarning(ex, "Unable to convert log line to plain text: {Line}", Encoding.UTF8.GetString(input));
				output[outputOffset] = (byte)'\n';
				return outputOffset + 1;
			}
		}

		/// <inheritdoc/>
		public async Task CopyPlainTextStreamAsync(ILog log, Stream outputStream, CancellationToken cancellationToken)
		{
			long offset = 0;
			long length = Int64.MaxValue;

			using (Stream stream = await OpenRawStreamAsync(log, 0, Int64.MaxValue, cancellationToken))
			{
				byte[] readBuffer = new byte[4096];
				int readBufferLength = 0;

				byte[] writeBuffer = new byte[4096];
				int writeBufferLength = 0;

				while (length > 0)
				{
					// Add more data to the buffer
					int readBytes = await stream.ReadAsync(readBuffer.AsMemory(readBufferLength, readBuffer.Length - readBufferLength), cancellationToken);
					readBufferLength += readBytes;

					// Copy as many lines as possible to the output
					int convertedBytes = 0;
					for (int endIdx = 1; endIdx < readBufferLength; endIdx++)
					{
						if (readBuffer[endIdx] == '\n')
						{
							writeBufferLength = GuardedConvertToPlainText(readBuffer.AsSpan(convertedBytes, endIdx - convertedBytes), writeBuffer, writeBufferLength, _logger);
							convertedBytes = endIdx + 1;
						}
					}

					// If there's anything in the write buffer, write it out
					if (writeBufferLength > 0)
					{
						if (offset < writeBufferLength)
						{
							int writeLength = (int)Math.Min((long)writeBufferLength - offset, length);
							await outputStream.WriteAsync(writeBuffer.AsMemory((int)offset, writeLength), cancellationToken);
							length -= writeLength;
						}
						offset = Math.Max(offset - writeBufferLength, 0);
						writeBufferLength = 0;
					}

					// If we were able to read something, shuffle down the rest of the buffer. Otherwise expand the read buffer.
					if (convertedBytes > 0)
					{
						Buffer.BlockCopy(readBuffer, convertedBytes, readBuffer, 0, readBufferLength - convertedBytes);
						readBufferLength -= convertedBytes;
					}
					else if (readBufferLength > 0)
					{
						Array.Resize(ref readBuffer, readBuffer.Length + 128);
						writeBuffer = new byte[readBuffer.Length];
					}

					// Exit if we didn't read anything in this iteration
					if (readBytes == 0)
					{
						break;
					}
				}
			}
		}

		/// <inheritdoc/>
		public async Task<(int, long)> GetLineOffsetAsync(ILog log, int lineIdx, CancellationToken cancellationToken)
		{
			if (log.UseNewStorageBackend)
			{
				using IStorageClient storageClient = _storageService.CreateClient(log.NamespaceId);

				LogNode? root = await storageClient.TryReadRefTargetAsync<LogNode>(log.RefName, cancellationToken: cancellationToken);
				if (root == null)
				{
					return (0, 0);
				}

				int chunkIdx = root.TextChunkRefs.GetChunkForLine(lineIdx);
				LogChunkRef chunk = root.TextChunkRefs[chunkIdx];
				LogChunkNode chunkData = await chunk.Target.ReadBlobAsync(cancellationToken: cancellationToken);

				if (lineIdx < chunk.LineIndex)
				{
					lineIdx = chunk.LineIndex;
				}

				int maxLineIndex = chunk.LineIndex + chunkData.LineCount;
				if (lineIdx >= maxLineIndex)
				{
					lineIdx = maxLineIndex;
				}

				long offset = chunk.Offset + chunkData.LineOffsets[lineIdx - chunk.LineIndex];
				return (lineIdx, offset);
			}
			else
			{
				int chunkIdx = log.Chunks.GetChunkForLine(lineIdx);

				ILogChunk chunk = log.Chunks[chunkIdx];
				LogChunkData chunkData = await ReadChunkAsync(log, chunkIdx);

				if (lineIdx < chunk.LineIndex)
				{
					lineIdx = chunk.LineIndex;
				}

				int maxLineIndex = chunk.LineIndex + chunkData.LineCount;
				if (lineIdx >= maxLineIndex)
				{
					lineIdx = maxLineIndex;
				}

				long offset = chunk.Offset + chunkData.GetLineOffsetWithinChunk(lineIdx - chunk.LineIndex);
				return (lineIdx, offset);
			}
		}

		/// <summary>
		/// Reads a chunk from storage
		/// </summary>
		/// <param name="log">Log file to read from</param>
		/// <param name="chunkIdx">The chunk to read</param>
		/// <returns>Chunk data</returns>
		private async Task<LogChunkData> ReadChunkAsync(ILog log, int chunkIdx)
		{
			ILogChunk chunk = log.Chunks[chunkIdx];

			// Try to read the chunk data from storage
			LogChunkData? chunkData = null;
			try
			{
				chunkData = await _storage.ReadChunkAsync(log.Id, chunk.Offset, chunk.LineIndex);
			}
			catch (Exception ex)
			{
				_logger.LogError(ex, "Unable to read log {LogId} at offset {Offset}", log.Id, chunk.Offset);
			}

			// Get the minimum length and line count for the chunk
			if (chunkIdx + 1 < log.Chunks.Count)
			{
				ILogChunk nextChunk = log.Chunks[chunkIdx + 1];
				chunkData = RepairChunkData(log, chunkIdx, chunkData, (int)(nextChunk.Offset - chunk.Offset), nextChunk.LineIndex - chunk.LineIndex, $"before next");
			}
			else
			{
				if (log.MaxLineIndex != null && chunk.Length != 0)
				{
					chunkData = RepairChunkData(log, chunkIdx, chunkData, chunk.Length, log.MaxLineIndex.Value - chunk.LineIndex, $"last chunk (max line index = {log.MaxLineIndex})");
				}
				else
				{
					chunkData ??= RepairChunkData(log, chunkIdx, chunkData, 1024, 1, "default");
				}
			}

			return chunkData;
		}

		/// <summary>
		/// Validates the given chunk data, and fix it up if necessary
		/// </summary>
		/// <param name="log">The log file instance</param>
		/// <param name="chunkIdx">Index of the chunk within the log</param>
		/// <param name="chunkData">The chunk data that was read</param>
		/// <param name="length">Expected length of the data</param>
		/// <param name="lineCount">Expected number of lines in the data</param>
		/// <param name="context">Context string for diagnostic output</param>
		/// <returns>Repaired chunk data</returns>
		LogChunkData RepairChunkData(ILog log, int chunkIdx, LogChunkData? chunkData, int length, int lineCount, string context)
		{
			int currentLength = 0;
			int currentLineCount = 0;
			if (chunkData != null)
			{
				currentLength = chunkData.Length;
				currentLineCount = chunkData.LineCount;
			}

			if (chunkData == null || currentLength < length || currentLineCount < lineCount)
			{
				_logger.LogWarning("Creating placeholder subchunk for log {LogId} chunk {ChunkIdx} (length {Length} vs expected {ExpLength}, lines {LineCount} vs expected {ExpLineCount}, context {Context})", log.Id, chunkIdx, currentLength, length, currentLineCount, lineCount, context);

				List<LogSubChunkData> subChunks = new List<LogSubChunkData>();
				if (chunkData != null && chunkData.Length < length && chunkData.LineCount < lineCount)
				{
					subChunks.AddRange(chunkData.SubChunks);
				}

				LogText text = new LogText();
				text.AppendMissingDataInfo(chunkIdx, log.Chunks[chunkIdx].Server, length - currentLength, lineCount - currentLineCount);
				subChunks.Add(new LogSubChunkData(log.Type, currentLength, currentLineCount, text));

				ILogChunk chunk = log.Chunks[chunkIdx];
				chunkData = new LogChunkData(chunk.Offset, chunk.LineIndex, subChunks);
			}
			return chunkData;
		}

		/// <summary>
		/// Reads a chunk from storage
		/// </summary>
		/// <param name="log">Log file to read from</param>
		/// <param name="length">Length of the log covered by the index</param>
		/// <returns>Chunk data</returns>
		private async Task<LogIndexData?> ReadIndexAsync(ILog log, long length)
		{
			try
			{
				LogIndexData? index = await _storage.ReadIndexAsync(log.Id, length);
				return index;
			}
			catch (Exception ex)
			{
				_logger.LogError(ex, "Unable to read log {LogId} index at length {Length}", log.Id, length);
				return null;
			}
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

		/// <inheritdoc/>
		public async Task<List<int>> SearchLogDataAsync(ILog log, string text, int firstLine, int count, SearchStats searchStats, CancellationToken cancellationToken)
		{
			Stopwatch timer = Stopwatch.StartNew();

			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(LogService)}.{nameof(SearchLogDataAsync)}");
			span.SetAttribute("logId", log.Id.ToString());
			span.SetAttribute("text", text);
			span.SetAttribute("count", count);

			List<int> results = new List<int>();
			if (count > 0)
			{
				IAsyncEnumerable<int> enumerable = (log.UseNewStorageBackend) ?
						SearchLogDataInternalNewAsync(log, text, firstLine, searchStats, cancellationToken) :
						SearchLogDataInternalAsync(log, text, firstLine, searchStats);

				await using IAsyncEnumerator<int> enumerator = enumerable.GetAsyncEnumerator(cancellationToken);
				while (await enumerator.MoveNextAsync() && results.Count < count)
				{
					results.Add(enumerator.Current);
				}
			}

			_logger.LogDebug("Search for \"{SearchText}\" in log {LogId} found {NumResults}/{MaxResults} results, took {Time}ms ({@Stats})", text, log.Id, results.Count, count, timer.ElapsedMilliseconds, searchStats);
			return results;
		}

		async IAsyncEnumerable<int> SearchLogDataInternalNewAsync(ILog log, string text, int firstLine, SearchStats searchStats, [EnumeratorCancellation] CancellationToken cancellationToken)
		{
			SearchTerm searchText = new SearchTerm(text);
			using IStorageClient storageClient = _storageService.CreateClient(log.NamespaceId);

			// Search the index
			if (log.LineCount > 0)
			{
				LogNode? root = await storageClient.ReadRefTargetAsync<LogNode>(log.RefName, cancellationToken: cancellationToken);
				if (root != null)
				{
					LogIndexNode index = await root.IndexRef.ReadBlobAsync(cancellationToken: cancellationToken);
					await foreach (int lineIdx in index.SearchAsync(firstLine, searchText, searchStats, cancellationToken: cancellationToken))
					{
						yield return lineIdx;
					}
					if (root.Complete)
					{
						yield break;
					}
					firstLine = root.LineCount;
				}
			}

			// Search any tail data we have
			if (!log.Complete)
			{
				for (; ; )
				{
					Utf8String[] lines = await ReadTailAsync(log, firstLine, cancellationToken);
					if (lines.Length == 0)
					{
						break;
					}

					for (int idx = 0; idx < lines.Length; idx++)
					{
						if (SearchTerm.FindNextOcurrence(lines[idx].Span, 0, searchText) != -1)
						{
							yield return firstLine + idx;
						}
					}

					firstLine += lines.Length;
				}
			}
		}

		async Task<Utf8String[]> ReadTailAsync(ILog log, int index, CancellationToken cancellationToken)
		{
			_ = cancellationToken;

			const int BatchSize = 128;

			if (log.Complete)
			{
				return Array.Empty<Utf8String>();
			}

			string cacheKey = $"{log.Id}@{index}";
			if (_logCache.TryGetValue(cacheKey, out Utf8String[]? lines))
			{
				return lines!;
			}

			lines = (await _logTailService.ReadAsync(log.Id, index, BatchSize)).ToArray();
			if (log.Type == LogType.Json)
			{
				LogChunkBuilder builder = new LogChunkBuilder(lines.Sum(x => x.Length));
				foreach (Utf8String line in lines)
				{
					builder.AppendJsonAsPlainText(line.Span, _logger);
				}
				lines = lines.ToArray();
			}

			if (lines.Length == BatchSize)
			{
				int length = lines.Sum(x => x.Length);
				using (ICacheEntry entry = _logCache.CreateEntry(cacheKey))
				{
					entry.SetSlidingExpiration(TimeSpan.FromMinutes(1.0));
					entry.SetSize(length);
					entry.SetValue(lines);
				}
			}

			return lines;
		}

		async IAsyncEnumerable<int> SearchLogDataInternalAsync(ILog log, string text, int firstLine, SearchStats searchStats)
		{
			SearchText searchText = new SearchText(text);

			// Read the index for this log file
			if (log.IndexLength != null)
			{
				LogIndexData? indexData = await ReadIndexAsync(log, log.IndexLength.Value);
				if (indexData != null && firstLine < indexData.LineCount)
				{
					using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(LogService)}.{nameof(SearchLogDataInternalAsync)}.Indexed");
					span.SetAttribute("lineCount", indexData.LineCount);

					foreach (int lineIndex in indexData.Search(firstLine, searchText, searchStats))
					{
						yield return lineIndex;
					}

					firstLine = indexData.LineCount;
				}
			}

			// Manually search through the rest of the log
			int chunkIdx = log.Chunks.GetChunkForLine(firstLine);
			for (; chunkIdx < log.Chunks.Count; chunkIdx++)
			{
				ILogChunk chunk = log.Chunks[chunkIdx];

				// Read the chunk data
				LogChunkData chunkData = await ReadChunkAsync(log, chunkIdx);
				if (firstLine < chunkData.LineIndex + chunkData.LineCount)
				{
					// Find the first sub-chunk we're looking for
					int subChunkIdx = 0;
					if (firstLine > chunk.LineIndex)
					{
						subChunkIdx = chunkData.GetSubChunkForLine(firstLine - chunk.LineIndex);
					}

					// Search through the sub-chunks
					for (; subChunkIdx < chunkData.SubChunks.Count; subChunkIdx++)
					{
						LogSubChunkData subChunkData = chunkData.SubChunks[subChunkIdx];
						if (firstLine < subChunkData.LineIndex + subChunkData.LineCount)
						{
							// Create an index containing just this sub-chunk
							LogIndexData index = subChunkData.BuildIndex(_logger);
							foreach (int lineIndex in index.Search(firstLine, searchText, searchStats))
							{
								yield return lineIndex;
							}
						}
					}
				}
			}
		}
	}
}

