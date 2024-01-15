// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage.ObjectStores
{
	/// <summary>
	/// Storage backend wrapper which adds a prefix to the start of each item
	/// </summary>
	public sealed class PrefixedObjectStore : IObjectStore
	{
		readonly ObjectKey _prefix;
		readonly IObjectStore _inner;

		/// <inheritdoc/>
		public bool SupportsRedirects => _inner.SupportsRedirects;

		ObjectKey GetFullLocator(ObjectKey locator) => new ObjectKey($"{_prefix}/{locator}");

		/// <summary>
		/// Constructor
		/// </summary>
		public PrefixedObjectStore(ObjectKey prefix, IObjectStore inner)
		{
			_prefix = prefix;
			_inner = inner;
		}

		/// <inheritdoc/>
		public void Dispose()
		{
		}

		/// <inheritdoc/>
		public Task DeleteAsync(ObjectKey locator, CancellationToken cancellationToken = default) => _inner.DeleteAsync(GetFullLocator(locator), cancellationToken);

		/// <inheritdoc/>
		public async IAsyncEnumerable<ObjectKey> EnumerateAsync([EnumeratorCancellation] CancellationToken cancellationToken = default)
		{
			await foreach (ObjectKey locator in _inner.EnumerateAsync(cancellationToken))
			{
				if (locator.WithinFolder(_prefix))
				{
					yield return new ObjectKey(locator.Path.Substring(_prefix.Path.Length));
				}
			}
		}

		/// <inheritdoc/>
		public Task<bool> ExistsAsync(ObjectKey locator, CancellationToken cancellationToken = default) => _inner.ExistsAsync(GetFullLocator(locator), cancellationToken);

		/// <inheritdoc/>
		public Task<Stream> OpenAsync(ObjectKey locator, int offset, int? length, CancellationToken cancellationToken = default) => _inner.OpenAsync(GetFullLocator(locator), offset, length, cancellationToken);

		/// <inheritdoc/>
		public Task<IReadOnlyMemoryOwner<byte>> ReadAsync(ObjectKey locator, int offset, int? length, CancellationToken cancellationToken = default) => _inner.ReadAsync(GetFullLocator(locator), offset, length, cancellationToken);

		/// <inheritdoc/>
		public ValueTask<Uri?> TryGetReadRedirectAsync(ObjectKey locator, CancellationToken cancellationToken = default) => _inner.TryGetReadRedirectAsync(GetFullLocator(locator), cancellationToken);

		/// <inheritdoc/>
		public ValueTask<Uri?> TryGetWriteRedirectAsync(ObjectKey locator, CancellationToken cancellationToken = default) => _inner.TryGetWriteRedirectAsync(GetFullLocator(locator), cancellationToken);

		/// <inheritdoc/>
		public Task WriteAsync(ObjectKey locator, Stream stream, CancellationToken cancellationToken = default) => _inner.WriteAsync(GetFullLocator(locator), stream, cancellationToken);

		/// <inheritdoc/>
		public void GetStats(StorageStats stats) => _inner.GetStats(stats);
	}
}
