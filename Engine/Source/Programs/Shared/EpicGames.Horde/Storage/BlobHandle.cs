// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Threading;
using System.Threading.Tasks;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Handle to a node. Can be used to reference nodes that have not been flushed yet.
	/// </summary>
	public abstract class BlobHandle
	{
		/// <summary>
		/// Gets a path to this blob that can be used to describe blob references over the wire.
		/// </summary>
		public BlobLocator GetLocator()
		{
			BlobLocator locator;
			if (!TryGetLocator(out locator))
			{
				throw new InvalidOperationException("Blob has not yet been written to storage");
			}
			return locator;
		}

		/// <summary>
		/// Attempt to get a path for this blob.
		/// </summary>
		/// <param name="locator">Receives the blob path on success.</param>
		/// <returns>True if a path was available, false if the blob has not yet been flushed to storage.</returns>
		public abstract bool TryGetLocator([NotNullWhen(true)] out BlobLocator locator);

		/// <summary>
		/// Gets the type of this blob
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public virtual async ValueTask<BlobType> GetTypeAsync(CancellationToken cancellationToken = default)
		{
			using BlobData data = await ReadAsync(cancellationToken);
			return data.Type;
		}

		/// <summary>
		/// Gets the outward references from this blob
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public virtual async ValueTask<IReadOnlyList<BlobHandle>> GetRefsAsync(CancellationToken cancellationToken = default)
		{
			using BlobData data = await ReadAsync(cancellationToken);
			return data.Refs;
		}

		/// <summary>
		/// Reads the blob's data
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public abstract ValueTask<BlobData> ReadAsync(CancellationToken cancellationToken = default);

		/// <summary>
		/// Creates a reader for this node's data
		/// </summary>
		/// <param name="offset">Offset within the payload stream to start reading</param>
		/// <param name="buffer">Buffer to receive the data that was read</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Number of bytes that were read</returns>
		public virtual async ValueTask<int> ReadPartialAsync(int offset, Memory<byte> buffer, CancellationToken cancellationToken = default)
		{
			using BlobData data = await ReadAsync(cancellationToken);

			int length = data.Data.Length - offset;
			if (length < 0)
			{
				return 0;
			}
			if (length > buffer.Length)
			{
				length = buffer.Length;
			}

			data.Data.Slice(offset, length).CopyTo(buffer);
			return length;
		}

		/// <summary>
		/// Flush the referenced not to underlying storage
		/// </summary>
		public virtual ValueTask FlushAsync(CancellationToken cancellationToken) => new ValueTask();

		/// <inheritdoc/>
		public override string ToString()
		{
			BlobLocator blobId;
			if (TryGetLocator(out blobId))
			{
				return blobId.ToString();
			}
			return base.ToString() ?? "Unknown";
		}
	}
}
