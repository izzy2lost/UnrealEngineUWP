// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Buffers.Text;
using System.IO;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage.Bundles.V2
{
	/// <summary>
	/// Handle to an packet within a bundle. 
	/// </summary>
	public class PacketHandle : IBlobHandle
	{
		static readonly Utf8String s_fragmentPrefix = new Utf8String("pkt=");

		readonly IStorageClient _storageClient;
		readonly IBlobHandle _outer;
		readonly int _packetOffset;
		readonly int _packetLength;
		readonly BundleCache _cache;

		/// <inheritdoc/>
		public IBlobHandle? Outer => _outer;

		/// <summary>
		/// Constructor
		/// </summary>
		public PacketHandle(IStorageClient storageClient, IBlobHandle outer, int packetOffset, int packetLength, BundleCache cache)
		{
			_storageClient = storageClient;

			_outer = outer;
			_packetOffset = packetOffset;
			_packetLength = packetLength;
			_cache = cache;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public PacketHandle(IStorageClient storageClient, IBlobHandle outer, ReadOnlySpan<byte> fragment, BundleCache cache)
		{
			_storageClient = storageClient;

			_outer = outer;
			_cache = cache;

			if (!TryParse(fragment, out _packetOffset, out _packetLength))
			{
				throw new FormatException($"Cannot parse fragment {Encoding.UTF8.GetString(fragment)} relative to {outer}");
			}
		}

		/// <summary>
		/// Parse a fragment containing an offset and length
		/// </summary>
		static bool TryParse(ReadOnlySpan<byte> fragment, out int packetOffset, out int packetLength)
		{
			if (!fragment.StartsWith(s_fragmentPrefix.Span))
			{
				packetOffset = packetLength = 0;
				return false;
			}

			fragment = fragment.Slice(s_fragmentPrefix.Length);
			if (!Utf8Parser.TryParse(fragment, out packetOffset, out int numBytesRead))
			{
				packetOffset = packetLength = 0;
				return false;
			}

			fragment = fragment[numBytesRead..];
			if (fragment.Length == 0 || fragment[0] != (byte)',')
			{
				packetOffset = packetLength = 0;
				return false;
			}

			fragment = fragment[1..];
			if (!Utf8Parser.TryParse(fragment, out packetLength, out numBytesRead) || numBytesRead != fragment.Length)
			{
				packetOffset = packetLength = 0;
				return false;
			}

			return true;
		}

		/// <inheritdoc/>
		public ValueTask FlushAsync(CancellationToken cancellationToken = default) => default;

		/// <inheritdoc/>
		public async ValueTask<BlobData> ReadAsync(CancellationToken cancellationToken = default)
		{
			try
			{
				using IRefCountedHandle<PacketReader> packetReaderHandle = await GetPacketReaderAsync(cancellationToken);
				return packetReaderHandle.Target.Read();
			}
			catch (Exception ex)
			{
				BlobLocator locator = this.GetLocator();
				throw new StorageException($"Unable to read {locator}: {ex.Message}", ex);
			}
		}

		/// <summary>
		/// Reads an export from this packet
		/// </summary>
		/// <param name="exportIdx">Index of the export</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async ValueTask<BlobData> ReadExportAsync(int exportIdx, CancellationToken cancellationToken = default)
		{
			try
			{
				using IRefCountedHandle<PacketReader> packetReaderHandle = await GetPacketReaderAsync(cancellationToken);
				return packetReaderHandle.Target.ReadExport(exportIdx);
			}
			catch (Exception ex)
			{
				BlobLocator locator = this.GetLocator();
				throw new StorageException($"Unable to read {locator}: {ex.Message}", ex);
			}
		}

		/// <summary>
		/// Reads an export body from this packet
		/// </summary>
		/// <param name="exportIdx">Index of the export</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async ValueTask<IReadOnlyMemoryOwner<byte>> ReadExportBodyAsync(int exportIdx, CancellationToken cancellationToken = default)
		{
			try
			{
				using IRefCountedHandle<PacketReader> packetReaderHandle = await GetPacketReaderAsync(cancellationToken);
				return packetReaderHandle.Target.ReadExportBody(exportIdx);
			}
			catch (Exception ex)
			{
				BlobLocator locator = this.GetLocator();
				throw new StorageException($"Unable to read {locator}: {ex.Message}", ex);
			}
		}

		/// <inheritdoc/>
		public bool TryAppendIdentifier(Utf8StringBuilder builder)
		{
			AppendIdentifier(builder, _packetOffset, _packetLength);
			return true;
		}

		/// <summary>
		/// Appends an identifier for a packet to the given buffer
		/// </summary>
		public static void AppendIdentifier(Utf8StringBuilder builder, int packetOffset, int packetLength)
		{
			builder.Append(s_fragmentPrefix);
			builder.Append(packetOffset);
			builder.Append((byte)',');
			builder.Append(packetLength);
		}

		/// <inheritdoc/>
		public IBlobHandle GetFragmentHandle(ReadOnlySpan<byte> fragment)
			=> new ExportHandle(this, fragment);

		/// <inheritdoc/>
		public override bool Equals(object? obj)
			=> obj is PacketHandle other && _outer.Equals(other._outer) && _packetOffset == other._packetOffset;

		/// <inheritdoc/>
		public override int GetHashCode()
			=> HashCode.Combine(_outer, _packetOffset);

		#region Packet reader access

		record struct PacketReaderCacheKey(IBlobHandle Bundle, int Offset);
		record struct EncodedPacketCacheKey(IBlobHandle Bundle, int Offset);

		async ValueTask<IRefCountedHandle<PacketReader>> GetPacketReaderAsync(CancellationToken cancellationToken = default)
		{
			PacketReaderCacheKey cacheKey = new PacketReaderCacheKey(_outer, _packetOffset);
			return await _cache.FindOrAddAsync(cacheKey, CreatePacketReaderAsync, cancellationToken);
		}

		async Task<PacketReader> CreatePacketReaderAsync(PacketReaderCacheKey cacheKey, CancellationToken cancellationToken)
		{
			using IRefCountedHandle<IReadOnlyMemoryOwner<byte>> encodedData = await ReadEncodedPacketAsync(cancellationToken);
			IRefCountedHandle<Packet> packet = Packet.Decode(encodedData.Target.Memory, _cache.Allocator);
			return new PacketReader(_storageClient, _cache, _outer, this, packet.Target, packet);
		}

		async ValueTask<IRefCountedHandle<IReadOnlyMemoryOwner<byte>>> ReadEncodedPacketAsync(CancellationToken cancellationToken)
		{
			EncodedPacketCacheKey encodedPacketCacheKey = new EncodedPacketCacheKey(_outer, _packetOffset);
			return await _cache.FindOrAddAsync(encodedPacketCacheKey, ReadEncodedPacketInternalAsync, cancellationToken);
		}

		async Task<IReadOnlyMemoryOwner<byte>> ReadEncodedPacketInternalAsync(EncodedPacketCacheKey key, CancellationToken cancellationToken)
		{
			IMemoryOwner<byte>? leadingPacket = null;
			IMemoryOwner<byte>? trailingPacket = null;
			try
			{
				// Read more data than was requested so we can add additional packets to the cache
				int readLength = Math.Max(_packetLength, 512 * 1024);
				using Stream stream = await _outer.OpenBodyAsync(_packetOffset, readLength, cancellationToken);

				// Read the first packet
				leadingPacket = _cache.Allocate(_packetLength);
				Memory<byte> memory = leadingPacket.Memory.Slice(0, _packetLength);
				await stream.ReadFixedLengthBytesAsync(memory, cancellationToken);

				// Read any other packets in the same stream
				byte[] header = new byte[Bundle.SignatureLength];
				for (int readOffset = _packetLength; readOffset + Bundle.SignatureLength < readLength;)
				{
					int readBytes = await stream.ReadGreedyAsync(header, cancellationToken);
					if (readBytes < header.Length)
					{
						break;
					}

					BundleSignature signature = Bundle.ReadSignature(header);
					if (readOffset + signature.HeaderLength >= readLength)
					{
						break;
					}

					trailingPacket = _cache.Allocate(signature.HeaderLength);
					header.CopyTo(trailingPacket.Memory);
					memory = trailingPacket.Memory.Slice(Bundle.SignatureLength, signature.HeaderLength - Bundle.SignatureLength);
					await stream.ReadFixedLengthBytesAsync(memory, cancellationToken);

					EncodedPacketCacheKey trailingKey = new EncodedPacketCacheKey(key.Bundle, _packetOffset + readOffset);
					if (!_cache.TryAdd(trailingKey, () => ReadOnlyMemoryOwner.Create<byte>(memory, trailingPacket)))
					{
						trailingPacket.Dispose();
					}
					trailingPacket = null;

					readOffset += signature.HeaderLength;
				}
			}
			catch
			{
				trailingPacket?.Dispose();
				leadingPacket?.Dispose();
				throw;
			}
			return ReadOnlyMemoryOwner.Create<byte>(leadingPacket.Memory.Slice(0, _packetLength), leadingPacket);
		}

		#endregion
	}
}
