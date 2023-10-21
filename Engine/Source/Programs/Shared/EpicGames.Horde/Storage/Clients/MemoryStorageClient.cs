// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.Extensions.Logging.Abstractions;
using EpicGames.Horde.Storage.Backends;

namespace EpicGames.Horde.Storage.Clients
{
	/// <summary>
	/// Implementation of <see cref="IStorageClient"/> which stores data in memory. Not intended for production use.
	/// </summary>
	public class MemoryStorageClient : BundleStorageClient
	{
		record class ExportEntry(BlobLocator Locator, int Rank, ReadOnlyMemory<byte> Data,  ExportEntry? Next);

		/// <summary>
		/// Backend instance
		/// </summary>
		readonly MemoryStorageBackend _backend;

		/// <summary>
		/// Map of ref name to ref data
		/// </summary>
		readonly ConcurrentDictionary<RefName, BlobLocator> _refs = new ConcurrentDictionary<RefName, BlobLocator>();

		/// <summary>
		/// Content addressed data lookup
		/// </summary>
		readonly ConcurrentDictionary<string, ExportEntry> _aliases = new ConcurrentDictionary<string, ExportEntry>(StringComparer.Ordinal);

		/// <summary>
		/// All data stored by the client
		/// </summary>
		public IReadOnlyDictionary<string, byte[]> Blobs => _backend.Blobs;

		/// <summary>
		/// All refs stored by the client
		/// </summary>
		public IReadOnlyDictionary<RefName, BlobLocator> Refs => _refs;

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
		public override async Task AddAliasAsync(string name, BlobHandle handle, int rank = 0, ReadOnlyMemory<byte> data = default, CancellationToken cancellationToken = default)
		{
			await handle.FlushAsync(cancellationToken);
			BlobLocator locator = handle.GetLocator();
			_aliases.AddOrUpdate(name, _ => new ExportEntry(locator, rank, data, null), (_, entry) => new ExportEntry(locator, rank, data, entry));
		}

		/// <inheritdoc/>
		public override Task RemoveAliasAsync(string name, BlobHandle handle, CancellationToken cancellationToken = default)
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
					BlobHandle handle = CreateBlobHandle(entry.Locator);
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
		public override Task<BlobHandle?> TryReadRefAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			BlobLocator refData;
			if (_refs.TryGetValue(name, out refData))
			{
				return Task.FromResult<BlobHandle?>(CreateBlobHandle(refData)); 
			}
			else
			{
				return Task.FromResult<BlobHandle?>(null);
			}
		}

		/// <inheritdoc/>
		public override async Task WriteRefAsync(RefName name, BlobHandle target, RefOptions? options = null, CancellationToken cancellationToken = default)
		{
			await target.FlushAsync(cancellationToken);

			_refs[name] = target.GetLocator();
		}

		#endregion
	}
}
