// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
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
		class PendingExportHandle : IBlobHandle
		{
			readonly PendingPacketHandle _packet;
			readonly int _exportIdx;

			readonly BlobType _type;
			readonly IBlobHandle[] _imports;
			readonly AliasInfo[] _aliases;
		
			public IBlobHandle? Outer => _packet;

			public IReadOnlyList<AliasInfo> Aliases => _aliases;

			public PendingExportHandle(PendingPacketHandle packet, int exportIdx, BlobType type, IBlobHandle[] imports, AliasInfo[] aliases)
			{
				_packet = packet;
				_exportIdx = exportIdx;
				_type = type;
				_imports = imports;
				_aliases = aliases;
			}

			public ValueTask FlushAsync(CancellationToken cancellationToken = default) => _packet.FlushAsync(cancellationToken);

			public async ValueTask<BlobData> ReadAsync(CancellationToken cancellationToken = default)
			{
				IReadOnlyMemoryOwner<byte> body = await _packet.ReadExportBodyAsync(_exportIdx, cancellationToken);
				return new BlobDataWithOwner(_type, body.Memory, _imports, body);
			}

			public bool TryAppendIdentifier(Utf8StringBuilder builder)
			{
				ExportHandle.AppendIdentifier(builder, _exportIdx);
				return true;
			}
		}

		// Packet that is still being built, but may be redirected to a flushed packet
		class PendingPacketHandle : IBlobHandle, IDisposable
		{
			readonly object _lockObject = new object();
			readonly PendingBundleHandle _bundle;

			PacketHandle? _flushedHandle;
			List<PendingExportHandle>? _pendingExports;

			Packet? _packet;
			PacketWriter? _packetWriter;
			int _packetOffset;
			int _packetLength;

			public IBlobHandle? Outer => _bundle;

			public Packet? Packet => _packet;

			public PendingPacketHandle(PendingBundleHandle bundle, IMemoryAllocator<byte> allocator)
			{
				_bundle = bundle;
				_packetWriter = new PacketWriter(_bundle, this, allocator, _lockObject);
			}

			public void Dispose()
			{
				if (_packetWriter != null)
				{
					_packetWriter.Dispose();
					_packetWriter = null;
				}
			}

			public ValueTask FlushAsync(CancellationToken cancellationToken = default) => _bundle.FlushAsync(cancellationToken);

			public bool IsEmpty() 
				=> _pendingExports!.Count == 0;

			public int GetLength()
				=> _packetWriter!.Length;

			public Memory<byte> GetOutputBuffer(int usedSize, int desiredSize)
				=> _packetWriter!.GetOutputBuffer(usedSize, desiredSize);

			public PendingExportHandle CompleteExport(BlobType type, int size, IReadOnlyList<IBlobHandle> references, IReadOnlyList<AliasInfo> aliases)
			{
				int exportIdx = _packetWriter!.CompleteExport(size, type, references);

				PendingExportHandle exportHandle = new PendingExportHandle(this, exportIdx, type, references.ToArray(), aliases.ToArray());
				_pendingExports!.Add(exportHandle);

				return exportHandle;
			}

			public void CompletePacket(BundleCompressionFormat compressionFormat, IMemoryWriter writer)
			{
				Debug.Assert(_packet == null);
				_packet = _packetWriter!.CompletePacket();

				_packetOffset = writer.Length;
				_packet.Encode(compressionFormat, writer);
				_packetLength = writer.Length - _packetOffset;
			}

			public async ValueTask CompleteBundleAsync(IStorageClient storageClient, IBlobHandle bundleHandle, BundleCache cache, CancellationToken cancellationToken)
			{
				Debug.Assert(_packetWriter != null);

				lock (_lockObject)
				{
					_flushedHandle = new PacketHandle(storageClient, bundleHandle, _packetOffset, _packetLength, cache);
					_packet = null;

					_packetWriter!.Dispose();
					_packetWriter = null;
				}

				for(int exportIdx = 0; exportIdx < _pendingExports!.Count; exportIdx++)
				{
					PendingExportHandle pendingExport = _pendingExports[exportIdx];
					foreach (AliasInfo alias in pendingExport.Aliases)
					{
						ExportHandle exportHandle = new ExportHandle(_flushedHandle, exportIdx);
						await storageClient.AddAliasAsync(alias.Name, exportHandle, alias.Rank, alias.Data, cancellationToken);
					}
				}

				_pendingExports = null;
			}

			public ValueTask<BlobData> ReadAsync(CancellationToken cancellationToken = default)
			{
				lock (_lockObject)
				{
					if (_packetWriter != null)
					{
						throw new NotSupportedException("Reading pending packets is not currently supported.");
					}
				}
				return _flushedHandle!.ReadAsync(cancellationToken);
			}

			public async ValueTask<IReadOnlyMemoryOwner<byte>> ReadExportBodyAsync(int exportIdx, CancellationToken cancellationToken = default)
			{
				lock (_lockObject)
				{
					if (_packetWriter != null)
					{
						return _packetWriter.GetExportData(exportIdx);
					}
				}

				return await _flushedHandle!.ReadExportBodyAsync(exportIdx, cancellationToken);
			}

			public bool TryAppendIdentifier(Utf8StringBuilder builder)
			{
				if (_flushedHandle != null)
				{
					return _flushedHandle.TryAppendIdentifier(builder);
				}
				else if (_packet != null)
				{
					PacketHandle.AppendIdentifier(builder, _packetOffset, _packetLength);
					return true;
				}
				else
				{
					return false;
				}
			}
		}

		// Fragment of a bundle that needs to be written to storage.
		class PendingBundleHandle : IBlobHandle, IDisposable
		{
			readonly IStorageClient _storageClient;
			readonly string? _basePath;
			readonly BundleCache _cache;
			readonly BundleOptions _options;

			IBlobHandle? _flushedHandle;
#pragma warning disable CA2213
			PendingPacketHandle? _currentPacket;
			List<PendingPacketHandle>? _pendingPackets = new List<PendingPacketHandle>();
#pragma warning restore CA2213
			int _length;
			RefCountedMemoryWriter _writer;

			/// <inheritdoc/>
			public IBlobHandle? Outer => null;

			/// <summary>
			/// Uncompressed length of this bundle
			/// </summary>
			public int Length => _length;

			public PendingBundleHandle(IStorageClient storageClient, string? basePath, BundleCache cache, BundleOptions options)
			{
				_storageClient = storageClient;
				_basePath = basePath;
				_cache = cache;
				_options = options;

				_currentPacket = new PendingPacketHandle(this, cache.Allocator);
				_writer = new RefCountedMemoryWriter(_cache.Allocator, 65536);
			}

			/// <inheritdoc/>
			public void Dispose()
			{
				if (_currentPacket != null)
				{
					_currentPacket.Dispose();
					_currentPacket = null;
				}
				if (_pendingPackets != null)
				{
					foreach (PendingPacketHandle pendingPacket in _pendingPackets)
					{
						pendingPacket.Dispose();
					}
					_pendingPackets = null;
				}
				if (_writer != null)
				{
					_writer.Dispose();
					_writer = null!;
				}
			}

			public Memory<byte> GetOutputBuffer(int usedSize, int desiredSize)
				=> _currentPacket!.GetOutputBuffer(usedSize, desiredSize);

			public PendingExportHandle CompleteExport(BlobType type, int size, IReadOnlyList<IBlobHandle> references, IReadOnlyList<AliasInfo> aliases)
			{
				PendingExportHandle exportHandle = _currentPacket!.CompleteExport(type, size, references, aliases);
				if (_currentPacket.GetLength() > Math.Min(_options.MinCompressionPacketSize, _options.MaxBlobSize))
				{
					CompletePacket();
					_currentPacket = new PendingPacketHandle(this, _cache.Allocator);
				}
				return exportHandle;
			}

			void CompletePacket()
			{
				_currentPacket!.CompletePacket(_options.CompressionFormat, _writer);

				_length += _currentPacket.Packet!.Length;
				_pendingPackets!.Add(_currentPacket);

				_currentPacket = null;
			}

			// Write this bundle to storage
			public async ValueTask FlushAsync(CancellationToken cancellationToken = default)
			{
				if (_currentPacket == null)
				{
					return;
				}
				if (!_currentPacket!.IsEmpty())
				{
					CompletePacket();
				}
				if (_pendingPackets!.Count == 0)
				{
					return;
				}

				// Find all the other bundles that are referenced
				HashSet<BlobLocator> references = new HashSet<BlobLocator>();
				foreach (PendingPacketHandle pendingPacket in _pendingPackets)
				{
					for (int importIdx = 0; importIdx < pendingPacket.Packet!.GetImportCount(); importIdx++)
					{
						PacketImport import = pendingPacket.Packet.GetImport(importIdx);
						if (import.BaseIdx == -1)
						{
							references.Add(new BlobLocator(import.Fragment));
						}
					}
				}

				// TODO: put all the encoded packets into the cache

				// Write the bundle data
				using (ReadOnlySequenceStream stream = new ReadOnlySequenceStream(_writer.AsSequence()))
				{
					IBlobHandle[] referencedHandles = references.Select(x => _storageClient.CreateBlobHandle(x)).ToArray(); 
					_flushedHandle = await _storageClient.WriteBlobAsync(Bundle.BlobType, stream, referencedHandles, _basePath, cancellationToken);
				}

				// Update all the packets to point to the flushed bundle
				for (int packetIdx = 0; packetIdx < _pendingPackets.Count; packetIdx++)
				{
					await _pendingPackets[packetIdx].CompleteBundleAsync(_storageClient, _flushedHandle, _cache, cancellationToken);
				}

				// Clear out all the pending packets
				_pendingPackets = null;
			}

			/// <inheritdoc/>
			public ValueTask<BlobData> ReadAsync(CancellationToken cancellationToken)
				=> GetFlushedHandle().ReadAsync(cancellationToken);

			/// <inheritdoc/>
			public ValueTask<IReadOnlyMemoryOwner<byte>> ReadBodyAsync(int offset, int? length, CancellationToken cancellationToken = default)
				=> GetFlushedHandle().ReadBodyAsync(offset, length, cancellationToken);

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
