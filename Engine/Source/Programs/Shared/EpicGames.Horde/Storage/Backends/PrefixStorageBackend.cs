// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage.Backends
{
	/// <summary>
	/// Storage backend wrapper which adds a prefix to the start of each item
	/// </summary>
	public sealed class PrefixStorageBackend : IStorageBackend
	{
		readonly string _prefix;
		readonly IStorageBackend _inner;

		/// <inheritdoc/>
		public bool SupportsRedirects => _inner.SupportsRedirects;

		/// <summary>
		/// Constructor
		/// </summary>
		public PrefixStorageBackend(string prefix, IStorageBackend inner)
		{
			_prefix = prefix;
			if (_prefix.Length > 0 && !prefix.EndsWith("/", StringComparison.Ordinal))
			{
				_prefix += "/";
			}

			_inner = inner;
		}

		/// <inheritdoc/>
		public void Dispose() => _inner.Dispose();

		/// <inheritdoc/>
		public Task DeleteAsync(string path, CancellationToken cancellationToken = default) => _inner.DeleteAsync($"{_prefix}{path}", cancellationToken);

		/// <inheritdoc/>
		public async IAsyncEnumerable<string> EnumerateAsync([EnumeratorCancellation] CancellationToken cancellationToken = default)
		{
			await foreach (string path in _inner.EnumerateAsync(cancellationToken))
			{
				if (path.StartsWith(_prefix, StringComparison.Ordinal))
				{
					yield return path.Substring(_prefix.Length);
				}
			}
		}

		/// <inheritdoc/>
		public Task<bool> ExistsAsync(string path, CancellationToken cancellationToken = default) => _inner.ExistsAsync($"{_prefix}{path}", cancellationToken);

		/// <inheritdoc/>
		public Task<Stream> OpenAsync(string path, int offset, int? length, CancellationToken cancellationToken = default) => _inner.OpenAsync($"{_prefix}{path}", offset, length, cancellationToken);

		/// <inheritdoc/>
		public Task<IReadOnlyMemoryOwner<byte>> ReadAsync(string path, int offset, int? length, CancellationToken cancellationToken = default) => _inner.ReadAsync($"{_prefix}{path}", offset, length, cancellationToken);

		/// <inheritdoc/>
		public ValueTask<Uri?> TryGetReadRedirectAsync(string path, CancellationToken cancellationToken = default) => _inner.TryGetReadRedirectAsync($"{_prefix}{path}", cancellationToken);

		/// <inheritdoc/>
		public ValueTask<(string, Uri)?> TryGetWriteRedirectAsync(string? basePath = null, CancellationToken cancellationToken = default) => _inner.TryGetWriteRedirectAsync($"{_prefix}{basePath}", cancellationToken);

		/// <inheritdoc/>
		public async Task<string> WriteAsync(Stream stream, string? basePath = null, CancellationToken cancellationToken = default)
		{
			string path = await _inner.WriteAsync(stream, $"{_prefix}{basePath}", cancellationToken);
			if (!path.StartsWith(_prefix))
			{
				throw new InvalidOperationException($"Expected written blob to start with requested prefix '{_prefix}{basePath}'. Got '{path}'.");
			}
			return path.Substring(_prefix.Length);
		}

		/// <inheritdoc/>
		[Obsolete("Use WriteAsync() instead. Ability to specify an explicit path is deprecated and will be removed in a future release.")]
		public Task WriteExplicitPathAsync(string path, Stream stream, CancellationToken cancellationToken = default) => _inner.WriteExplicitPathAsync($"{_prefix}{path}", stream, cancellationToken);

		/// <inheritdoc/>
		public void GetStats(StorageStats stats) => _inner.GetStats(stats);
	}
}
