// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Logging;
using Microsoft.CodeAnalysis;
using System.Diagnostics;
using System.IO;
using System.Diagnostics.CodeAnalysis;

namespace EpicGames.Horde.Storage.Bundles
{
	/// <summary>
	/// Writes nodes from bundles in an <see cref="IStorageClient"/> instance.
	/// </summary>
	public class BundleReader
	{
		/// <summary>
		/// Computed information about a bundle
		/// </summary>
		class BundleInfo
		{
			public readonly BundleLocator Locator;
			public readonly BundleHeader Header;
			public readonly int HeaderLength;

			public BundleInfo(BundleLocator locator, BundleHeader header, int headerLength)
			{
				Locator = locator;
				Header = header;
				HeaderLength = headerLength;
			}
		}

		/// <summary>
		/// Bundle header queued to be read
		/// </summary>
		class QueuedHeader
		{
			public readonly BundleLocator Blob;
			public readonly TaskCompletionSource<BundleInfo> CompletionSource = new TaskCompletionSource<BundleInfo>(TaskCreationOptions.RunContinuationsAsynchronously);

			public Utf8String Path => Blob.Path;

			public QueuedHeader(BundleLocator blob)
			{
				Blob = blob;
			}
		}

		/// <summary>
		/// Encoded bundle packet queued to be read
		/// </summary>
		class QueuedPacket
		{
			public readonly BundleInfo Bundle;
			public readonly int PacketIdx;
			public readonly TaskCompletionSource<ReadOnlyMemory<byte>> CompletionSource = new TaskCompletionSource<ReadOnlyMemory<byte>>(TaskCreationOptions.RunContinuationsAsynchronously);

			public Utf8String Path => Bundle.Locator.Path;

			public QueuedPacket(BundleInfo bundle, int packetIdx)
			{
				Bundle = bundle;
				PacketIdx = packetIdx;
			}
		}

		// Size of data to fetch by default. This is larger than the minimum request size to reduce number of reads.
		const int DefaultFetchSize = 15 * 1024 * 1024;

		// When reader is uncached, use a smaller default fetch size
		const int DefaultUncachedFetchSize = 1 * 1024 * 1024;

		readonly BundleStorageClient _store;
		readonly IMemoryCache? _cache;
		readonly ILogger _logger;

		readonly object _queueLock = new object();
		readonly List<QueuedHeader> _queuedHeaders = new List<QueuedHeader>();
		readonly List<QueuedPacket> _queuedPackets = new List<QueuedPacket>();
		readonly Dictionary<string, Task<ReadOnlyMemory<byte>>> _decodeTasks = new Dictionary<string, Task<ReadOnlyMemory<byte>>>(StringComparer.Ordinal);
		Task? _readTask;

		int _numHeaderReads;
		int _numPacketReads;

		/// <summary>
		/// Total number of header reads
		/// </summary>
		public int NumHeaderReads => _numHeaderReads;

		/// <summary>
		/// Total number of packet reads
		/// </summary>
		public int NumPacketReads => _numPacketReads;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="store"></param>
		/// <param name="cache">Cache for data</param>
		/// <param name="logger">Logger for output</param>
		public BundleReader(BundleStorageClient store, IMemoryCache? cache, ILogger logger)
		{
			_store = store;
			_cache = cache;
			_logger = logger;
		}

		#region Cache

		static void AddCachedValue(IMemoryCache? cache, string cacheKey, object value, int size)
		{
			if (cache != null)
			{
				using (ICacheEntry entry = cache.CreateEntry(cacheKey))
				{
					entry.SetValue(value);
					entry.SetSize(size);
				}
			}
		}

		static bool TryGetCachedValue<T>(IMemoryCache? cache, string cacheKey, [MaybeNull, NotNullWhen(true)] out T value)
		{
			if (cache == null)
			{
				value = default;
				return false;
			}
			return cache.TryGetValue(cacheKey, out value);
		}

