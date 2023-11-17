// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage.Bundles.V2
{
	/// <summary>
	/// Implements the primary storage writer interface for V2 bundles. Writes exports into packets, and flushes them to storage in bundles.
	/// </summary>
	public sealed class BundleWriter : IStorageWriter
	{
		// An export that has been written and is waiting to be flushed to disk
		internal sealed class PendingExportHandle : IBlobHandle
		{
			readonly PendingPacketHandle _packet;
			readonly int _exportIdx;

			public IBlobHandle? Outer => _packet;

			public PendingExportHandle(PendingPacketHandle packet, int exportIdx)
			{
				_packet = packet;
				_exportIdx = exportIdx;
			}

			public ValueTask FlushAsync(CancellationToken cancellationToken = default) 
				=> _packet.FlushAsync(cancellationToken);

			public ValueTask<BlobData> ReadAsync(CancellationToken cancellationToken = default)
				=> _packet.ReadExportAsync(_exportIdx, cancellationToken);

			public bool TryAppendIdentifier(Utf8StringBuilder builder)
			{
				ExportHandle.AppendIdentifier(builder, _exportIdx);
				return true;
			}
		}

		// Packet that is still being built, but may be redirected to a flushed packet
		internal sealed class PendingPacketHandle : IBlobHandle
		{
			readonly PendingBundleHandle _bundle;
			PacketHandle? _flushedHandle;

			public IBlobHandle? Outer => _flushedHandle?.Outer ?? _bundle;
			public IBlobHandle? FlushedHandle => _flushedHandle;

			public PendingPacketHandle(PendingBundleHandle bundle) => _bundle = bundle;

			public ValueTask FlushAsync(CancellationToken cancellationToken = default) 
				=> _bundle.FlushAsync(cancellationToken);

			public void CompletePacket(PacketHandle flushedHandle)
				=> _flushedHandle = flushedHandle;

			public ValueTask<BlobData> ReadAsync(CancellationToken cancellationToken = default)
			{
				lock (_bundle.LockObject)
				{
					if (_flushedHandle == null)
					{
						throw new NotSupportedException("Reading pending packets is not currently supported.");
					}
				}
				return _flushedHandle!.ReadAsync(cancellationToken);
			}

			public async ValueTask<BlobData> ReadExportAsync(int exportIdx, CancellationToken cancellationToken = default)
			{
				lock (_bundle.LockObject)
				{
					if (_flushedHandle == null)
					{
						return _bundle.GetPendingExport(exportIdx);
					}
				}
				return await _flushedHandle!.ReadExportAsync(exportIdx, cancellationToken);
			}

			public bool TryAppendIdentifier(Utf8StringBuilder builder)
				=> _flushedHandle?.TryAppendIdentifier(builder) ?? false;
		}

		// Fragment of a bundle that needs to be written to storage.
		internal sealed class PendingBundleHandle : IBlobHandle, IDisposable
		{
			readonly object _lockObject = new object();

			readonly IStorageClient _storageClient;
			readonly string? _basePath;
			readonly BundleCache _cache;
			readonly BundleOptions _options;

			IBlobHandle? _flushedHandle;

			PacketWriter? _packetWriter;
			PendingPacketHandle? _packetHandle;
			List<IBlobHandle>? _bundleReferences;
			List<(PendingExportHandle, AliasInfo)>? _pendingExportAliases;
			RefCountedMemoryWriter? _encodedPacketWriter;

			/// <summary>
			/// Object used for locking access to this bundle's state
			/// </summary>
			public object LockObject => _lockObject;

			public IBlobHandle? Outer => null;
			public IBlobHandle? FlushedHandle => _flushedHandle;

			/// <summary>
			/// Compressed length of this bundle
			/// </summary>
			public int Length => _encodedPacketWriter?.Length ?? throw new InvalidOperationException("Bundle has been flushed");

			public PendingBundleHandle(IStorageClient storageClient, string? basePath, BundleCache cache, BundleOptions options)
			{
				_storageClient = storageClient;
				_basePath = basePath;
				_cache = cache;
				_options = options;

				_bundleReferences = new List<IBlobHandle>();
				_encodedPacketWriter = new RefCountedMemoryWriter(_cache.Allocator, 65536);

				StartPacket();
			}

			public void Dispose() => ReleaseResources();

			void ReleaseResources()
			{
				if (_packetWriter != null)
				{
					_packetWriter.Dispose();
					_packetWriter = null;
				}
				if (_encodedPacketWriter != null)
				{
					_encodedPacketWriter.Dispose();
					_encodedPacketWriter = null;
				}

				// Also clear out any arrays that can be GC'd
				_packetHandle = null;
				_bundleReferences = null;
				_pendingExportAliases = null;
			}

			public Memory<byte> GetOutputBuffer(int usedSize, int desiredSize)
			{
				Debug.Assert(_packetWriter != null);
				return _packetWriter!.GetOutputBuffer(usedSize, desiredSize);
			}

			public BlobData GetPendingExport(int exportIdx)
			{
				Debug.Assert(_packetWriter != null);
				return _packetWriter.GetExport(exportIdx);
			}

			public PendingExportHandle CompleteExport(BlobType type, int size, IReadOnlyList<IBlobHandle> references, IReadOnlyList<AliasInfo> aliases)
			{
				Debug.Assert(_packetWriter != null);
				Debug.Assert(_packetHandle != null);

				int exportIdx = _packetWriter.CompleteExport(size, type, references);
				PendingExportHandle exportHandle = new PendingExportHandle(_packetHandle, exportIdx);

				if (aliases.Count > 0)
				{
					_pendingExportAliases ??= new List<(PendingExportHandle, AliasInfo)>();
					_pendingExportAliases.AddRange(aliases.Select(x => (exportHandle, x)));
				}

				if (_packetWriter.Length > Math.Min(_options.MinCompressionPacketSize, _options.MaxBlobSize))
				{
					FinishPacket();
					StartPacket();
				}

				return exportHandle;
			}

			void StartPacket()
			{
				Debug.Assert(_packetHandle == null);
				Debug.Assert(_packetWriter == null);

				_packetHandle = new PendingPacketHandle(this);
				_packetWriter = new PacketWriter(this, _packetHandle, _cache.Allocator, _lockObject);
			}

			void FinishPacket()
			{
				Debug.Assert(_packetHandle != null);
				Debug.Assert(_packetWriter != null);
				Debug.Assert(_bundleReferences != null);
				Debug.Assert(_encodedPacketWriter != null);

				if (_packetWriter.GetExportCount() > 0)
				{
					int packetOffset = _encodedPacketWriter.Length;
					Packet packet = _packetWriter.CompletePacket();
					packet.Encode(_options.CompressionFormat, _encodedPacketWriter);
					int packetLength = _encodedPacketWriter.Length - packetOffset;

					// Point the packet handle to the encoded data
					lock (_lockObject)
					{
						PacketHandle flushedPacketHandle = new PacketHandle(_storageClient, this, packetOffset, packetLength, _cache);
						_packetHandle.CompletePacket(flushedPacketHandle);
					}

					// Find all the other bundles that are referenced
					for (int importIdx = 0; importIdx < packet.GetImportCount(); importIdx++)
					{
						PacketImport import = packet.GetImport(importIdx);
						if (import.BaseIdx == -1)
						{
							_bundleReferences.Add(_packetWriter.GetImport(importIdx));
						}
					}
				}

				_packetWriter.Dispose();
				_packetWriter = null;

				_packetHandle = null;
			}

			// Write this bundle to storage
			public async ValueTask FlushAsync(CancellationToken cancellationToken = default)
			{
				// Check we haven't already flushed this bundle
				if (_encodedPacketWriter == null)
				{
					return;
				}

				FinishPacket();

				if (_encodedPacketWriter.Length == 0)
				{
					return;
				}

				Debug.Assert(_bundleReferences != null);

				// Write the bundle data
				IBlobHandle flushedHandle;
				using (ReadOnlySequenceStream stream = new ReadOnlySequenceStream(_encodedPacketWriter.AsSequence()))
				{
					flushedHandle = await _storageClient.WriteBlobAsync(Bundle.BlobType, stream, _bundleReferences, _basePath, cancellationToken);
				}

				// Release all the intermediate data
				lock (_lockObject)
				{
					_flushedHandle = flushedHandle;
					ReleaseResources();
				}

				// TODO: put all the encoded packets into the cache using the final handles

				// Add all the aliases
				if (_pendingExportAliases != null)
				{
					foreach ((PendingExportHandle exportHandle, AliasInfo aliasInfo) in _pendingExportAliases)
					{
						await _storageClient.AddAliasAsync(aliasInfo.Name, exportHandle, aliasInfo.Rank, aliasInfo.Data, cancellationToken);
					}
				}
			}

			/// <inheritdoc/>
			public ValueTask<BlobData> ReadAsync(CancellationToken cancellationToken)
				=> GetFlushedHandle().ReadAsync(cancellationToken);

			/// <inheritdoc/>
			public async Task<Stream> OpenBodyAsync(int offset, int? length, CancellationToken cancellationToken = default)
			{
				if (_flushedHandle == null)
				{
					IReadOnlyMemoryOwner<byte> owner = await ReadBodyAsync(offset, length, cancellationToken);
					return owner.AsStream();
				}

				return await _flushedHandle.OpenBodyAsync(offset, length, cancellationToken);
			}

			/// <inheritdoc/>
			public async ValueTask<IReadOnlyMemoryOwner<byte>> ReadBodyAsync(int offset, int? length, CancellationToken cancellationToken = default)
			{
				if (_flushedHandle == null)
				{
					lock (_lockObject)
					{
						if (_flushedHandle == null)
						{
							Debug.Assert(_encodedPacketWriter != null);
							int fetchLength = length ?? (_encodedPacketWriter.Length - offset);

							IRefCountedHandle<ReadOnlyMemory<byte>> handle = _encodedPacketWriter.AsRefCountedMemory(offset, fetchLength);
							return ReadOnlyMemoryOwner.Create(handle.Target, handle);
						}
					}
				}

				return await _flushedHandle.ReadBodyAsync(offset, length, cancellationToken);
			}

			/// <inheritdoc/>
			public bool TryAppendIdentifier(Utf8StringBuilder builder)
				=> _flushedHandle?.TryAppendIdentifier(builder) ?? false;

			IBlobHandle GetFlushedHandle()
				=> _flushedHandle ?? throw new NotSupportedException("Bundle has not yet been flushed");
		}

		readonly IStorageClient _storageClient;
		readonly string? _basePath;
		readonly BundleOptions _options;
		readonly BundleCache _bundleCache;

		PendingBundleHandle _currentBundle;

		/// <summary>
		/// 
		/// </summary>
		public BundleWriter(IStorageClient storageClient, string? basePath, BundleCache bundleCache, BundleOptions options)
		{
			_storageClient = storageClient;
			_basePath = basePath;
			_options = options;
			_bundleCache = bundleCache;

			_currentBundle = new PendingBundleHandle(storageClient, basePath, bundleCache, _options);
		}

		/// <inheritdoc/>
		public async ValueTask DisposeAsync()
		{
			await FlushAsync();
			_currentBundle.Dispose();
		}

		/// <inheritdoc/>
		public async Task FlushAsync(CancellationToken cancellationToken = default)
		{
			await _currentBundle.FlushAsync(cancellationToken);
			_currentBundle.Dispose();
			_currentBundle = new PendingBundleHandle(_storageClient, _basePath, _bundleCache, _options);
		}

		/// <inheritdoc/>
		public IStorageWriter Fork()
			=> new BundleWriter(_storageClient, _basePath, _bundleCache, _options);

		/// <inheritdoc/>
		public Memory<byte> GetOutputBuffer(int usedSize, int desiredSize)
			=> _currentBundle.GetOutputBuffer(usedSize, desiredSize);

		/// <inheritdoc/>
		public async ValueTask<IBlobHandle> WriteBlobAsync(BlobType type, int size, IReadOnlyList<IBlobHandle> references, IReadOnlyList<AliasInfo> aliases, CancellationToken cancellationToken = default)
		{
			PendingExportHandle exportHandle = _currentBundle.CompleteExport(type, size, references, aliases);
			if (_currentBundle.Length > _options.MaxBlobSize)
			{
				await FlushAsync(cancellationToken);
			}
			return exportHandle;
		}
	}
}
