// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics.CodeAnalysis;
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
		/// <summary>
		/// Flush the referenced data to underlying storage
		/// </summary>
		ValueTask FlushAsync(CancellationToken cancellationToken = default);

		/// <summary>
		/// Reads the blob's data
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		ValueTask<BlobData> ReadBlobDataAsync(CancellationToken cancellationToken = default);

		/// <summary>
		/// Attempt to get a path for this blob.
		/// </summary>
		/// <param name="locator">Receives the blob path on success.</param>
		/// <returns>True if a path was available, false if the blob has not yet been flushed to storage.</returns>
		bool TryGetLocator([NotNullWhen(true)] out BlobLocator locator);
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

			public ValueTask FlushAsync(CancellationToken cancellationToken = default) => _inner.FlushAsync(cancellationToken);
			public ValueTask<BlobData> ReadBlobDataAsync(CancellationToken cancellationToken = default) => _inner.ReadBlobDataAsync(cancellationToken);
			public bool TryGetLocator([NotNullWhen(true)] out BlobLocator locator) => _inner.TryGetLocator(out locator);
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
			if (!handle.TryGetLocator(out locator))
			{
				throw new InvalidOperationException("Blob has not yet been written to storage");
			}
			return locator;
		}
	}
}