		static string GetBundleInfoCacheKey(BundleLocator locator) => $"bundle:{locator}";
		static string GetEncodedPacketCacheKey(BundleLocator locator, int packetIdx) => $"encoded-packet:{locator}#{packetIdx}";
		static string GetDecodedPacketCacheKey(BundleLocator locator, int packetIdx) => $"decoded-packet:{locator}#{packetIdx}";

		void AddCachedBundleInfo(BundleLocator locator, BundleInfo bundleInfo) => AddCachedValue(_cache, GetBundleInfoCacheKey(locator), bundleInfo, bundleInfo.HeaderLength);
		bool TryGetCachedBundleInfo(BundleLocator locator, [NotNullWhen(true)] out BundleInfo? bundleInfo) => TryGetCachedValue(_cache, GetBundleInfoCacheKey(locator), out bundleInfo);

		void AddCachedEncodedPacket(BundleLocator locator, int packetIdx, ReadOnlyMemory<byte> data) => AddCachedValue(_cache, GetEncodedPacketCacheKey(locator, packetIdx), data, data.Length);
		bool TryGetCachedEncodedPacket(BundleLocator locator, int packetIdx, out ReadOnlyMemory<byte> data) => TryGetCachedValue(_cache, GetEncodedPacketCacheKey(locator, packetIdx), out data);

		void AddCachedDecodedPacket(BundleLocator locator, int packetIdx, ReadOnlyMemory<byte> data) => AddCachedValue(_cache, GetDecodedPacketCacheKey(locator, packetIdx), data, data.Length);
		bool TryGetCachedDecodedPacket(BundleLocator locator, int packetIdx, out ReadOnlyMemory<byte> data) => TryGetCachedValue(_cache, GetDecodedPacketCacheKey(locator, packetIdx), out data);

		#endregion

		#region Bundles

		/// <summary>
		/// Starts the background task for reading data from the store
		/// </summary>
		void StartReadTask()
		{
			if (_readTask == null)
			{
				_readTask = Task.Run(() => ServiceReadQueueAsync(CancellationToken.None));
			}
		}

		/// <summary>
		/// Dispatches requests in the read queue
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the background task</param>
		async Task ServiceReadQueueAsync(CancellationToken cancellationToken)
		{
			const int MaxConcurrentReads = 4;

			List<(Utf8String Path, Task Task)> currentTasks = new List<(Utf8String, Task)>();
			for (; ; )
			{
				// Start any new reads
				lock (_queueLock)
				{
					while (currentTasks.Count < MaxConcurrentReads)
					{
						HashSet<Utf8String> currentPaths = new HashSet<Utf8String>(currentTasks.Select(x => x.Path));

						// Try to start another header read
						QueuedHeader? queuedHeader = _queuedHeaders.FirstOrDefault(x => !currentPaths.Contains(x.Path));
						if (queuedHeader != null)
						{
							Task task = Task.Run(() => PerformHeaderReadGuardedAsync(queuedHeader, cancellationToken), cancellationToken);
							currentTasks.Add((queuedHeader.Path, task));
							continue;
						}

						// Try to start another packet read
						QueuedPacket? queuedPacket = _queuedPackets.FirstOrDefault(x => !currentPaths.Contains(x.Path));
						if (queuedPacket != null)
						{
							Task task = Task.Run(() => PerformPacketReadGuardedAsync(queuedPacket, cancellationToken), cancellationToken);
							currentTasks.Add((queuedPacket.Path, task));
							continue;
						}

						// If we're not waiting for anything else and there are no more requests, end the task thread.
						if (currentTasks.Count == 0)
						{
							_readTask = null;
							return;
						}

						// Break out of the loop
						break;
					}
				}

				// Wait for any read task to complete
				await Task.WhenAny(currentTasks.Select(x => x.Task));

				// Remove any tasks which are complete
				for (int idx = 0; idx < currentTasks.Count; idx++)
				{
					Task task = currentTasks[idx].Task;
					if (task.IsCompleted)
					{
						if (task.Exception != null)
						{
							_logger.LogError(task.Exception, "Exception while reading from blob {BlobId}.", currentTasks[idx].Path);
						}
						currentTasks.RemoveAt(idx--);
					}
				}
			}
		}

