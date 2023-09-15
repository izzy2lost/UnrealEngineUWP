// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.Extensions.Logging.Abstractions;
using EpicGames.Horde.Storage.Bundles;
using EpicGames.Horde.Storage.Backends;

namespace EpicGames.Horde.Storage.Clients
{
	/// <summary>
	/// Implementation of <see cref="IStorageClient"/> which stores data in memory. Not intended for production use.
	/// </summary>
	public class MemoryStorageClient : BundleStorageClient
	{
		record class ExportEntry(BundleNodeLocator Locator, int Rank, ExportEntry? Next);

		/// <summary>
		/// Backend instance
		/// </summary>
		readonly MemoryStorageBackend _backend;

		/// <summary>
		/// Map of ref name to ref data
		/// </summary>
		readonly ConcurrentDictionary<RefName, BundleNodeLocator> _refs = new ConcurrentDictionary<RefName, BundleNodeLocator>();

		/// <summary>
		/// Content addressed data lookup
		/// </summary>
		readonly ConcurrentDictionary<Utf8String, ExportEntry> _exports = new ConcurrentDictionary<Utf8String, ExportEntry>();

		/// <summary>
		/// All data stored by the client
		/// </summary>
		public IReadOnlyDictionary<string, byte[]> Blobs => _backend.Blobs;

		/// <inheritdoc cref="_refs"/>
		public IReadOnlyDictionary<RefName, BundleNodeLocator> Refs => _refs;

		/// <summary>
		/// Constructor
		/// </summary>
		public MemoryStorageClient() 
			: this(new MemoryStorageBackend())
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		private MemoryStorageClient(MemoryStorageBackend backend)
			: base(backend, StorageCache.None, NullLogger.Instance)
		{
			_backend = backend;
		}

		#region Aliases

		/// <inheritdoc/>
		public override Task AddAliasAsync(Utf8String name, BundleNodeLocator handle, int rank = 0, CancellationToken cancellationToken = default)
		{
			_exports.AddOrUpdate(name, _ => new ExportEntry(handle, rank, null), (_, entry) => new ExportEntry(handle, rank, entry));
			return Task.CompletedTask;
		}

		/// <inheritdoc/>
		public override Task RemoveAliasAsync(Utf8String name, BundleNodeLocator handle, CancellationToken cancellationToken = default)
		{
			throw new NotSupportedException();
		}

		/// <inheritdoc/>
		public override async IAsyncEnumerable<BundleNodeHandle> FindAliasAsync(Utf8String alias, [EnumeratorCancellation] CancellationToken cancellationToken = default)
		{
			if(_exports.TryGetValue(alias, out ExportEntry? entry))
			{
				for (; entry != null; entry = entry.Next)
				{
					cancellationToken.ThrowIfCancellationRequested();
					await Task.Yield();
					yield return CreateNodeHandle(entry.Locator);
				}
			}
		}

		#endregion

		#region Refs

		/// <inheritdoc/>
		public override Task<bool> DeleteRefAsync(RefName name, CancellationToken cancellationToken) => Task.FromResult(_refs.TryRemove(name, out _));

		/// <inheritdoc/>
		public override Task<BundleNodeHandle?> TryReadRefTargetAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			BundleNodeLocator hashedLocator;
			if (_refs.TryGetValue(name, out hashedLocator))
			{
				return Task.FromResult<BundleNodeHandle?>(new FlushedNodeHandle(BundleReader, hashedLocator)); 
			}
			else
			{
				return Task.FromResult<BundleNodeHandle?>(null);
			}
		}

		/// <inheritdoc/>
		public override Task WriteRefTargetAsync(RefName name, BundleNodeLocator target, RefOptions? options = null, CancellationToken cancellationToken = default)
		{
			_refs[name] = target;
			return Task.CompletedTask;
		}

		#endregion
	}
}
