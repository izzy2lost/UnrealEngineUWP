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
	/// Handle to a node. Can be used to reference nodes that have not been flushed yet.
	/// </summary>
	public interface IBlobHandle
	{
		private class BlobDataFragment : IReadOnlyMemoryOwner<byte>
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

		private class BlobDataStream : ReadOnlyMemoryStream
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
		IBlobHandle? Outer { get; }

		/// <summary>
		/// Flush the referenced data to underlying storage
		/// </summary>
		ValueTask FlushAsync(CancellationToken cancellationToken = default);

		/// <summary>
		/// Open the blob's data stream
		/// </summary>
		/// <param name="offset">Start offset of the stream</param>
		/// <param name="length">Length of the stream</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		async Task<Stream> OpenBodyAsync(int offset = 0, int? length = null, CancellationToken cancellationToken = default)
		{
			BlobData blobData = await ReadBlobDataAsync(cancellationToken);
			int maxLength = blobData.Data.Length - offset;
			ReadOnlyMemory<byte> memory = blobData.Data.Slice(offset, length.HasValue ? Math.Min(length.Value, maxLength) : maxLength);
			return new BlobDataStream(blobData, memory);
		}

		/// <summary>
		/// Reads the blob's data
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		ValueTask<BlobData> ReadBlobDataAsync(CancellationToken cancellationToken = default);

		/// <summary>
		/// Reads part of the blob, and returns a handle that can be used to access the data.
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		async ValueTask<IReadOnlyMemoryOwner<byte>> ReadBodyAsync(CancellationToken cancellationToken = default)
		{
			return await ReadBodyAsync(0, null, cancellationToken);
		}

		/// <summary>
		/// Reads part of the blob, and returns a handle that can be used to access the data.
		/// </summary>
		/// <param name="offset">Offset of the data within the blob</param>
		/// <param name="length">Length of the data to read</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		async ValueTask<IReadOnlyMemoryOwner<byte>> ReadBodyAsync(int offset, int? length, CancellationToken cancellationToken = default)
		{
			BlobData data = await ReadBlobDataAsync(cancellationToken);
			return new BlobDataFragment(data, offset, length);
		}

		/// <summary>
		/// Gets an identifier for this blob, relative to its outer
		/// </summary>
		/// <param name="builder">Builder for appending the identifier to</param>
		/// <returns>True if an identifier was returned, false otherwise</returns>
		bool TryAppendIdentifier(Utf8StringBuilder builder);

		/// <summary>
		/// Gets a handle to a nested blob object
		/// </summary>
		/// <param name="fragment">Name of the blob fragment</param>
		public IBlobHandle GetFragmentHandle(ReadOnlySpan<byte> fragment)
			=> throw new InvalidOperationException("Not supported for this handle type.");
	}

	/// <summary>
	/// Typed interface to a particular blob handle
	/// </summary>
	/// <typeparam name="T">Type of the deserialized blob</typeparam>
	public interface IBlobHandle<out T> : IBlobHandle
	{
		/// <summary>
		/// Hash of the target node
		/// </summary>
		IoHash Hash { get; }
	}

	/// <summary>
	/// Instance of <see cref="IBlobHandle"/> which wraps an inner handle and a fragment
	/// </summary>
	public class BlobFragmentHandle : IBlobHandle
	{
		/// <summary>
		/// Handle to the outer blob
		/// </summary>
		public IBlobHandle Outer { get; }

		/// <summary>
		/// The fragment portion of the handle
		/// </summary>
		public Utf8String Fragment { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public BlobFragmentHandle(IBlobHandle outer, Utf8String fragment)
		{
			Outer = outer;
			Fragment = fragment;
		}

		/// <inheritdoc/>
		public ValueTask FlushAsync(CancellationToken cancellationToken = default) => default;

		/// <inheritdoc/>
		public bool TryAppendIdentifier(Utf8StringBuilder builder)
		{
			builder.Append(Fragment);
			return true;
		}

		/// <inheritdoc/>
		public ValueTask<BlobData> ReadBlobDataAsync(CancellationToken cancellationToken = default)
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
		interface IWrappedBlobHandle
		{
			public IBlobHandle Inner { get; }
		}

		class TypedBlobHandle<T> : IWrappedBlobHandle, IBlobHandle<T>
		{
			readonly IBlobHandle _inner;
			readonly IoHash _hash;

			public TypedBlobHandle(IBlobHandle inner, IoHash hash)
			{
				_inner = inner;
				_hash = hash;
			}

			public IBlobHandle Inner => _inner;
			public IoHash Hash => _hash;

			public IBlobHandle? Outer => _inner.Outer;

			public ValueTask FlushAsync(CancellationToken cancellationToken = default) => _inner.FlushAsync(cancellationToken);
			public ValueTask<BlobData> ReadBlobDataAsync(CancellationToken cancellationToken = default) => _inner.ReadBlobDataAsync(cancellationToken);
			public bool TryAppendIdentifier(Utf8StringBuilder builder) => _inner.TryAppendIdentifier(builder);
		}

		/// <summary>
		/// Creates a typed blob handle
		/// </summary>
		public static IBlobHandle<T> ForType<T>(this IBlobHandle handle, IoHash hash) => new TypedBlobHandle<T>(handle, hash);

		/// <summary>
		/// 
		/// </summary>
		/// <param name="handle"></param>
		/// <returns></returns>
		public static IBlobHandle Unwrap(this IBlobHandle handle)
		{
			while (handle is IWrappedBlobHandle wrappedHandle)
			{
				handle = wrappedHandle.Inner;
			}
			return handle;
		}

		/// <summary>
		/// Gets a path to this blob that can be used to describe blob references over the wire.
		/// </summary>
		/// <param name="handle">Handle to query</param>
		public static BlobLocator GetLocator(this IBlobHandle handle)
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
		public static bool TryGetLocator(this IBlobHandle handle, [NotNullWhen(true)] out BlobLocator locator)
		{
			Utf8StringBuilder builder = new Utf8StringBuilder();
			if (TryAppendLocator(handle, builder))
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
		static bool TryAppendLocator(IBlobHandle handle, Utf8StringBuilder builder)
		{
			IBlobHandle? outer = handle.Outer;
			if (outer != null)
			{
				if (!TryAppendLocator(outer, builder))
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
		public static bool TryGetIdentifier(this IBlobHandle handle, out Utf8String fragment)
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
		public static async ValueTask<BlobLocator> GetLocatorAsync(this Task<IBlobHandle> handleTask)
		{
			IBlobHandle handle = await handleTask;
			return handle.GetLocator();
		}
	}
}
