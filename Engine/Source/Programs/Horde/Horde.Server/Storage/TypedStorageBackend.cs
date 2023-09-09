// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;

namespace Horde.Server.Storage
{
	/// <summary>
	/// Generic version of IStorageBackend, to allow for dependency injection of different singletons
	/// </summary>
	/// <typeparam name="T">Type distinguishing different singletons</typeparam>
	public interface IStorageBackend<T> : IStorageBackend
	{
	}

	/// <summary>
	/// Extension methods for <see cref="IStorageBackend"/>
	/// </summary>
	public static class StorageBackend
	{
		/// <summary>
		/// Wrapper for <see cref="IStorageBackend"/>
		/// </summary>
		/// <typeparam name="T"></typeparam>
		sealed class TypedStorageBackend<T> : IStorageBackend<T>
		{
			readonly IStorageBackend _inner;

			/// <inheritdoc/>
			public bool SupportsRedirects => _inner.SupportsRedirects;

			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="inner"></param>
			public TypedStorageBackend(IStorageBackend inner) => _inner = inner;

			/// <inheritdoc/>
			public void Dispose() => _inner.Dispose();

			/// <inheritdoc/>
			public Task<Stream> ReadAsync(string path, int offset, int? length, CancellationToken cancellationToken) => _inner.ReadAsync(path, offset, length, cancellationToken);

			/// <inheritdoc/>
			public Task WriteAsync(string path, Stream stream, CancellationToken cancellationToken) => _inner.WriteAsync(path, stream, cancellationToken);

			/// <inheritdoc/>
			public Task DeleteAsync(string path, CancellationToken cancellationToken) => _inner.DeleteAsync(path, cancellationToken);

			/// <inheritdoc/>
			public Task<bool> ExistsAsync(string path, CancellationToken cancellationToken) => _inner.ExistsAsync(path, cancellationToken);

			/// <inheritdoc/>
			public IAsyncEnumerable<string> EnumerateAsync(CancellationToken cancellationToken = default) => _inner.EnumerateAsync(cancellationToken);

			/// <inheritdoc/>
			public ValueTask<Uri?> TryGetReadRedirectAsync(string path, CancellationToken cancellationToken = default) => _inner.TryGetReadRedirectAsync(path, cancellationToken);

			/// <inheritdoc/>
			public ValueTask<Uri?> TryGetWriteRedirectAsync(string path, CancellationToken cancellationToken = default) => _inner.TryGetWriteRedirectAsync(path, cancellationToken);
		}

		/// <summary>
		/// Extension for blob files
		/// </summary>
		public const string BlobExtension = ".blob";

		/// <summary>
		/// Gets the blob id from a path
		/// </summary>
		/// <param name="path">Path to the blob</param>
		/// <returns>Path to the blob</returns>
		public static Utf8String GetBlobPathFromFileName(string path)
		{
			Utf8String blobPath;
			if (!TryGetBlobPathFromFileName(path, out blobPath))
			{
				throw new ArgumentException("Path is not a valid blob identifier", nameof(path));
			}
			return blobPath;
		}

		/// <summary>
		/// Gets the path to a blob
		/// </summary>
		/// <param name="blobPath">Blob identifier</param>
		/// <returns>Path to the blob</returns>
		public static string GetBlobFileName(Utf8String blobPath) => $"{blobPath}{BlobExtension}";

		/// <summary>
		/// Gets a blob id from a path within the storage backend
		/// </summary>
		/// <param name="path">Path to the file</param>
		/// <param name="blobPath">Receives the blob id on success</param>
		/// <returns>True on success</returns>
		public static bool TryGetBlobPathFromFileName(string path, out Utf8String blobPath)
		{
			if (path.EndsWith(BlobExtension, StringComparison.Ordinal))
			{
				blobPath = new Utf8String(path.Substring(0, path.Length - BlobExtension.Length));
				return true;
			}
			else
			{
				blobPath = default;
				return false;
			}
		}

		/// <summary>
		/// Creates a typed wrapper around the given storage backend
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="backend"></param>
		/// <returns></returns>
		public static IStorageBackend<T> ForType<T>(this IStorageBackend backend)
		{
			return new TypedStorageBackend<T>(backend);
		}
	}
}