		/// <summary>
		/// Reads a bundle header from the queue
		/// </summary>
		/// <param name="queuedHeader">The header to read</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		async Task PerformHeaderReadGuardedAsync(QueuedHeader queuedHeader, CancellationToken cancellationToken)
		{
			try
			{
				await PerformHeaderReadAsync(queuedHeader, cancellationToken);
			}
			catch (Exception ex)
			{
				queuedHeader.CompletionSource.TrySetException(ex);
			}
		}

		/// <summary>
		/// Reads a bundle header from the queue
		/// </summary>
		/// <param name="queuedHeader">The header to read</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		async Task PerformHeaderReadAsync(QueuedHeader queuedHeader, CancellationToken cancellationToken)
		{
			Interlocked.Increment(ref _numHeaderReads);

			int prefetchSize = _cache != null ? DefaultFetchSize : DefaultUncachedFetchSize;
			for (; ; )
			{
				await using (Stream stream = await _store.OpenAsync(queuedHeader.Blob, 0, prefetchSize, cancellationToken))
				{
					// Read the header data
					byte[] prelude = new byte[BundleHeader.PreludeLength];
					await stream.ReadFixedLengthBytesAsync(prelude, cancellationToken);

					// Make sure we've read enough to hold the header
					int headerSize = BundleHeader.ReadPrelude(prelude);
					if (headerSize > prefetchSize)
					{
						prefetchSize = headerSize;
						continue;
					}

					// Parse the header and construct the bundle info from it
					BundleHeader header = await BundleHeader.ReadAsync(prelude, stream, cancellationToken);

					// Construct the bundle info
					BundleInfo bundleInfo = new BundleInfo(queuedHeader.Blob, header, headerSize);

					List<ReadOnlyMemory<byte>> packets = new List<ReadOnlyMemory<byte>>();
					if (_cache != null)
					{
						// Also add any encoded packets we prefetched
						int packetOffset = headerSize;
						for (int packetIdx = 0; packetIdx < header.Packets.Count; packetIdx++)
						{
							int packetLength = header.Packets[packetIdx].EncodedLength;
							if (packetOffset + packetLength > prefetchSize)
							{
								break;
							}

							byte[] packetData = await ReadPacketAsync(stream, packetLength, cancellationToken);
							AddCachedEncodedPacket(queuedHeader.Blob, packetIdx, packetData);
							packets.Add(packetData);

							packetOffset += packetLength;
						}

						// Add the info to the cache
						AddCachedBundleInfo(queuedHeader.Blob, bundleInfo);
					}

					// Update any packets that now have a cached value
					lock (_queueLock)
					{
						queuedHeader.CompletionSource.TrySetResult(bundleInfo);

						foreach (QueuedPacket queuedPacket in _queuedPackets)
						{
							if (queuedPacket.Path == bundleInfo.Locator.Path && queuedPacket.PacketIdx < packets.Count)
							{
								queuedPacket.CompletionSource.TrySetResult(packets[queuedPacket.PacketIdx]);
							}
						}
					}

					// Remove it from the queue
					lock (_queueLock)
					{
						_queuedHeaders.Remove(queuedHeader);
					}
					break;
				}
			}
		}

		/// <summary>
		/// Reads a packet from storage
		/// </summary>
		/// <param name="queuedPacket">The packet to read</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		async Task PerformPacketReadGuardedAsync(QueuedPacket queuedPacket, CancellationToken cancellationToken)
		{
			try
			{
				await PerformPacketReadAsync(queuedPacket, cancellationToken);
			}
			catch (Exception ex)
			{
				if (!queuedPacket.CompletionSource.TrySetException(ex))
				{
					_logger.LogWarning(ex, "Exception after setting completion source state; existing state: {Status}, new exception: {Ex}", queuedPacket.CompletionSource.Task.Status, ex);
				}
			}
		}

