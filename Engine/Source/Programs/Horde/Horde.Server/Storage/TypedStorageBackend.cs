// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
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
			public Task<Stream> OpenAsync(string path, int offset, int? length, CancellationToken cancellationToken) => _inner.OpenAsync(path, offset, length, cancellationToken);

			/// <inheritdoc/>
			public Task<IStorageObject> ReadAsync(string path, int offset, int? length, CancellationToken cancellationToken) => _inner.ReadAsync(path, offset, length, cancellationToken);

			/// <inheritdoc/>
			public Task<string> WriteAsync(Stream stream, string? prefix, CancellationToken cancellationToken) => _inner.WriteAsync(stream, prefix, cancellationToken);

#pragma warning disable CS0618
			/// <inheritdoc/>
			public Task WriteExplicitPathAsync(string path, Stream stream, CancellationToken cancellationToken) => _inner.WriteExplicitPathAsync(path, stream, cancellationToken);
#pragma warning restore CS0618

			/// <inheritdoc/>
			public Task DeleteAsync(string path, CancellationToken cancellationToken) => _inner.DeleteAsync(path, cancellationToken);

			/// <inheritdoc/>
			public Task<bool> ExistsAsync(string path, CancellationToken cancellationToken) => _inner.ExistsAsync(path, cancellationToken);

			/// <inheritdoc/>
			public IAsyncEnumerable<string> EnumerateAsync(CancellationToken cancellationToken = default) => _inner.EnumerateAsync(cancellationToken);

			/// <inheritdoc/>
			public ValueTask<Uri?> TryGetReadRedirectAsync(string path, CancellationToken cancellationToken = default) => _inner.TryGetReadRedirectAsync(path, cancellationToken);

			/// <inheritdoc/>
			public ValueTask<(string, Uri)?> TryGetWriteRedirectAsync(string? prefix = null, CancellationToken cancellationToken = default) => _inner.TryGetWriteRedirectAsync(prefix, cancellationToken);

			/// <inheritdoc/>
			public void GetStats(StorageStats stats) { }
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
