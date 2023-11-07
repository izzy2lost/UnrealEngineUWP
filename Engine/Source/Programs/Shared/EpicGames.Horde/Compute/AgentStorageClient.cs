// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Clients;

namespace EpicGames.Horde.Compute
{
	/// <summary>
	/// Storage client which can read bundles over a compute channel
	/// </summary>
	public sealed class AgentStorageClient : KeyValueStorageClient
	{
		readonly AgentMessageChannel _channel;
		readonly SemaphoreSlim _semaphore;

		/// <inheritdoc/>
		public override bool SupportsRedirects => false;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="channel"></param>
		public AgentStorageClient(AgentMessageChannel channel)
		{
			_channel = channel;
			_semaphore = new SemaphoreSlim(1);
		}

		/// <inheritdoc/>
		protected override void Dispose(bool disposing)
		{
			base.Dispose(disposing);

			if (disposing)
			{
				_semaphore.Dispose();
			}
		}

		#region Blobs

		/// <inheritdoc/>
		public override async ValueTask<BlobData> ReadBlobAsync(BlobLocator locator, CancellationToken cancellationToken = default)
		{
			await _semaphore.WaitAsync(cancellationToken);
			try
			{
				// TODO: Want to pass 0 for length to read here (meaning entire blob), but older streams misinterpret this as a 0 byte read.
				ReadOnlyMemory<byte> data = await _channel.ReadBlobAsync(locator.ToString(), 0, 128 * 1024 * 1024, cancellationToken);
				return new BlobData(BlobType.Leaf, data, Array.Empty<IBlobHandle>());
			}
			finally
			{
				_semaphore.Release();
			}
		}

		/// <inheritdoc/>
		public override ValueTask<IBlobHandle> WriteBlobAsync(BlobType type, Stream stream, IReadOnlyList<IBlobHandle> references, string? basePath = null, CancellationToken cancellationToken = default) => throw new NotSupportedException();

		/// <inheritdoc/>
		public override ValueTask<Uri?> TryGetReadRedirectAsync(BlobLocator locator, CancellationToken cancellationToken = default) => default;

		/// <inheritdoc/>
		public override ValueTask<(BlobLocator, Uri)?> TryGetWriteRedirectAsync(string? prefix = null, CancellationToken cancellationToken = default) => default;

		#endregion

		#region Aliases

		/// <inheritdoc/>
		public override Task AddAliasAsync(string name, IBlobHandle target, int rank, ReadOnlyMemory<byte> data, CancellationToken cancellationToken = default) => throw new NotSupportedException();

		/// <inheritdoc/>
		public override Task RemoveAliasAsync(string name, IBlobHandle target, CancellationToken cancellationToken = default) => throw new NotSupportedException();

		/// <inheritdoc/>
		public override Task<BlobAlias[]> FindAliasesAsync(string name, int? maxResults, CancellationToken cancellationToken = default) => throw new NotSupportedException();

		#endregion

		#region Refs

		/// <inheritdoc/>
		public override Task<bool> DeleteRefAsync(RefName name, CancellationToken cancellationToken = default) => throw new NotSupportedException();

		/// <inheritdoc/>
		public override Task<IBlobHandle?> TryReadRefAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default) => throw new NotSupportedException();

		/// <inheritdoc/>
		public override Task WriteRefAsync(RefName name, IBlobHandle target, RefOptions? options = null, CancellationToken cancellationToken = default) => throw new NotSupportedException();

		#endregion

		/// <inheritdoc/>
		public override void GetStats(StorageStats stats)
		{
		}
	}
}