		/// <summary>
		/// Reads a packet from storage
		/// </summary>
		/// <param name="queuedPacket">The packet to read</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		async Task PerformPacketReadAsync(QueuedPacket queuedPacket, CancellationToken cancellationToken)
		{
			Interlocked.Increment(ref _numPacketReads);

			BundleInfo bundleInfo = queuedPacket.Bundle;
			int minPacketIdx = queuedPacket.PacketIdx;

			BundlePacket packet = bundleInfo.Header.Packets[minPacketIdx];
			int readLength = packet.EncodedLength;

			int maxPacketIdx = minPacketIdx + 1;
			if(_cache != null)
			{
				for (; maxPacketIdx < bundleInfo.Header.Packets.Count; maxPacketIdx++)
				{
					int nextReadLength = readLength + bundleInfo.Header.Packets[maxPacketIdx].EncodedLength;
					if (nextReadLength > DefaultFetchSize)
					{
						break;
					}
					readLength = nextReadLength;
				}
			}

			await using (Stream stream = await _store.OpenAsync(bundleInfo.Locator, bundleInfo.HeaderLength + bundleInfo.Header.Packets[minPacketIdx].EncodedOffset, readLength, cancellationToken))
			{
				// Copy all the packets that have been read into separate buffers, so we can cache them indidually.
				ReadOnlyMemory<byte>[] packets = new ReadOnlyMemory<byte>[maxPacketIdx - minPacketIdx];
				for (int idx = minPacketIdx; idx < maxPacketIdx; idx++)
				{
					byte[] data = await ReadPacketAsync(stream, bundleInfo.Header.Packets[idx].EncodedLength, cancellationToken);
					packets[idx - minPacketIdx] = data;
				}

				// Add any complete packets
				if (_cache != null)
				{
					for (int idx = minPacketIdx; idx < maxPacketIdx; idx++)
					{
						ReadOnlyMemory<byte> data = packets[idx - minPacketIdx];
						AddCachedEncodedPacket(bundleInfo.Locator, idx, data);
					}
				}

				// Find all the packets we can mark as complete
				lock (_queueLock)
				{
					for (int idx = 0; idx < _queuedPackets.Count; idx++)
					{
						QueuedPacket updatePacket = _queuedPackets[idx];
						if (updatePacket.Path == bundleInfo.Locator.Path && (updatePacket.PacketIdx >= minPacketIdx && updatePacket.PacketIdx < maxPacketIdx))
						{
							ReadOnlyMemory<byte> data = packets[updatePacket.PacketIdx - minPacketIdx];
							updatePacket.CompletionSource.TrySetResult(data);
							_queuedPackets.RemoveAt(idx--);
						}
					}
				}
			}
		}

		static async Task<byte[]> ReadPacketAsync(Stream stream, int packetSize, CancellationToken cancellationToken)
		{
			byte[] packet = new byte[packetSize];
			await stream.ReadFixedLengthBytesAsync(packet, cancellationToken);
			return packet;
		}

		/// <summary>
		/// Reads a bundle header from the given blob locator, or retrieves it from the cache
		/// </summary>
		/// <param name="locator"></param>
		/// <param name="cancellationToken"></param>
		/// <returns></returns>
		public async Task<BundleHeader> ReadBundleHeaderAsync(BundleLocator locator, CancellationToken cancellationToken = default)
		{
			BundleInfo info = await GetBundleInfoAsync(locator, cancellationToken);
			return info.Header;
		}

