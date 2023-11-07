// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage.Clients
{
	/// <summary>
	/// Implementation of <see cref="IStorageClient"/> which stores data in memory. Not intended for production use.
	/// </summary>
	public sealed class MemoryStorageClient : KeyValueStorageClient
	{
		record class AliasListNode(BlobLocator Locator, int Rank, ReadOnlyMemory<byte> Data, AliasListNode? Next);

		readonly ConcurrentDictionary<BlobLocator, BlobData> _blobs = new ConcurrentDictionary<BlobLocator, BlobData>();
		readonly ConcurrentDictionary<RefName, BlobLocator> _refs = new ConcurrentDictionary<RefName, BlobLocator>();
		readonly ConcurrentDictionary<string, AliasListNode?> _aliases = new ConcurrentDictionary<string, AliasListNode?>(StringComparer.Ordinal);

		/// <summary>
		/// All data stored by the client
		/// </summary>
		public IReadOnlyDictionary<BlobLocator, BlobData> Blobs => _blobs;

		/// <summary>
		/// Accessor for all refs stored by the client
		/// </summary>
		public IReadOnlyDictionary<RefName, BlobLocator> Refs => _refs;

		/// <inheritdoc/>
		public override bool SupportsRedirects => false;

		static readonly string s_sessionId = Guid.NewGuid().ToString("N");
		static int s_uniqueId = 0;

		#region Blobs

		/// <inheritdoc/>
		public override ValueTask<BlobData> ReadBlobAsync(BlobLocator locator, CancellationToken cancellationToken = default)
		{
			return new ValueTask<BlobData>(_blobs[locator]);
		}

		/// <inheritdoc/>
		public override async ValueTask<IBlobHandle> WriteBlobAsync(BlobType type, Stream stream, IReadOnlyList<IBlobHandle> references, string? basePath = null, CancellationToken cancellationToken = default)
		{
			BlobLocator locator = new BlobLocator($"mem-{s_sessionId}-{Interlocked.Increment(ref s_uniqueId)}");
			_blobs[locator] = new BlobData(type, await stream.ReadAllBytesAsync(cancellationToken), references);
			return CreateBlobHandle(locator);
		}

		/// <inheritdoc/>
		public override ValueTask<Uri?> TryGetReadRedirectAsync(BlobLocator locator, CancellationToken cancellationToken = default)
			=> default;

		/// <inheritdoc/>
		public override ValueTask<(BlobLocator, Uri)?> TryGetWriteRedirectAsync(string? prefix = null, CancellationToken cancellationToken = default)
			=> default;

		#endregion

		#region Aliases

		/// <inheritdoc/>
		public override Task AddAliasAsync(string name, IBlobHandle handle, int rank = 0, ReadOnlyMemory<byte> data = default, CancellationToken cancellationToken = default)
		{
			BlobLocator locator = handle.GetLocator();
			_aliases.AddOrUpdate(name, _ => new AliasListNode(locator, rank, data, null), (_, entry) => new AliasListNode(locator, rank, data, entry));
			return Task.CompletedTask;
		}

		/// <inheritdoc/>
		public override Task RemoveAliasAsync(string name, IBlobHandle handle, CancellationToken cancellationToken = default)
		{
			BlobLocator locator = handle.GetLocator();
			for (; ; )
			{
				AliasListNode? entry;
				if (!_aliases.TryGetValue(name, out entry))
				{
					break;
				}

				AliasListNode? newEntry = RemoveAliasFromList(entry, locator);
				if (entry == newEntry)
				{
					break;
				}

				if (newEntry == null)
				{
					if (_aliases.TryRemove(new KeyValuePair<string, AliasListNode?>(name, entry)))
					{
						break;
					}
				}
				else
				{
					if (_aliases.TryUpdate(name, newEntry, entry))
					{
						break;
					}
				}
			}
			return Task.CompletedTask;
		}

		static AliasListNode? RemoveAliasFromList(AliasListNode? entry, BlobLocator locator)
		{
			if (entry == null)
			{
				return null;
			}
			if (entry.Locator == locator)
			{
				return entry.Next;
			}

			AliasListNode? nextEntry = RemoveAliasFromList(entry.Next, locator);
			if (nextEntry != entry.Next)
			{
				entry = new AliasListNode(entry.Locator, entry.Rank, entry.Data, nextEntry);
			}
			return entry;
		}

		/// <inheritdoc/>
		public override Task<BlobAlias[]> FindAliasesAsync(string alias, int? maxResults = null, CancellationToken cancellationToken = default)
		{
			List<BlobAlias> aliases = new List<BlobAlias>();
			if (_aliases.TryGetValue(alias, out AliasListNode? entry))
			{
				for (; entry != null; entry = entry.Next)
				{
					IBlobHandle handle = CreateBlobHandle(entry.Locator);
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
		public override Task<IBlobHandle?> TryReadRefAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			BlobLocator locator;
			if (_refs.TryGetValue(name, out locator))
			{
				return Task.FromResult<IBlobHandle?>(CreateBlobHandle(locator));
			}
			else
			{
				return Task.FromResult<IBlobHandle?>(null);
			}
		}

		/// <inheritdoc/>
		public override async Task WriteRefAsync(RefName name, IBlobHandle target, RefOptions? options = null, CancellationToken cancellationToken = default)
		{
			await target.FlushAsync(cancellationToken);
			_refs[name] = target.GetLocator();
		}

		#endregion

		/// <inheritdoc/>
		public override void GetStats(StorageStats stats)
		{
		}
	}
}
