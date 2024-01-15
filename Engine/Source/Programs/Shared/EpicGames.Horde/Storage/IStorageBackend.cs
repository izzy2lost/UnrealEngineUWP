// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Interface for a low-level storage backend.
	/// </summary>
	public interface IStorageBackend : IDisposable
	{
		/// <summary>
		/// Whether this storage backend supports HTTP redirects for reads and writes
		/// </summary>
		bool SupportsRedirects { get; }

		/// <summary>
		/// Attempts to open a read stream for the given path.
		/// </summary>
		/// <param name="locator">Relative path within the bucket</param>
		/// <param name="offset">Offset to start reading from</param>
		/// <param name="length">Length of data to read</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		Task<Stream> OpenBlobAsync(BlobLocator locator, int offset, int? length, CancellationToken cancellationToken = default);

		/// <summary>
		/// Reads an object into memory and returns a handle to it.
		/// </summary>
		/// <param name="locator">Path to the file</param>
		/// <param name="offset">Offset of the data to retrieve</param>
		/// <param name="length">Length of the data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Handle to the data. Must be disposed by the caller.</returns>
		Task<IReadOnlyMemoryOwner<byte>> ReadBlobAsync(BlobLocator locator, int offset, int? length, CancellationToken cancellationToken = default);

		/// <summary>
		/// Writes a stream to the storage backend. If the stream throws an exception during read, the write will be aborted.
		/// </summary>
		/// <param name="stream">Stream to write</param>
		/// <param name="prefix">Path prefix for the uploaded data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Path to the uploaded object</returns>
		Task<BlobLocator> WriteBlobAsync(Stream stream, string? prefix = null, CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets a HTTP redirect for a read request
		/// </summary>
		/// <param name="locator">Path to read from</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Path to upload the data to</returns>
		ValueTask<Uri?> TryGetBlobReadRedirectAsync(BlobLocator locator, CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets a HTTP redirect for a write request
		/// </summary>
		/// <param name="prefix">Prefix for the uploaded data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Path for retrieval, and URI to upload the data to</returns>
		ValueTask<(BlobLocator, Uri)?> TryGetBlobWriteRedirectAsync(string? prefix = null, CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets stats for this storage backend
		/// </summary>
		void GetStats(StorageStats stats);
	}

	/// <summary>
	/// Utility methods for storage backend implementations
	/// </summary>
	public static class StorageHelpers
	{
		/// <summary>
		/// Unique session id used for unique ids
		/// </summary>
		static readonly string s_sessionPrefix = $"{Guid.NewGuid():n}_";

		/// <summary>
		/// Incremented value used for each supplied id
		/// </summary>
		static int s_increment;

		/// <summary>
		/// Creates a unique name with a given prefix
		/// </summary>
		/// <param name="prefix">The prefix to use</param>
		/// <returns>Unique name generated with the given prefix</returns>
		public static BlobLocator CreateUniqueLocator(string? prefix)
		{
			StringBuilder builder = new StringBuilder(prefix);
			if (builder.Length > 0 && builder[^1] != '/')
			{
				builder.Append('/');
			}
			builder.Append(s_sessionPrefix);
			builder.Append(Interlocked.Increment(ref s_increment));
			return new BlobLocator(builder.ToString());
		}
	}

	/// <summary>
	/// Extension methods for <see cref="IStorageBackend"/>
	/// </summary>
	public static class StorageBackendExtensions
	{
		/// <summary>
		/// Attempts to open a read stream for the given path.
		/// </summary>
		/// <param name="storageBackend">Backend to read from</param>
		/// <param name="locator">Object name within the store</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Stream for the object</returns>
		public static Task<Stream> OpenAsync(this IStorageBackend storageBackend, BlobLocator locator, CancellationToken cancellationToken = default) => storageBackend.OpenBlobAsync(locator, 0, null, cancellationToken);

		/// <summary>
		/// Attempts to open a read stream for the given path.
		/// </summary>
		/// <param name="storageBackend">Backend to read from</param>
		/// <param name="locator">Object name within the store</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Stream for the object</returns>
		public static Task<IReadOnlyMemoryOwner<byte>> ReadAsync(this IStorageBackend storageBackend, BlobLocator locator, CancellationToken cancellationToken = default) => storageBackend.ReadBlobAsync(locator, 0, null, cancellationToken);

		/// <summary>
		/// Reads an object as an array of bytes
		/// </summary>
		/// <param name="storageBackend">Backend to read from</param>
		/// <param name="locator">Object name within the store</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Contents of the object</returns>
		public static async Task<byte[]> ReadBytesAsync(this IStorageBackend storageBackend, BlobLocator locator, CancellationToken cancellationToken = default)
		{
			using IReadOnlyMemoryOwner<byte> storageObject = await storageBackend.ReadAsync(locator, cancellationToken);
			return storageObject.Memory.ToArray();
		}

		/// <summary>
		/// Writes a block of memory to storage
		/// </summary>
		/// <param name="storageBackend">Backend to read from</param>
		/// <param name="data">Data to be written</param>
		/// <param name="prefix">Prefix for the uploaded data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public static async Task<BlobLocator> WriteBytesAsync(this IStorageBackend storageBackend, ReadOnlyMemory<byte> data, string? prefix = null, CancellationToken cancellationToken = default)
		{
			using (ReadOnlyMemoryStream stream = new ReadOnlyMemoryStream(data))
			{
				return await storageBackend.WriteBlobAsync(stream, prefix, cancellationToken);
			}
		}
	}
}
