// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers.Text;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage.Bundles.V1;
using EpicGames.Horde.Storage.Bundles.V2;

namespace EpicGames.Horde.Storage.Bundles
{
	/// <summary>
	/// Base class for packet handles
	/// </summary>
	public abstract class BundleHandle : BlobHandle
	{
	}

	/// <summary>
	/// Generic flushed bundle handle; can be either V1 or V2 format.
	/// </summary>
	class FlushedBundleHandle : BundleHandle
	{
		readonly BundleStorageClient _storageClient;
		readonly IBlobHandle _inner;

		/// <inheritdoc/>
		public override IBlobHandle? Outer => _inner.Outer;

		/// <summary>
		/// Constructor
		/// </summary>
		public FlushedBundleHandle(BundleStorageClient storageClient, IBlobHandle inner)
		{
			_storageClient = storageClient;
			_inner = inner;
		}

		/// <inheritdoc/>
		public override ValueTask FlushAsync(CancellationToken cancellationToken = default) => _inner.FlushAsync(cancellationToken);

		/// <inheritdoc/>
		public override Task<Stream> OpenBodyAsync(int offset = 0, int? length = null, CancellationToken cancellationToken = default)
			=> _inner.OpenBodyAsync(offset, length, cancellationToken);

		/// <inheritdoc/>
		public override async ValueTask<BlobData> ReadBlobDataAsync(CancellationToken cancellationToken = default)
		{
			IReadOnlyMemoryOwner<byte> data = await _inner.ReadBodyAsync(cancellationToken);

			List<BlobLocator> importLocators = ReadImportsFromData(data.Memory).ToList();
			List<IBlobHandle> importHandles = importLocators.ConvertAll(x => _storageClient.CreateBlobHandle(x));

			return new BlobDataWithOwner(Bundle.BlobType, data.Memory, importHandles, data);
		}

		IEnumerable<BlobLocator> ReadImportsFromData(ReadOnlyMemory<byte> data)
		{
			BundleSignature signature = Bundle.ReadSignature(data.Span);
			if (signature.Version <= BundleVersion.LatestV1)
			{
				return ReadImportsFromDataV1(data);
			}
			else if (signature.Version <= BundleVersion.LatestV2)
			{
				return ReadImportsFromDataV2(data);
			}
			else
			{
				throw new InvalidOperationException($"Unsupported bundle version {(int)signature.Version}");
			}
		}

		static IEnumerable<BlobLocator> ReadImportsFromDataV1(ReadOnlyMemory<byte> data)
		{
			BundleHeader header = BundleHeader.Read(data);
			return header.Imports.Select(x => x.BaseLocator);
		}

		IEnumerable<BlobLocator> ReadImportsFromDataV2(ReadOnlyMemory<byte> data)
		{
			HashSet<BlobLocator> locators = new HashSet<BlobLocator>();
			while (data.Length > 0)
			{
				BundleSignature signature = Bundle.ReadSignature(data.Span);

				using IRefCountedHandle<Bundles.V2.Packet> packet = Bundles.V2.Packet.Decode(data, _storageClient.Cache.Allocator);
				for (int idx = 0; idx < packet.Target.GetImportCount(); idx++)
				{
					PacketImport import = packet.Target.GetImport(idx);
					if (import.BaseIdx == -1)
					{
						locators.Add(new BlobLocator(import.Fragment.Clone()));
					}
				}

				data = data.Slice(signature.HeaderLength);
			}
			return locators;
		}

		/// <inheritdoc/>
		public override bool TryAppendIdentifier(Utf8StringBuilder builder)
			=> _inner.TryAppendIdentifier(builder);

		/// <inheritdoc/>
		public override IBlobHandle GetFragmentHandle(ReadOnlySpan<byte> fragment)
		{
			int exportIdx;
			if (Utf8Parser.TryParse(fragment, out exportIdx, out int numBytesRead) && numBytesRead == fragment.Length)
			{
				return new Bundles.V1.FlushedNodeHandle(_storageClient.BundleReader, _inner.GetLocator(), this, exportIdx);
			}

			int ampIdx = fragment.IndexOf((byte)'&');
			if (ampIdx == -1)
			{
				return new Bundles.V2.FlushedPacketHandle(_storageClient, this, fragment, _storageClient.Cache);
			}
			else
			{
				return new Bundles.V2.FlushedPacketHandle(_storageClient, this, fragment.Slice(0, ampIdx), _storageClient.Cache).GetFragmentHandle(fragment.Slice(ampIdx + 1));
			}
		}

		/// <inheritdoc/>
		public override bool Equals(object? obj) => obj is FlushedBundleHandle other && _inner.Equals(other._inner);

		/// <inheritdoc/>
		public override int GetHashCode() => _inner.GetHashCode();
	}
}
