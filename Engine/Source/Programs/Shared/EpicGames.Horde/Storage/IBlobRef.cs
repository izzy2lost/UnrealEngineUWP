// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics.CodeAnalysis;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Accounts;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Reference to another node in storage. This type is similar to <see cref="IHashedBlobRef"/>, but without a hash.
	/// </summary>
	public interface IBlobRef
	{
		/// <summary>
		/// Accessor for the innermost import
		/// </summary>
		IBlobRef Innermost { get; }

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
	public interface IBlobRef<out T> : IBlobRef
	{
		/// <summary>
		/// Options for deserializing the blob
		/// </summary>
		BlobSerializerOptions? SerializerOptions { get; }
	}

	/// <summary>
	/// Static methods for IBlobRef types.
	/// </summary>
	public static class BlobRef
	{
		class TypedBlobRef<T> : IBlobRef<T>
		{
			readonly IBlobRef _inner;

			/// <inheritdoc/>
			public BlobSerializerOptions? SerializerOptions { get; }

			public IBlobRef Innermost => throw new NotImplementedException();

			public TypedBlobRef(IBlobRef inner, BlobSerializerOptions? serializerOptions)
			{
				_inner = inner;
				SerializerOptions = serializerOptions;
			}

			public ValueTask FlushAsync(CancellationToken cancellationToken = default)
				=> _inner.FlushAsync(cancellationToken);

			public ValueTask<BlobData> ReadBlobDataAsync(CancellationToken cancellationToken = default)
				=> _inner.ReadBlobDataAsync(cancellationToken);

			public bool TryGetLocator([NotNullWhen(true)] out BlobLocator locator)
				=> _inner.TryGetLocator(out locator);
		}

		/// <summary>
		/// Create a typed blob reference
		/// </summary>
		/// <typeparam name="T">Target type</typeparam>
		/// <param name="blobRef">Blob referfence to wrap</param>
		/// <param name="serializerOptions">Options for deserializing the blob</param>
		public static IBlobRef<T> Create<T>(IBlobRef blobRef, BlobSerializerOptions? serializerOptions = null)
			=> new TypedBlobRef<T>(blobRef, serializerOptions);
	}

	/// <summary>
	/// Handle to a node. Can be used to reference nodes that have not been flushed yet.
	/// </summary>
	public interface IHashedBlobRef : IBlobRef
	{
		/// <summary>
		/// Hash of the target node
		/// </summary>
		IoHash Hash { get; }
	}

	/// <summary>
	/// Typed interface to a particular blob handle
	/// </summary>
	/// <typeparam name="T">Type of the deserialized blob</typeparam>
	public interface IHashedBlobRef<out T> : IHashedBlobRef, IBlobRef<T>
	{
	}

	/// <summary>
	/// Helper methods for creating blob handles
	/// </summary>
	public static class HashedBlobRef
	{
		class HashedBlobRefImpl : IHashedBlobRef
		{
			readonly IoHash _hash;
			readonly IBlobRef _handle;

			public IBlobRef Innermost => _handle.Innermost;
			public IoHash Hash => _hash;

			public HashedBlobRefImpl(IoHash hash, IBlobRef handle)
			{
				_hash = hash;
				_handle = handle;
			}

			public ValueTask FlushAsync(CancellationToken cancellationToken = default)
				=> _handle.FlushAsync(cancellationToken);

			public ValueTask<BlobData> ReadBlobDataAsync(CancellationToken cancellationToken = default)
				=> _handle.ReadBlobDataAsync(cancellationToken);

			public bool TryGetLocator([NotNullWhen(true)] out BlobLocator locator)
				=> _handle.TryGetLocator(out locator);
		}

		class HashedBlobRefImpl<T> : HashedBlobRefImpl, IHashedBlobRef<T>
		{
			readonly BlobSerializerOptions _options;

			public BlobSerializerOptions SerializerOptions => _options;

			public HashedBlobRefImpl(IoHash hash, IBlobRef handle, BlobSerializerOptions options)
				: base(hash, handle)
			{
				_options = options;
			}
		}

		/// <summary>
		/// Create an untyped blob handle
		/// </summary>
		/// <param name="handle">Imported blob interface</param>
		/// <param name="hash">Hash of the blob</param>
		/// <returns>Handle to the blob</returns>
		public static IHashedBlobRef Create(IoHash hash, IBlobRef handle)
			=> new HashedBlobRefImpl(hash, handle);

		/// <summary>
		/// Create a typed blob handle
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="blobRef">Existing blob reference</param>
		/// <param name="options">Options for deserializing the target blob</param>
		/// <returns>Handle to the blob</returns>
		public static IHashedBlobRef<T> Create<T>(IHashedBlobRef blobRef, BlobSerializerOptions? options = null)
			=> new HashedBlobRefImpl<T>(blobRef.Hash, blobRef, options ?? BlobSerializerOptions.Default);

		/// <summary>
		/// Create a typed blob handle
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="hash">Hash of the blob</param>
		/// <param name="handle">Imported blob interface</param>
		/// <param name="options">Options for deserializing the target blob</param>
		/// <returns>Handle to the blob</returns>
		public static IHashedBlobRef<T> Create<T>(IoHash hash, IBlobRef handle, BlobSerializerOptions? options = null)
			=> new HashedBlobRefImpl<T>(hash, handle, options ?? BlobSerializerOptions.Default);
	}

	/// <summary>
	/// Contains the value for a blob ref
	/// </summary>
	public record class HashedBlobRefValue(IoHash Hash, BlobLocator Locator);

	/// <summary>
	/// Extension methods for <see cref="IBlobRef"/>
	/// </summary>
	public static class BlobRefExtensions
	{
		/// <summary>
		/// Gets a path to this blob that can be used to describe blob references over the wire.
		/// </summary>
		/// <param name="import">Handle to query</param>
		public static BlobLocator GetLocator(this IBlobRef import)
		{
			BlobLocator locator;
			if (!import.TryGetLocator(out locator))
			{
				throw new InvalidOperationException("Blob has not yet been written to storage");
			}
			return locator;
		}

		/// <summary>
		/// Gets a BlobRefValue from an IBlobRef
		/// </summary>
		public static HashedBlobRefValue GetRefValue(this IHashedBlobRef blobRef)
		{
			return new HashedBlobRefValue(blobRef.Hash, blobRef.GetLocator());
		}
	}
}
