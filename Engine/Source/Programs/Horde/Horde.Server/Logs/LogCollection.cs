// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Agents.Sessions;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Logs;
using EpicGames.Horde.Storage;
using Horde.Server.Logs.Data;
using Horde.Server.Server;
using Horde.Server.Storage;
using Horde.Server.Utilities;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Logging;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;
using OpenTelemetry.Trace;

namespace Horde.Server.Logs
{
	/// <summary>
	/// Wrapper around the jobs collection in a mongo DB
	/// </summary>
	public sealed class LogCollection : ILogCollection, IDisposable
	{
		class Log : ILog
		{
			readonly LogCollection _collection;
			readonly LogDocument _document;

			LogId ILog.Id => _document.Id;
			JobId ILog.JobId => _document.JobId;
			LeaseId? ILog.LeaseId => _document.LeaseId;
			SessionId? ILog.SessionId => _document.SessionId;
			LogType ILog.Type => _document.Type;
			NamespaceId ILog.NamespaceId => _document.NamespaceId;
			RefName ILog.RefName => _document.RefName;

			public Log(LogCollection collection, LogDocument document)
			{
				_collection = collection;
				_document = document;
			}

			public async Task<ILog> UpdateLineCountAsync(int lineCount, bool complete, CancellationToken cancellationToken = default)
			{
				LogDocument newDocument = await _collection.UpdateLineCountAsync(_document, lineCount, complete, cancellationToken);
				return new Log(_collection, newDocument);
			}

			public Task<List<Utf8String>> ReadLinesAsync(int index, int count, CancellationToken cancellationToken = default)
				=> _collection.ReadLinesAsync(_document, index, count, cancellationToken);

			public Task<LogMetadata> GetMetadataAsync(CancellationToken cancellationToken)
				=> _collection.GetMetadataAsync(_document, cancellationToken);

			public Task<Stream> OpenRawStreamAsync(CancellationToken cancellationToken = default)
				=> _collection.OpenRawStreamAsync(_document, 0, Int64.MaxValue, cancellationToken);

			public Task<Stream> OpenRawStreamAsync(long offset, long length, CancellationToken cancellationToken)
				=> _collection.OpenRawStreamAsync(_document, offset, length, cancellationToken);

			public Task CopyPlainTextStreamAsync(Stream outputStream, CancellationToken cancellationToken = default)
				=> _collection.CopyPlainTextStreamAsync(this, outputStream, cancellationToken);

			public Task<List<int>> SearchLogDataAsync(string text, int firstLine, int count, SearchStats stats, CancellationToken cancellationToken)
				=> _collection.SearchLogDataAsync(_document, text, firstLine, count, stats, cancellationToken);
		}

		class LogDocument
		{
			[BsonRequired, BsonId]
			public LogId Id { get; set; }

			[BsonRequired]
			public JobId JobId { get; set; }

			[BsonIgnoreIfNull]
			public LeaseId? LeaseId { get; set; }

			[BsonIgnoreIfNull]
			public SessionId? SessionId { get; set; }

			public LogType Type { get; set; }
			public bool UseNewStorageBackend { get; set; }

			[BsonIgnoreIfNull]
			public int? MaxLineIndex { get; set; }

			[BsonIgnoreIfNull]
			public long? IndexLength { get; set; }

			public List<LogChunkDocument> Chunks { get; set; } = new List<LogChunkDocument>();

			public int LineCount { get; set; }

			public NamespaceId NamespaceId { get; set; } = Namespace.Logs;
			public RefName RefName { get; set; }

			[BsonIgnoreIfDefault]
			public bool Complete { get; set; }

			[BsonRequired]
			public int UpdateIndex { get; set; }

			[BsonConstructor]
			private LogDocument()
			{
			}

			public LogDocument(JobId jobId, LeaseId? leaseId, SessionId? sessionId, LogType type, LogId? logId, NamespaceId namespaceId)
			{
				Id = logId ?? LogIdUtils.GenerateNewId();
				JobId = jobId;
				LeaseId = leaseId;
				SessionId = sessionId;
				Type = type;
				UseNewStorageBackend = true;
				MaxLineIndex = 0;
				NamespaceId = namespaceId;
				RefName = new RefName(Id.ToString());
			}
		}

