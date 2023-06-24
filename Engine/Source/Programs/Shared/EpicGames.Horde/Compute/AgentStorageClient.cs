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

namespace EpicGames.Horde.Compute
{
	/// <summary>
	/// Storage client which can read bundles over a compute channel
	/// </summary>
	public sealed class AgentStorageClient : BundleStorageClient, IDisposable
	{
		readonly AgentMessageChannel _channel;
		readonly SemaphoreSlim _semaphore;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="channel"></param>
		public AgentStorageClient(AgentMessageChannel channel)
			: base(null, NullLogger.Instance)
		{
			_channel = channel;
			_semaphore = new SemaphoreSlim(1);
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			_semaphore.Dispose();
		}

		#region Blobs

		/// <inheritdoc/>
		public override async Task<Bundle> ReadBundleAsync(BundleLocator locator, CancellationToken cancellationToken = default)
		{
			ReadOnlyMemory<byte> data = await ReadBundleRangeAsync(locator, 0, 0, cancellationToken);
			return new Bundle(data);
		}

		/// <inheritdoc/>
		public override async Task<ReadOnlyMemory<byte>> ReadBundleRangeAsync(BundleLocator locator, int offset, int length, CancellationToken cancellationToken = default)
		{
			await _semaphore.WaitAsync(cancellationToken);
			try
			{
				return await _channel.ReadBlobAsync(locator, offset, length, cancellationToken);
			}
			finally
			{
				_semaphore.Release();
			}
		}

		/// <inheritdoc/>
		public override Task<BundleLocator> WriteBundleAsync(Bundle bundle, Utf8String prefix = default, CancellationToken cancellationToken = default)
		{
			throw new NotSupportedException();
		}

		#endregion

		#region Nodes

		/// <inheritdoc/>
		public override Task AddAliasAsync(Utf8String name, BundleNodeHandle locator, CancellationToken cancellationToken = default)
		{
			throw new NotSupportedException();
		}

		/// <inheritdoc/>
		public override Task RemoveAliasAsync(Utf8String name, BundleNodeHandle locator, CancellationToken cancellationToken = default)
		{
			throw new NotSupportedException();
		}

		/// <inheritdoc/>
		public override IAsyncEnumerable<BundleNodeHandle> FindNodesAsync(Utf8String name, CancellationToken cancellationToken = default)
		{
			throw new NotSupportedException();
		}

		#endregion

		#region Refs

		/// <inheritdoc/>
		public override Task DeleteRefAsync(RefName name, CancellationToken cancellationToken = default)
		{
			throw new NotSupportedException();
		}

		/// <inheritdoc/>
		public override Task<BundleNodeHandle?> TryReadRefTargetAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			throw new NotSupportedException();
		}

		/// <inheritdoc/>
		public override Task WriteRefTargetAsync(RefName name, BundleNodeHandle target, RefOptions? options = null, CancellationToken cancellationToken = default)
		{
			throw new NotSupportedException();
		}

		#endregion
	}
}
