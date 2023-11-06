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

			public BlobDataFragment(BlobData data, int offset, int? length)
			{
				_data = data;

				if (length == null)
				{
					Memory = data.Data.Slice(offset);
				}
				else
				{
					Memory = data.Data.Slice(offset, Math.Min(length.Value, data.Data.Length - offset));
				}
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
		/// For a blob nested within another blob, gets a handle to the containing blob (eg. For a bundle node, will return the packet. For a bundle packet, will return the bundle. For a bundle or other non-nested blob, returns null.)
		/// </summary>
		public abstract BlobHandle? Outer { get; }

		/// <summary>
		/// Gets the type of this blob
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public virtual async ValueTask<BlobType> GetTypeAsync(CancellationToken cancellationToken = default)
		{
			using BlobData data = await ReadBlobDataAsync(cancellationToken);
			return data.Type;
		}

		/// <summary>
		/// Gets the outward references from this blob
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public virtual async ValueTask<IReadOnlyList<BlobHandle>> GetRefsAsync(CancellationToken cancellationToken = default)
		{
			using BlobData data = await ReadBlobDataAsync(cancellationToken);
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
			BlobData blobData = await ReadBlobDataAsync(cancellationToken);
			int maxLength = blobData.Data.Length - offset;
			ReadOnlyMemory<byte> memory = blobData.Data.Slice(offset, length.HasValue ? Math.Min(length.Value, maxLength) : maxLength);
			return new BlobDataStream(blobData, memory);
		}

		/// <summary>
		/// Reads part of the blob, and returns a handle that can be used to access the data.
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		public virtual async ValueTask<IReadOnlyMemoryOwner<byte>> ReadAsync(CancellationToken cancellationToken = default)
		{
			return await ReadPartialAsync(0, null, cancellationToken);
		}

		/// <summary>
		/// Reads part of the blob, and returns a handle that can be used to access the data.
		/// </summary>
		/// <param name="offset">Offset of the data within the blob</param>
		/// <param name="length">Length of the data to read</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		public virtual async ValueTask<IReadOnlyMemoryOwner<byte>> ReadPartialAsync(int offset, int? length, CancellationToken cancellationToken = default)
		{
			BlobData data = await ReadBlobDataAsync(cancellationToken);
			return new BlobDataFragment(data, offset, length);
		}

		/// <summary>
		/// Reads the blob's data
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public abstract ValueTask<BlobData> ReadBlobDataAsync(CancellationToken cancellationToken = default);

		/// <summary>
		/// Flush the referenced not to underlying storage
		/// </summary>
		public virtual ValueTask FlushAsync(CancellationToken cancellationToken = default) => new ValueTask();

		/// <summary>
		/// Gets an identifier for this blob, relative to its outer
		/// </summary>
		/// <param name="builder">Builder for appending the identifier to</param>
		/// <returns>True if an identifier was returned, false otherwise</returns>
		public abstract bool TryAppendIdentifier(Utf8StringBuilder builder);

		/// <inheritdoc/>
		public override string ToString()
		{
			BlobLocator locator;
			if (this.TryGetLocator(out locator))
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
		/// Handle to the outer blob
		/// </summary>
		public override BlobHandle Outer { get; }

		/// <summary>
		/// The fragment portion of the handle
		/// </summary>
		public Utf8String Fragment { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public BlobFragmentHandle(BlobHandle outer, Utf8String fragment)
		{
			Outer = outer;
			Fragment = fragment;
		}

		/// <inheritdoc/>
		public override bool TryAppendIdentifier(Utf8StringBuilder builder)
		{
			builder.Append(Fragment);
			return true;
		}

		/// <inheritdoc/>
		public override ValueTask<BlobData> ReadBlobDataAsync(CancellationToken cancellationToken = default)
		{
			throw new NotSupportedException("Blob fragment handles cannot be read directly, and should be deconstructed into more specific types.");
		}

		/// <inheritdoc/>
		public override bool Equals(object? obj) => obj is BlobFragmentHandle other && Outer.Equals(other.Outer) && Fragment == other.Fragment;

		/// <inheritdoc/>
		public override int GetHashCode() => HashCode.Combine(Outer, Fragment);
	}

	/// <summary>
	/// Extension methods for blob handles
	/// </summary>
	public static class BlobHandleExtensions
	{
		/// <summary>
		/// Gets a path to this blob that can be used to describe blob references over the wire.
		/// </summary>
		/// <param name="handle">Handle to query</param>
		public static BlobLocator GetLocator(this BlobHandle handle)
		{
			BlobLocator locator;
			if (!TryGetLocator(handle, out locator))
			{
				throw new InvalidOperationException("Blob has not yet been written to storage");
			}
			return locator;
		}

		/// <summary>
		/// Attempt to get a path for this blob.
		/// </summary>
		/// <param name="handle">Handle to query</param>
		/// <param name="locator">Receives the blob path on success.</param>
		/// <returns>True if a path was available, false if the blob has not yet been flushed to storage.</returns>
		public static bool TryGetLocator(this BlobHandle handle, [NotNullWhen(true)] out BlobLocator locator)
		{
			Utf8StringBuilder builder = new Utf8StringBuilder();
			if (AppendLocator(handle, builder))
			{
				locator = new BlobLocator(builder.ToUtf8String());
				return true;
			}
			else
			{
				locator = default;
				return false;
			}
		}

		/// <summary>
		/// Builds a full locator for a blob by traversing the outer chain
		/// </summary>
		static bool AppendLocator(BlobHandle handle, Utf8StringBuilder builder)
		{
			BlobHandle? outer = handle.Outer;
			if (outer != null)
			{
				if (!AppendLocator(outer, builder))
				{
					return false;
				}
				if (outer.Outer == null)
				{
					builder.Append('#');
				}
				else
				{
					builder.Append('&');
				}
			}
			return handle.TryAppendIdentifier(builder);
		}

		/// <summary>
		/// Gets an identifier for a blob handle
		/// </summary>
		/// <param name="handle">Handle to the blob</param>
		/// <param name="fragment">On success, receives the fragment path</param>
		/// <returns>True if the handle has an identifier</returns>
		public static bool TryGetIdentifier(this BlobHandle handle, out Utf8String fragment)
		{
			Utf8StringBuilder builder = new Utf8StringBuilder();
			if (handle.TryAppendIdentifier(builder))
			{
				fragment = builder.ToUtf8String();
				return true;
			}
			else
			{
				fragment = default;
				return false;
			}
		}

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