		class LogChunkDocument
		{
			public long Offset { get; set; }
			public int Length { get; set; }
			public int LineIndex { get; set; }

			[BsonIgnoreIfNull]
			public string? Server { get; set; }

			[BsonConstructor]
			public LogChunkDocument()
			{
			}

			public LogChunkDocument(LogChunkDocument other)
			{
				Offset = other.Offset;
				Length = other.Length;
				LineIndex = other.LineIndex;
				Server = other.Server;
			}

			public LogChunkDocument Clone()
			{
				return (LogChunkDocument)MemberwiseClone();
			}
		}

		readonly IMongoCollection<LogDocument> _logCollection;
		readonly ILogStorage _storage;
		readonly LogTailService _logTailService;
		readonly StorageService _storageService;
		readonly Tracer _tracer;
		readonly ILogger _logger;
		readonly IMemoryCache _logCache;

		/// <summary>
		/// Constructor
		/// </summary>
		public LogCollection(MongoService mongoService, ILogStorage storage, LogTailService logTailService, StorageService storageService, Tracer tracer, ILogger<LogCollection> logger)
		{
			_logCollection = mongoService.GetCollection<LogDocument>("LogFiles");
			_storage = storage;
			_logTailService = logTailService;
			_storageService = storageService;
			_tracer = tracer;
			_logger = logger;
			_logCache = new MemoryCache(new MemoryCacheOptions());
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			_logCache.Dispose();
		}

		/// <inheritdoc/>
		public async Task<ILog> AddAsync(JobId jobId, LeaseId? leaseId, SessionId? sessionId, LogType type, LogId? logId, CancellationToken cancellationToken)
		{
			LogDocument newLog = new LogDocument(jobId, leaseId, sessionId, type, logId, Namespace.Logs);
			await _logCollection.InsertOneAsync(newLog, null, cancellationToken);
			return new Log(this, newLog);
		}

		/// <inheritdoc/>
		public async Task<ILog?> GetAsync(LogId logId, CancellationToken cancellationToken)
		{
			LogDocument? logDocument = await _logCollection.Find<LogDocument>(x => x.Id == logId).FirstOrDefaultAsync(cancellationToken);
			if (logDocument == null)
			{
				return null;
			}

			if (logDocument.UseNewStorageBackend)
			{
				return new Log(this, logDocument);
			}
			else
			{
				return new LogV1(this, logDocument);
			}
		}

		/// <inheritdoc/>
		async Task<List<Utf8String>> ReadLinesAsync(LogDocument log, int index, int count, CancellationToken cancellationToken)
		{
			List<Utf8String> lines = new List<Utf8String>();

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

			return lines;
		}

		/// <inheritdoc/>
		async Task<LogMetadata> GetMetadataAsync(LogDocument log, CancellationToken cancellationToken)
		{
			LogMetadata metadata = new LogMetadata();
			if (log.Complete)
			{
				metadata.MaxLineIndex = log.LineCount;
			}
			else
			{
				metadata.MaxLineIndex = await _logTailService.GetFullLineCountAsync(log.Id, log.LineCount, cancellationToken);
			}
			return metadata;
		}

		/// <inheritdoc/>
		async Task<LogDocument> UpdateLineCountAsync(LogDocument log, int lineCount, bool complete, CancellationToken cancellationToken)
		{
			FilterDefinition<LogDocument> filter = Builders<LogDocument>.Filter.Eq(x => x.Id, log.Id);
			UpdateDefinition<LogDocument> update = Builders<LogDocument>.Update.Set(x => x.LineCount, lineCount).Set(x => x.Complete, complete).Inc(x => x.UpdateIndex, 1);
			return await _logCollection.FindOneAndUpdateAsync(filter, update, new FindOneAndUpdateOptions<LogDocument, LogDocument> { ReturnDocument = ReturnDocument.After }, cancellationToken);
		}

		/// <inheritdoc/>
		async Task<(int, long)> GetLineOffsetAsync(LogDocument log, int lineIdx, CancellationToken cancellationToken)
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

