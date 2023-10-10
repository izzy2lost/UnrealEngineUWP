// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
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
		record class ExportEntry(BundleNodeLocator Locator, int Rank, ReadOnlyMemory<byte> Data,  ExportEntry? Next);

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
		readonly ConcurrentDictionary<string, ExportEntry> _aliases = new ConcurrentDictionary<string, ExportEntry>(StringComparer.Ordinal);

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
			: base(backend, BundleReaderCache.None, NullLogger.Instance)
		{
			_backend = backend;
		}

		#region Aliases

		/// <inheritdoc/>
		public override Task AddAliasAsync(string name, BundleNodeLocator handle, int rank = 0, ReadOnlyMemory<byte> data = default, CancellationToken cancellationToken = default)
		{
			_aliases.AddOrUpdate(name, _ => new ExportEntry(handle, rank, data, null), (_, entry) => new ExportEntry(handle, rank, data, entry));
			return Task.CompletedTask;
		}

		/// <inheritdoc/>
		public override Task RemoveAliasAsync(string name, BundleNodeLocator handle, CancellationToken cancellationToken = default)
		{
			throw new NotSupportedException();
		}

		/// <inheritdoc/>
		public override Task<BlobAlias[]> FindAliasesAsync(string alias, int? maxResults = null, CancellationToken cancellationToken = default)
		{
			List<BlobAlias> aliases = new List<BlobAlias>();
			if (_aliases.TryGetValue(alias, out ExportEntry? entry))
			{
				for (; entry != null; entry = entry.Next)
				{
					BundleNodeHandle handle = CreateNodeHandle(entry.Locator);
					aliases.Add(new BlobAlias(handle, entry.Rank, entry.Data));
				}
			}
			return Task.FromResult(aliases.ToArray());
		}

		#endregion

		#region Refs

		/// <inheritdoc/>
		public override Task<bool> DeleteRefAsync(RefName name, CancellationToken cancellationToken) => Task.FromResult(_refs.TryRemove(name, out _));

		/// <inheritdoc/>
		public override Task<BlobHandle?> TryReadRefTargetAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			BundleNodeLocator hashedLocator;
			if (_refs.TryGetValue(name, out hashedLocator))
			{
				return Task.FromResult<BlobHandle?>(CreateNodeHandle(hashedLocator)); 
			}
			else
			{
				return Task.FromResult<BlobHandle?>(null);
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
