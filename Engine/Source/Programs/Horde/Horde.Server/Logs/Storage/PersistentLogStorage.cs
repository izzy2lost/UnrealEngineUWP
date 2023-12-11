// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Logs;
using EpicGames.Horde.Storage;
using Horde.Server.Logs.Data;
using Horde.Server.Storage;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Logs.Storage
{
	/// <summary>
	/// Bulk storage for log file data
	/// </summary>
	class PersistentLogStorage : ILogStorage
	{
		readonly IStorageBackend _storageProvider;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="storageProvider">The storage provider</param>
		/// <param name="logger">Logging provider</param>
		public PersistentLogStorage(IStorageBackend<PersistentLogStorage> storageProvider, ILogger<PersistentLogStorage> logger)
		{
			_storageProvider = storageProvider;
			_logger = logger;
		}

		/// <inheritdoc/>
		public void Dispose()
		{
		}

		/// <inheritdoc/>
		public async Task<LogIndexData?> ReadIndexAsync(LogId logId, long length)
		{
			_logger.LogDebug("Reading log {LogId} index length {Length} from persistent storage", logId, length);

			string path = $"{logId}/index_{length}";
			ReadOnlyMemory<byte> data = await _storageProvider.ReadBytesAsync(path);
			return LogIndexData.FromMemory(data);
		}

		/// <inheritdoc/>
		public async Task WriteIndexAsync(LogId logId, long length, LogIndexData indexData)
		{
			_logger.LogDebug("Writing log {LogId} index length {Length} to persistent storage", logId, length);

			string path = $"{logId}/index_{length}";
			using ReadOnlyMemoryStream stream = new ReadOnlyMemoryStream(indexData.ToByteArray());

#pragma warning disable CS0618
			await _storageProvider.WriteExplicitPathAsync(path, stream);
#pragma warning restore CS0618
		}

		/// <inheritdoc/>
		public async Task<LogChunkData?> ReadChunkAsync(LogId logId, long offset, int lineIndex)
		{
			_logger.LogDebug("Reading log {LogId} chunk offset {Offset} from persistent storage", logId, offset);

			string path = $"{logId}/offset_{offset}";
			ReadOnlyMemory<byte> data = await _storageProvider.ReadBytesAsync(path);

			MemoryReader reader = new MemoryReader(data);
			LogChunkData chunkData = reader.ReadLogChunkData(offset, lineIndex);

			if (reader.RemainingMemory.Length > 0)
			{
				throw new Exception($"Serialization of persistent chunk {path} is not at expected offset ({reader.RemainingMemory.Length} bytes remaining)");
			}

			return chunkData;
		}

		/// <inheritdoc/>
		public async Task WriteChunkAsync(LogId logId, long offset, LogChunkData chunkData)
		{
			_logger.LogDebug("Writing log {LogId} chunk offset {Offset} to persistent storage", logId, offset);

			string path = $"{logId}/offset_{offset}";
			byte[] data = new byte[chunkData.GetSerializedSize(_logger)];
			MemoryWriter writer = new MemoryWriter(data);
			writer.WriteLogChunkData(chunkData, _logger);
			writer.CheckEmpty();

			using ReadOnlyMemoryStream stream = new ReadOnlyMemoryStream(data);
#pragma warning disable CS0618
			await _storageProvider.WriteExplicitPathAsync(path, stream);
#pragma warning restore CS0618
		}
	}
}