		/// <inheritdoc/>
		async Task<Stream> OpenRawStreamAsync(LogDocument log, long offset, long length, CancellationToken cancellationToken)
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
				return new ResponseStream(root, offset, length);
			}
		}

		/// <inheritdoc/>
		async Task<List<int>> SearchLogDataAsync(LogDocument log, string text, int firstLine, int count, SearchStats searchStats, CancellationToken cancellationToken)
		{
			Stopwatch timer = Stopwatch.StartNew();

			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(LogService)}.{nameof(SearchLogDataAsync)}");
			span.SetAttribute("logId", log.Id.ToString());
			span.SetAttribute("text", text);
			span.SetAttribute("count", count);

			List<int> results = new List<int>();
			if (count > 0)
			{
				IAsyncEnumerable<int> enumerable = SearchLogDataInternalNewAsync(log, text, firstLine, searchStats, cancellationToken);

				await using IAsyncEnumerator<int> enumerator = enumerable.GetAsyncEnumerator(cancellationToken);
				while (await enumerator.MoveNextAsync() && results.Count < count)
				{
					results.Add(enumerator.Current);
				}
			}

			_logger.LogDebug("Search for \"{SearchText}\" in log {LogId} found {NumResults}/{MaxResults} results, took {Time}ms ({@Stats})", text, log.Id, results.Count, count, timer.ElapsedMilliseconds, searchStats);
			return results;
		}

		async IAsyncEnumerable<int> SearchLogDataInternalNewAsync(LogDocument log, string text, int firstLine, SearchStats searchStats, [EnumeratorCancellation] CancellationToken cancellationToken)
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

		async Task<Utf8String[]> ReadTailAsync(LogDocument log, int index, CancellationToken cancellationToken)
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

		/// <inheritdoc/>
		async Task CopyPlainTextStreamAsync(ILog log, Stream outputStream, CancellationToken cancellationToken)
		{
			long offset = 0;
			long length = Int64.MaxValue;

			using (Stream stream = await log.OpenRawStreamAsync(0, Int64.MaxValue, cancellationToken))
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

		/// <summary>
		/// Streams log data to a caller
		/// </summary>
		class ResponseStream : Stream
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
			public ResponseStream(LogNode rootNode, long offset, long length)
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

		#region V1

		class LogV1 : ILog
		{
			readonly LogCollection _collection;
			readonly LogDocument _document;

			LogId ILog.Id => _document.Id;
			JobId ILog.JobId => _document.JobId;
			LeaseId? ILog.LeaseId => _document.LeaseId;
			SessionId? ILog.SessionId => _document.SessionId;
			LogType ILog.Type => _document.Type;
			NamespaceId ILog.NamespaceId => _document.NamespaceId;
			RefName ILog.RefName => _document.RefName;

			public LogV1(LogCollection collection, LogDocument document)
			{
				_collection = collection;
				_document = document;
			}

			public async Task<ILog> UpdateLineCountAsync(int lineCount, bool complete, CancellationToken cancellationToken = default)
			{
				LogDocument newDocument = await _collection.UpdateLineCountAsync(_document, lineCount, complete, cancellationToken);
				return new Log(_collection, newDocument);
			}

			public Task<List<Utf8String>> ReadLinesAsync(int index, int count, CancellationToken cancellationToken = default)
				=> _collection.ReadLinesV1Async(_document, index, count, cancellationToken);

			public Task<LogMetadata> GetMetadataAsync(CancellationToken cancellationToken)
				=> _collection.GetMetadataV1Async(_document);

			public Task<Stream> OpenRawStreamAsync(CancellationToken cancellationToken = default)
				=> _collection.OpenRawStreamV1Async(_document, 0, Int64.MaxValue);

			public Task<Stream> OpenRawStreamAsync(long offset, long length, CancellationToken cancellationToken)
				=> _collection.OpenRawStreamV1Async(_document, offset, length);

			public Task CopyPlainTextStreamAsync(Stream outputStream, CancellationToken cancellationToken = default)
				=> _collection.CopyPlainTextStreamAsync(this, outputStream, cancellationToken);

			public Task<List<int>> SearchLogDataAsync(string text, int firstLine, int count, SearchStats stats, CancellationToken cancellationToken)
				=> _collection.SearchLogDataV1Async(_document, text, firstLine, count, stats, cancellationToken);
		}

		async Task<List<Utf8String>> ReadLinesV1Async(LogDocument log, int index, int count, CancellationToken cancellationToken)
		{
			List<Utf8String> lines = new List<Utf8String>();

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

			return lines;
		}

		/// <inheritdoc/>
		async Task<LogMetadata> GetMetadataV1Async(LogDocument log)
		{
			LogMetadata metadata = new LogMetadata();
			if (log.Chunks.Count > 0)
			{
				LogChunkDocument chunk = log.Chunks[log.Chunks.Count - 1];
				if (log.MaxLineIndex == null || chunk.Length == 0)
				{
					LogChunkData chunkData = await ReadChunkV1Async(log, log.Chunks.Count - 1);
					metadata.Length = chunk.Offset + chunkData.Length;
					metadata.MaxLineIndex = chunk.LineIndex + chunkData.LineCount;
				}
				else
				{
					metadata.Length = chunk.Offset + chunk.Length;
					metadata.MaxLineIndex = log.MaxLineIndex.Value;
				}
			}
			return metadata;
		}

		/// <inheritdoc/>
		async Task<Stream> OpenRawStreamV1Async(LogDocument log, long offset, long length)
		{
			if (log.Chunks.Count == 0)
			{
				return new MemoryStream(Array.Empty<byte>(), false);
			}
			else
			{
				int lastChunkIdx = log.Chunks.Count - 1;

				// Clamp the length of the request
				LogChunkDocument lastChunk = log.Chunks[lastChunkIdx];
				if (length > lastChunk.Offset)
				{
					long lastChunkLength = lastChunk.Length;
					if (lastChunkLength <= 0)
					{
						LogChunkData lastChunkData = await ReadChunkV1Async(log, lastChunkIdx);
						lastChunkLength = lastChunkData.Length;
					}
					length = Math.Min(length, (lastChunk.Offset + lastChunkLength) - offset);
				}

				// Create the new stream
				return new ResponseStreamV1(this, log, offset, length);
			}
		}

		/// <inheritdoc/>
		async Task<List<int>> SearchLogDataV1Async(LogDocument log, string text, int firstLine, int count, SearchStats searchStats, CancellationToken cancellationToken)
		{
			Stopwatch timer = Stopwatch.StartNew();

			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(LogService)}.{nameof(SearchLogDataAsync)}");
			span.SetAttribute("logId", log.Id.ToString());
			span.SetAttribute("text", text);
			span.SetAttribute("count", count);

			List<int> results = new List<int>();
			if (count > 0)
			{
				IAsyncEnumerable<int> enumerable = SearchLogDataInternalV1Async(log, text, firstLine, searchStats);

				await using IAsyncEnumerator<int> enumerator = enumerable.GetAsyncEnumerator(cancellationToken);
				while (await enumerator.MoveNextAsync() && results.Count < count)
				{
					results.Add(enumerator.Current);
				}
			}

			_logger.LogDebug("Search for \"{SearchText}\" in log {LogId} found {NumResults}/{MaxResults} results, took {Time}ms ({@Stats})", text, log.Id, results.Count, count, timer.ElapsedMilliseconds, searchStats);
			return results;
		}

		/// <summary>
		/// Reads a chunk from storage
		/// </summary>
		/// <param name="log">Log file to read from</param>
		/// <param name="chunkIdx">The chunk to read</param>
		/// <returns>Chunk data</returns>
		private async Task<LogChunkData> ReadChunkV1Async(LogDocument log, int chunkIdx)
		{
			LogChunkDocument chunk = log.Chunks[chunkIdx];

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
				LogChunkDocument nextChunk = log.Chunks[chunkIdx + 1];
				chunkData = RepairChunkDataV1(log, chunkIdx, chunkData, (int)(nextChunk.Offset - chunk.Offset), nextChunk.LineIndex - chunk.LineIndex, $"before next");
			}
			else
			{
				if (log.MaxLineIndex != null && chunk.Length != 0)
				{
					chunkData = RepairChunkDataV1(log, chunkIdx, chunkData, chunk.Length, log.MaxLineIndex.Value - chunk.LineIndex, $"last chunk (max line index = {log.MaxLineIndex})");
				}
				else
				{
					chunkData ??= RepairChunkDataV1(log, chunkIdx, chunkData, 1024, 1, "default");
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
		LogChunkData RepairChunkDataV1(LogDocument log, int chunkIdx, LogChunkData? chunkData, int length, int lineCount, string context)
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

				LogChunkDocument chunk = log.Chunks[chunkIdx];
				chunkData = new LogChunkData(chunk.Offset, chunk.LineIndex, subChunks);
			}
			return chunkData;
		}

		async IAsyncEnumerable<int> SearchLogDataInternalV1Async(LogDocument log, string text, int firstLine, SearchStats searchStats)
		{
			SearchText searchText = new SearchText(text);

			// Read the index for this log file
			if (log.IndexLength != null)
			{
				LogIndexData? indexData = await ReadIndexV1Async(log, log.IndexLength.Value);
				if (indexData != null && firstLine < indexData.LineCount)
				{
					using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(LogService)}.{nameof(SearchLogDataInternalV1Async)}.Indexed");
					span.SetAttribute("lineCount", indexData.LineCount);

					foreach (int lineIndex in indexData.Search(firstLine, searchText, searchStats))
					{
						yield return lineIndex;
					}

					firstLine = indexData.LineCount;
				}
			}

			// Manually search through the rest of the log
			int chunkIdx = GetChunkForLineV1(log.Chunks, firstLine);
			for (; chunkIdx < log.Chunks.Count; chunkIdx++)
			{
				LogChunkDocument chunk = log.Chunks[chunkIdx];

				// Read the chunk data
				LogChunkData chunkData = await ReadChunkV1Async(log, chunkIdx);
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

		/// <summary>
		/// Reads a chunk from storage
		/// </summary>
		/// <param name="log">Log file to read from</param>
		/// <param name="length">Length of the log covered by the index</param>
		/// <returns>Chunk data</returns>
		private async Task<LogIndexData?> ReadIndexV1Async(LogDocument log, long length)
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
		/// Gets the chunk index containing the given offset.
		/// </summary>
		/// <param name="chunks">The chunks to search</param>
		/// <param name="offset">The offset to search for</param>
		/// <returns>The chunk index containing the given offset</returns>
		static int GetChunkForOffsetV1(IReadOnlyList<LogChunkDocument> chunks, long offset)
		{
			int chunkIndex = chunks.BinarySearch(x => x.Offset, offset);
			if (chunkIndex < 0)
			{
				chunkIndex = ~chunkIndex - 1;
			}
			return chunkIndex;
		}

		/// <summary>
		/// Gets the starting chunk index for the given line
		/// </summary>
		/// <param name="chunks">The chunks to search</param>
		/// <param name="lineIndex">Index of the line to query</param>
		/// <returns>Index of the chunk to fetch</returns>
		static int GetChunkForLineV1(IReadOnlyList<LogChunkDocument> chunks, int lineIndex)
		{
			int chunkIndex = chunks.BinarySearch(x => x.LineIndex, lineIndex);
			if (chunkIndex < 0)
			{
				chunkIndex = ~chunkIndex - 1;
			}
			return chunkIndex;
		}

		/// <summary>
		/// Streams log data to a caller
		/// </summary>
		class ResponseStreamV1 : Stream
		{
			/// <summary>
			/// The log file service that created this stream
			/// </summary>
			readonly LogCollection _logCollection;

			/// <summary>
			/// The log file being read
			/// </summary>
			readonly LogDocument _log;

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
			public ResponseStreamV1(LogCollection logService, LogDocument log, long offset, long length)
			{
				_logCollection = logService;
				_log = log;

				_responseOffset = offset;
				_responseLength = length;

				_currentOffset = offset;

				_chunkIdx = GetChunkForOffsetV1(log.Chunks, offset);
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
						LogChunkDocument chunk = _log.Chunks[_chunkIdx];
						LogChunkData chunkData = await _logCollection.ReadChunkV1Async(_log, _chunkIdx);

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

		#endregion
	}
}
