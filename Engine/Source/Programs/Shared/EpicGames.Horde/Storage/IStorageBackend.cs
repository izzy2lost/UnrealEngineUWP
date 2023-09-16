// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Handle to a raw object read from storage.
	/// </summary>
	public interface IStorageObject : IDisposable
	{
		/// <summary>
		/// Data for the item. Consumers of this interface must not cache this value beyond the lifetime of the object.
		/// </summary>
		public ReadOnlyMemory<byte> Data { get; }
	}

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
		/// <param name="path">Relative path within the bucket</param>
		/// <param name="offset">Offset to start reading from</param>
		/// <param name="length">Length of data to read</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		Task<Stream> OpenAsync(string path, int offset, int? length, CancellationToken cancellationToken = default);

		/// <summary>
		/// Reads an object into memory and returns a handle to it.
		/// </summary>
		/// <param name="path">Path to the file</param>
		/// <param name="offset">Offset of the data to retrieve</param>
		/// <param name="length">Length of the data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Handle to the data. Must be disposed by the caller.</returns>
		Task<IStorageObject> ReadAsync(string path, int offset, int? length, CancellationToken cancellationToken = default);

		/// <summary>
		/// Writes a stream to the storage backend. If the stream throws an exception during read, the write will be aborted.
		/// </summary>
		/// <param name="stream">Stream to write</param>
		/// <param name="prefix">Path prefix for the uploaded data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Path to the uploaded object</returns>
		Task<string> WriteAsync(Stream stream, string? prefix = null, CancellationToken cancellationToken = default);

		/// <summary>
		/// Writes a stream to the storage backend. This overload is deprecated; prefer passing a prefix to allow the server to determine a unique path.
		/// </summary>
		/// <param name="path"></param>
		/// <param name="stream">Stream to write</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Path to the uploaded object</returns>
		[Obsolete("Use WriteAsync() instead. Ability to specify an explicit path is deprecated and will be removed in a future release.")]
		Task WriteExplicitPathAsync(string path, Stream stream, CancellationToken cancellationToken = default);

		/// <summary>
		/// Tests whether the given path exists
		/// </summary>
		/// <param name="path">Relative path within the bucket</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		Task<bool> ExistsAsync(string path, CancellationToken cancellationToken = default);

		/// <summary>
		/// Deletes a file with the given path
		/// </summary>
		/// <param name="path">Relative path within the bucket</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Async task</returns>
		Task DeleteAsync(string path, CancellationToken cancellationToken = default);

		/// <summary>
		/// Enumerates all the objects in the store
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Sequence of object paths</returns>
		IAsyncEnumerable<string> EnumerateAsync(CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets a HTTP redirect for a read request
		/// </summary>
		/// <param name="path">Path to read from</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Path to upload the data to</returns>
		ValueTask<Uri?> TryGetReadRedirectAsync(string path, CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets a HTTP redirect for a write request
		/// </summary>
		/// <param name="prefix">Prefix for the uploaded data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Path for retrieval, and URI to upload the data to</returns>
		ValueTask<(string, Uri)?> TryGetWriteRedirectAsync(string? prefix = null, CancellationToken cancellationToken = default);
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
		static int _increment;

		/// <summary>
		/// Creates a unique name with a given prefix
		/// </summary>
		/// <param name="prefix">The prefix to use</param>
		/// <returns>Unique name generated with the given prefix</returns>
		public static string CreateUniqueName(string? prefix)
		{
			StringBuilder builder = new StringBuilder(prefix);
			if (builder.Length > 0 && builder[^1] != '/')
			{
				builder.Append('/');
			}
			builder.Append(s_sessionPrefix);
			builder.Append(Interlocked.Increment(ref _increment));
			return builder.ToString();
		}
	}

	/// <summary>
	/// Extension methods for <see cref="IStorageBackend"/>
	/// </summary>
	public static class StorageBackendExtensions
	{
		// Wraps an IStorageObject in a stream
		class StorageObjectStream : Stream
		{
			readonly IStorageObject _storageObject;
			int _offset;

			public override bool CanRead => true;
			public override bool CanSeek => true;
			public override bool CanWrite => false;

			public override long Length => _storageObject.Data.Length;

			public override long Position
			{
				get => _offset;
				set => _offset = (int)Math.Clamp(value, 0, _storageObject.Data.Length);
			}

			public StorageObjectStream(IStorageObject storageObject) => _storageObject = storageObject;

			protected override void Dispose(bool disposing)
			{
				base.Dispose(disposing);

				if (disposing)
				{
					_storageObject.Dispose();
				}
			}

			public override void Flush() { }

			public override int Read(Span<byte> buffer)
			{
				int length = (int)Math.Min(Length - _offset, buffer.Length);
				_storageObject.Data.Span.Slice(_offset, length).CopyTo(buffer);
				_offset += length;
				return length;
			}

			public override int Read(byte[] buffer, int offset, int count) => Read(buffer.AsSpan(offset, count));

			public override long Seek(long offset, SeekOrigin origin)
			{
				return _offset = origin switch
				{
					SeekOrigin.Begin => (int)Math.Clamp(offset, 0, Length),
					SeekOrigin.Current => (int)Math.Clamp(_offset + offset, 0, Length),
					SeekOrigin.End => (int)Math.Clamp(Length + offset, 0, Length),
					_ => throw new InvalidOperationException()
				};
			}

			public override void SetLength(long value) => throw new InvalidOperationException();
			public override void Write(byte[] buffer, int offset, int count) => throw new InvalidOperationException();
		}

		/// <summary>
		/// Create a stream to wrap a storage object. The object will be disposed when the stream is closed.
		/// </summary>
		/// <param name="storageObject">Storage object to wrap</param>
		/// <returns>Stream for the given storage object</returns>
		public static Stream CreateStream(this IStorageObject storageObject) => new StorageObjectStream(storageObject);

		// Creates a slice of a storage object
		class StorageObjectSlice : IStorageObject
		{
			readonly IStorageObject _inner;
			readonly int _offset;
			readonly int _length;

			public ReadOnlyMemory<byte> Data => _inner.Data.Slice(_offset, _length);

			public StorageObjectSlice(IStorageObject inner, int offset, int? length)
			{
				_inner = inner;
				_offset = offset;
				_length = length ?? (inner.Data.Length - offset);
			}

			public void Dispose() => _inner.Dispose();
		}

		/// <summary>
		/// Create a stream to wrap a storage object. The object will be disposed when the stream is closed.
		/// </summary>
		/// <param name="storageObject">Storage object to wrap</param>
		/// <param name="offset">Offset of the slice</param>
		/// <param name="length">Length to take for the slice</param>
		/// <returns>Stream for the given storage object</returns>
		public static IStorageObject CreateSlice(this IStorageObject storageObject, int offset, int? length)
		{
			int actualLength = length ?? (storageObject.Data.Length - offset);
			if (offset == 0 && actualLength == storageObject.Data.Length)
			{
				return storageObject;
			}
			else
			{
				return new StorageObjectSlice(storageObject, offset, length);
			}
		}

		/// <summary>
		/// Attempts to open a read stream for the given path.
		/// </summary>
		/// <param name="storageBackend">Backend to read from</param>
		/// <param name="path">Object name within the store</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Stream for the object</returns>
		public static Task<Stream> OpenAsync(this IStorageBackend storageBackend, string path, CancellationToken cancellationToken = default) => storageBackend.OpenAsync(path, 0, null, cancellationToken);

		/// <summary>
		/// Attempts to open a read stream for the given path.
		/// </summary>
		/// <param name="storageBackend">Backend to read from</param>
		/// <param name="path">Object name within the store</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Stream for the object</returns>
		public static Task<IStorageObject> ReadAsync(this IStorageBackend storageBackend, string path, CancellationToken cancellationToken = default) => storageBackend.ReadAsync(path, 0, null, cancellationToken);

		/// <summary>
		/// Reads an object as an array of bytes
		/// </summary>
		/// <param name="storageBackend">Backend to read from</param>
		/// <param name="path">Object name within the store</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Contents of the object</returns>
		public static async Task<byte[]> ReadBytesAsync(this IStorageBackend storageBackend, string path, CancellationToken cancellationToken = default)
		{
			using IStorageObject storageObject = await storageBackend.ReadAsync(path, cancellationToken);
			return storageObject.Data.ToArray();
		}

		/// <summary>
		/// Writes a block of memory to storage
		/// </summary>
		/// <param name="storageBackend">Backend to read from</param>
		/// <param name="data">Data to be written</param>
		/// <param name="prefix">Prefix for the uploaded data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public static async Task<string> WriteBytesAsync(this IStorageBackend storageBackend, ReadOnlyMemory<byte> data, string? prefix = null, CancellationToken cancellationToken = default)
		{
			using (ReadOnlyMemoryStream stream = new ReadOnlyMemoryStream(data))
			{
				return await storageBackend.WriteAsync(stream, prefix, cancellationToken);
			}
		}
	}
}
