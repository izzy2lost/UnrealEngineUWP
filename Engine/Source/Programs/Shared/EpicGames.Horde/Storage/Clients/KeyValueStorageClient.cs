// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage.Clients
{
	/// <summary>
	/// Base class for storage clients that wrap a diirect key/value type store without any merging/splitting.
	/// </summary>
	public abstract class KeyValueStorageClient : IStorageClient
	{
		class Handle : IBlobHandle
		{
			readonly KeyValueStorageClient _keyValueStorageClient;
			readonly BlobLocator _locator;

			/// <inheritdoc/>
			public IBlobHandle? Outer => null;

			/// <summary>
			/// Constructor
			/// </summary>
			public Handle(KeyValueStorageClient keyValueStorageClient, BlobLocator locator)
			{
				_keyValueStorageClient = keyValueStorageClient;
				_locator = locator;
			}

			/// <inheritdoc/>
			public ValueTask FlushAsync(CancellationToken cancellationToken = default) => default;

			/// <inheritdoc/>
			public ValueTask<BlobData> ReadAsync(CancellationToken cancellationToken = default) => _keyValueStorageClient.ReadBlobAsync(_locator, cancellationToken);

			/// <inheritdoc/>
			public bool TryAppendIdentifier(Utf8StringBuilder builder)
			{
				builder.Append(_locator.Path);
				return true;
			}

			/// <inheritdoc/>
			public override bool Equals(object? obj) => obj is Handle other && _locator == other._locator;

			/// <inheritdoc/>
			public override int GetHashCode() => _locator.GetHashCode();
		}

		/// <inheritdoc/>
		public abstract bool SupportsRedirects { get; }

		/// <inheritdoc/>
		public void Dispose()
		{
			Dispose(true);
			GC.SuppressFinalize(this);
		}

		/// <summary>
		/// Overridable dispose method
		/// </summary>
		protected virtual void Dispose(bool disposing)
		{
		}

		#region Blobs

		/// <inheritdoc/>
		public virtual IBlobHandle CreateBlobHandle(BlobLocator locator)
		{
			if (locator.TryUnwrap(out BlobLocator baseLocator, out Utf8String fragment))
			{
				return new BlobFragmentHandle(new Handle(this, baseLocator), fragment);
			}
			else
			{
				return new Handle(this, locator);
			}
		}

		/// <inheritdoc/>
		public virtual IStorageWriter CreateWriter(string? basePath = null) => new DefaultStorageWriter(this, basePath);

		/// <inheritdoc/>
		public abstract ValueTask<BlobData> ReadBlobAsync(BlobLocator locator, CancellationToken cancellationToken = default);

		/// <inheritdoc/>
		public abstract ValueTask<IBlobHandle> WriteBlobAsync(BlobType type, Stream stream, IReadOnlyList<IBlobHandle> references, string? basePath = null, CancellationToken cancellationToken = default);

		/// <inheritdoc/>
		public abstract ValueTask<Uri?> TryGetReadRedirectAsync(BlobLocator locator, CancellationToken cancellationToken = default);

		/// <inheritdoc/>
		public abstract ValueTask<(BlobLocator, Uri)?> TryGetWriteRedirectAsync(string? prefix = null, CancellationToken cancellationToken = default);

		#endregion

		#region Aliases

		/// <inheritdoc/>
		public abstract Task AddAliasAsync(string name, IBlobHandle handle, int rank = 0, ReadOnlyMemory<byte> data = default, CancellationToken cancellationToken = default);

		/// <inheritdoc/>
		public abstract Task RemoveAliasAsync(string name, IBlobHandle handle, CancellationToken cancellationToken = default);

		/// <inheritdoc/>
		public abstract Task<BlobAlias[]> FindAliasesAsync(string name, int? maxResults = null, CancellationToken cancellationToken = default);

		#endregion

		#region Refs

		/// <inheritdoc/>
		public abstract Task<IBlobHandle?> TryReadRefAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default);

		/// <inheritdoc/>
		public abstract Task WriteRefAsync(RefName name, IBlobHandle handle, RefOptions? options = null, CancellationToken cancellationToken = default);

		/// <inheritdoc/>
		public abstract Task<bool> DeleteRefAsync(RefName name, CancellationToken cancellationToken = default);

		#endregion

		/// <inheritdoc/>
		public abstract void GetStats(StorageStats stats);
	}
}
