// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Handle to a node. Can be used to reference nodes that have not been flushed yet. Handles should have value equality semantics, and must override <see cref="Object.Equals(Object?)"/> and <see cref="Object.GetHashCode()"/>.
	/// </summary>
	public abstract class BlobHandle
	{
		class BlobDataFragment : IReadOnlyMemoryOwner<byte>
		{
			readonly BlobData _data;

			public ReadOnlyMemory<byte> Memory { get; }

			public BlobDataFragment(BlobData data, int offset, int length)
			{
				_data = data;
				Memory = data.Data.Slice(offset, length);
			}

			public void Dispose() => _data.Dispose();
		}

		class BlobDataStream : ReadOnlyMemoryStream
		{
			readonly BlobData _blobData;

			public BlobDataStream(BlobData blobData, ReadOnlyMemory<byte> data)
				: base(data)
			{
				_blobData = blobData;
			}

			protected override void Dispose(bool disposing)
			{
				base.Dispose(disposing);
				if (disposing)
				{
					_blobData.Dispose();
				}
			}
		}

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
		/// Open the blob's data stream
		/// </summary>
		/// <param name="offset">Start offset of the stream</param>
		/// <param name="length">Length of the stream</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public virtual async Task<Stream> OpenAsync(int offset = 0, int? length = null, CancellationToken cancellationToken = default)
		{
			BlobData blobData = await ReadAsync(cancellationToken);
			int maxLength = blobData.Data.Length - offset;
			ReadOnlyMemory<byte> memory = blobData.Data.Slice(offset, length.HasValue ? Math.Min(length.Value, maxLength) : maxLength);
			return new BlobDataStream(blobData, memory);
		}

		/// <summary>
		/// Reads the blob's data
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public abstract ValueTask<BlobData> ReadAsync(CancellationToken cancellationToken = default);

		/// <summary>
		/// Reads part of the blob, and returns a handle that can be used to access the data.
		/// </summary>
		/// <param name="offset">Offset of the data within the blob</param>
		/// <param name="length">Length of the data to read</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		public virtual async ValueTask<IReadOnlyMemoryOwner<byte>> ReadPartialAsync(int offset, int length, CancellationToken cancellationToken = default)
		{
			BlobData data = await ReadAsync(cancellationToken);
			return new BlobDataFragment(data, offset, length);
		}

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
		public virtual ValueTask FlushAsync(CancellationToken cancellationToken = default) => new ValueTask();

		/// <inheritdoc/>
		public override string ToString()
		{
			BlobLocator locator;
			if (TryGetLocator(out locator))
			{
				return locator.ToString();
			}
			return base.ToString() ?? "Unknown";
		}

		/// <inheritdoc/>
		public abstract override bool Equals(object? obj);

		/// <inheritdoc/>
		public abstract override int GetHashCode();
	}

	/// <summary>
	/// Instance of <see cref="BlobHandle"/> which wraps an inner handle and a fragment
	/// </summary>
	public class BlobFragmentHandle : BlobHandle
	{
		/// <summary>
		/// Handle to the inner blob
		/// </summary>
		public BlobHandle Inner { get; }

		/// <summary>
		/// The fragment portion of the handle
		/// </summary>
		public Utf8String Fragment { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public BlobFragmentHandle(BlobHandle inner, Utf8String fragment)
		{
			Inner = inner;
			Fragment = fragment;
		}

		/// <inheritdoc/>
		public override bool TryGetLocator([NotNullWhen(true)] out BlobLocator locator)
		{
			BlobLocator innerLocator;
			if (Inner.TryGetLocator(out innerLocator))
			{
				locator = new BlobLocator(innerLocator, Fragment.Span);
				return true;
			}
			else
			{
				locator = default;
				return false;
			}
		}

		/// <inheritdoc/>
		public override ValueTask<BlobData> ReadAsync(CancellationToken cancellationToken = default)
		{
			throw new NotSupportedException("Blob fragment handles cannot be read directly, and should be deconstructed into more specific types.");
		}

		/// <inheritdoc/>
		public override bool Equals(object? obj) => obj is BlobFragmentHandle other && Inner == other.Inner && Fragment == other.Fragment;

		/// <inheritdoc/>
		public override int GetHashCode() => HashCode.Combine(Inner, Fragment);
	}

	/// <summary>
	/// Extension methods for blob handles
	/// </summary>
	public static class BlobHandleExtensions
	{
		/// <summary>
		/// Helper method for awaiting a handle and returning its locator
		/// </summary>
		public static async ValueTask<BlobLocator> GetLocatorAsync(this Task<BlobHandle> handleTask)
		{
			BlobHandle handle = await handleTask;
			return handle.GetLocator();
		}
	}
}