		/// <summary>
		/// Reads the header and structural metadata about the bundle
		/// </summary>
		/// <param name="locator">The bundle location</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about the bundle</returns>
		async ValueTask<BundleInfo> GetBundleInfoAsync(BundleLocator locator, CancellationToken cancellationToken = default)
		{
			Debug.Assert(locator.IsValid());

			BundleInfo? bundleInfo;
			if (TryGetCachedBundleInfo(locator, out bundleInfo))
			{
				return bundleInfo;
			}

			QueuedHeader? queuedHeader;
			lock (_queueLock)
			{
				// Check the cache again inside lock scope to avoid races
				if (TryGetCachedBundleInfo(locator, out bundleInfo))
				{
					return bundleInfo;
				}

				// Find or start the read
				queuedHeader = _queuedHeaders.FirstOrDefault(x => x.Blob == locator);
				if (queuedHeader == null)
				{
					queuedHeader = new QueuedHeader(locator);
					_queuedHeaders.Add(queuedHeader);
					StartReadTask();
				}
			}
			return await queuedHeader.CompletionSource.Task.WaitAsync(cancellationToken);
		}

		/// <summary>
		/// Gets a decoded block from the store
		/// </summary>
		/// <param name="bundleInfo">Information about the bundle</param>
		/// <param name="packetIdx">Index of the packet</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The decoded data</returns>
		async ValueTask<ReadOnlyMemory<byte>> ReadBundlePacketAsync(BundleInfo bundleInfo, int packetIdx, CancellationToken cancellationToken)
		{
			if (packetIdx < 0 || packetIdx >= bundleInfo.Header.Packets.Count)
			{
				throw new ArgumentException("Packet index is out of range", nameof(packetIdx));
			}

			ReadOnlyMemory<byte> decodedPacket;
			if (TryGetCachedDecodedPacket(bundleInfo.Locator, packetIdx, out decodedPacket))
			{
				return decodedPacket;
			}

			Task<ReadOnlyMemory<byte>>? decodeTask;
			lock (_queueLock)
			{
				// Query the cache again, to eliminate races between cache checks and decode tasks finishing.
				if (TryGetCachedDecodedPacket(bundleInfo.Locator, packetIdx, out decodedPacket))
				{
					return decodedPacket;
				}

				// Create an async task to read the data
				string decodedCacheKey = GetDecodedPacketCacheKey(bundleInfo.Locator, packetIdx);
				if (!_decodeTasks.TryGetValue(decodedCacheKey, out decodeTask))
				{
					decodeTask = Task.Run(() => ReadAndDecodePacketAsync(bundleInfo, packetIdx), CancellationToken.None);
					_decodeTasks.Add(decodedCacheKey, decodeTask);
				}
			}
			return await decodeTask.WaitAsync(cancellationToken);
		}

		/// <summary>
		/// Reads and decodes a packet from a bundle
		/// </summary>
		/// <param name="bundleInfo">Bundle to read from</param>
		/// <param name="packetIdx">Index of the packet to return</param>
		/// <returns>The decoded packet data</returns>
		async Task<ReadOnlyMemory<byte>> ReadAndDecodePacketAsync(BundleInfo bundleInfo, int packetIdx)
		{
			ReadOnlyMemory<byte> encodedPacket = await ReadEncodedPacketAsync(bundleInfo, packetIdx);

			BundlePacket packet = bundleInfo.Header.Packets[packetIdx];
			byte[] decodedPacket = new byte[packet.DecodedLength];

			BundleData.Decompress(packet.CompressionFormat, encodedPacket, decodedPacket);
			AddCachedDecodedPacket(bundleInfo.Locator, packetIdx, decodedPacket);

			lock (_queueLock)
			{
				string decodedCacheKey = GetDecodedPacketCacheKey(bundleInfo.Locator, packetIdx);
				_decodeTasks.Remove(decodedCacheKey);
			}

			return decodedPacket;
		}

