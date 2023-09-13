// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using Microsoft.Extensions.Logging.Abstractions;
using EpicGames.Horde.Storage.Bundles;
using EpicGames.Horde.Storage.Clients;

namespace EpicGames.Horde.Compute
{
	/// <summary>
	/// Storage client which can read bundles over a compute channel
	/// </summary>
	public sealed class AgentStorageClient : BundleStorageClient, IDisposable
	{
		readonly AgentStorageBackend _backend;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="channel"></param>
		public AgentStorageClient(AgentMessageChannel channel)
			: this(new AgentStorageBackend(channel))
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		private AgentStorageClient(AgentStorageBackend backend)
			: base(backend, StorageCache.None, NullLogger.Instance)
		{
			_backend = backend;
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			_backend.Dispose();
		}

		#region Nodes

		/// <inheritdoc/>
		public override Task AddAliasAsync(Utf8String name, BundleNodeHandle locator, int rank, CancellationToken cancellationToken = default) => throw new NotSupportedException();

		/// <inheritdoc/>
		public override Task RemoveAliasAsync(Utf8String name, BundleNodeHandle locator, CancellationToken cancellationToken = default) => throw new NotSupportedException();

		/// <inheritdoc/>
		public override IAsyncEnumerable<BundleNodeHandle> FindAliasAsync(Utf8String name, CancellationToken cancellationToken = default) => throw new NotSupportedException();

		#endregion

		#region Refs

		/// <inheritdoc/>
		public override Task<bool> DeleteRefAsync(RefName name, CancellationToken cancellationToken = default) => throw new NotSupportedException();

		/// <inheritdoc/>
		public override Task<BundleNodeHandle?> TryReadRefTargetAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default) => throw new NotSupportedException();

		/// <inheritdoc/>
		public override Task WriteRefTargetAsync(RefName name, BundleNodeHandle target, RefOptions? options = null, CancellationToken cancellationToken = default) => throw new NotSupportedException();

		#endregion
	}

	class AgentStorageBackend : IStorageBackend
	{
		readonly AgentMessageChannel _channel;
		readonly SemaphoreSlim _semaphore;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="channel"></param>
		public AgentStorageBackend(AgentMessageChannel channel)
		{
			_channel = channel;
			_semaphore = new SemaphoreSlim(1);
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			_semaphore.Dispose();
		}

		/// <inheritdoc/>
		public bool SupportsRedirects => throw new NotImplementedException();

		/// <inheritdoc/>
		public async Task<Stream> OpenAsync(string path, int offset, int? length, CancellationToken cancellationToken = default)
		{
			await _semaphore.WaitAsync(cancellationToken);
			try
			{
				ReadOnlyMemory<byte> data;
				if (length.HasValue && length.Value == 0)
				{
					data = ReadOnlyMemory<byte>.Empty;
				}
				else
				{
					data = await _channel.ReadBlobAsync(path, offset, length ?? 0, cancellationToken);
				}
				return new ReadOnlyMemoryStream(data);
			}
			finally
			{
				_semaphore.Release();
			}
		}

		/// <inheritdoc/>
		public Task<string> WriteAsync(Stream stream, string? prefix = null, CancellationToken cancellationToken = default) => throw new NotSupportedException();

		/// <inheritdoc/>
		public Task WriteExplicitPathAsync(string path, Stream stream, CancellationToken cancellationToken = default) => throw new NotSupportedException();

		/// <inheritdoc/>
		public Task DeleteAsync(string path, CancellationToken cancellationToken = default) => throw new NotSupportedException();

		/// <inheritdoc/>
		public IAsyncEnumerable<string> EnumerateAsync(CancellationToken cancellationToken = default) => throw new NotSupportedException();

		/// <inheritdoc/>
		public Task<bool> ExistsAsync(string path, CancellationToken cancellationToken = default) => throw new NotSupportedException();

		/// <inheritdoc/>
		public ValueTask<Uri?> TryGetReadRedirectAsync(string path, CancellationToken cancellationToken = default) => throw new NotSupportedException();

		/// <inheritdoc/>
		public ValueTask<(string, Uri)?> TryGetWriteRedirectAsync(string? prefix = null, CancellationToken cancellationToken = default) => throw new NotSupportedException();
	}
}