		/// <summary>
		/// Reads an encoded packet from a bundle
		/// </summary>
		/// <param name="bundleInfo">Bundle to read from</param>
		/// <param name="packetIdx">Index of the packet to return</param>
		/// <returns>The encoded packet data</returns>
		async ValueTask<ReadOnlyMemory<byte>> ReadEncodedPacketAsync(BundleInfo bundleInfo, int packetIdx)
		{
			if (packetIdx < 0 || packetIdx >= bundleInfo.Header.Packets.Count)
			{
				throw new ArgumentException("Packet index is out of range", nameof(packetIdx));
			}

			ReadOnlyMemory<byte> encodedPacket;
			if (TryGetCachedEncodedPacket(bundleInfo.Locator, packetIdx, out encodedPacket))
			{
				return encodedPacket;
			}

			QueuedPacket? queuedPacket;
			lock (_queueLock)
			{
				// Query the cache again, to eliminate races between cache checks and decode tasks finishing.
				if (TryGetCachedEncodedPacket(bundleInfo.Locator, packetIdx, out encodedPacket))
				{
					return encodedPacket;
				}

				// Add a read to the queue
				queuedPacket = _queuedPackets.FirstOrDefault(x => x.Bundle.Locator == bundleInfo.Locator && x.PacketIdx == packetIdx);
				if (queuedPacket == null)
				{
					queuedPacket = new QueuedPacket(bundleInfo, packetIdx);
					_queuedPackets.Add(queuedPacket);
					StartReadTask();
				}
			}
			return await queuedPacket.CompletionSource.Task;
		}

		#endregion

		/// <summary>
		/// Reads a node from a bundle
		/// </summary>
		/// <param name="locator">Locator for the node</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Node data read from the given bundle</returns>
		public async ValueTask<BlobData> ReadNodeDataAsync(BundleNodeLocator locator, CancellationToken cancellationToken = default)
		{
			BundleInfo bundleInfo = await GetBundleInfoAsync(locator.Blob, cancellationToken);
			BundleExport export = bundleInfo.Header.Exports[locator.ExportIdx];

			List<BlobHandle> refs = new List<BlobHandle>(export.References.Count);
			foreach (BundleExportRef reference in export.References)
			{
				BundleLocator importBlob;
				if (reference.ImportIdx == -1)
				{
					importBlob = locator.Blob;
				}
				else
				{
					importBlob = bundleInfo.Header.Imports[reference.ImportIdx];
				}
				Debug.Assert(importBlob.IsValid());
				refs.Add(new FlushedNodeHandle(this, new BundleNodeLocator(reference.Hash, importBlob, reference.NodeIdx)));
			}

			ReadOnlyMemory<byte> nodeData = ReadOnlyMemory<byte>.Empty;
			if (export.Length > 0)
			{
				ReadOnlyMemory<byte> packetData = await ReadBundlePacketAsync(bundleInfo, export.Packet, cancellationToken);
				nodeData = packetData.Slice(export.Offset, export.Length);
			}

			BlobType nodeType = bundleInfo.Header.Types[export.TypeIdx];
			return new BlobData(nodeType, export.Hash, nodeData, refs);
		}

		/// <summary>
		/// Reads a node from a bundle
		/// </summary>
		/// <param name="locator">Locator for the node</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Node data read from the given bundle</returns>
		public async ValueTask<Node> ReadNodeAsync(BundleNodeLocator locator, CancellationToken cancellationToken = default)
		{
			BlobData nodeData = await ReadNodeDataAsync(locator, cancellationToken);
			return Node.Deserialize(nodeData);
		}

		/// <summary>
		/// Reads a node from a bundle
		/// </summary>
		/// <param name="locator">Locator for the node</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Node data read from the given bundle</returns>
		public async ValueTask<TNode> ReadNodeAsync<TNode>(BundleNodeLocator locator, CancellationToken cancellationToken = default) where TNode : Node => (TNode)await ReadNodeAsync(locator, cancellationToken);
	}
}
