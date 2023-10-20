// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage.Clients
{
	/// <summary>
	/// Implementation of <see cref="IStorageClient"/> which stores data in memory. Not intended for production use.
	/// </summary>
	public sealed class MemoryStorageClient : IStorageClient
	{
		class Handle : BlobHandle
		{
			readonly MemoryStorageClient _owner;
			readonly BlobLocator _locator;

			public Handle(MemoryStorageClient owner, BlobLocator locator)
			{
				_owner = owner;
				_locator = locator;
			}

			/// <inheritdoc/>
			public override ValueTask<BlobData> ReadAsync(CancellationToken cancellationToken = default) => new ValueTask<BlobData>(_owner._blobs[_locator]);

			/// <inheritdoc/>
			public override bool TryGetLocator([NotNullWhen(true)] out BlobLocator locator)
			{
				locator = _locator;
				return true;
			}
		}

		record class AliasListNode(BlobLocator Locator, int Rank, ReadOnlyMemory<byte> Data, AliasListNode? Next);

		readonly ConcurrentDictionary<BlobLocator, BlobData> _blobs = new ConcurrentDictionary<BlobLocator, BlobData>();
		readonly ConcurrentDictionary<RefName, BlobLocator> _refs = new ConcurrentDictionary<RefName, BlobLocator>();
		readonly ConcurrentDictionary<string, AliasListNode?> _aliases = new ConcurrentDictionary<string, AliasListNode?>(StringComparer.Ordinal);

		int _nextId;

		/// <summary>
		/// All data stored by the client
		/// </summary>
		public IReadOnlyDictionary<BlobLocator, BlobData> Blobs => _blobs;

		/// <summary>
		/// Accessor for all refs stored by the client
		/// </summary>
		public IReadOnlyDictionary<RefName, BlobLocator> Refs => _refs;

		/// <inheritdoc/>
		public bool SupportsRedirects => false;

		/// <inheritdoc/>
		public void Dispose()
		{
		}

		#region Blobs

		/// <inheritdoc/>
		public BlobHandle CreateBlobHandle(BlobLocator locator)
		{
			if (locator.TryUnwrapFull(out BlobLocator outer, out Utf8String fragment))
			{
				return new BlobFragmentHandle(new Handle(this, outer), fragment);
			}
			else
			{
				return new Handle(this, locator);
			}
		}

		/// <inheritdoc/>
		public IStorageWriter CreateWriter(string? basePath = null) => new DefaultStorageWriter(this, basePath);

		/// <inheritdoc/>
		public async ValueTask<BlobHandle> WriteBlobAsync(BlobType type, Stream stream, IReadOnlyList<BlobHandle> references, string? basePath = null, CancellationToken cancellationToken = default)
		{
			BlobLocator locator = new BlobLocator($"mem-{Interlocked.Increment(ref _nextId)}");
			_blobs[locator] = new BlobData(type, await stream.ReadAllBytesAsync(cancellationToken), references);
			return new Handle(this, locator);
		}

		/// <inheritdoc/>
		public ValueTask<Uri?> TryGetReadRedirectAsync(BlobLocator locator, CancellationToken cancellationToken = default)
			=> default;

		/// <inheritdoc/>
		public ValueTask<(BlobLocator, Uri)?> TryGetWriteRedirectAsync(string? prefix = null, CancellationToken cancellationToken = default)
			=> default;

		#endregion

		#region Aliases

		/// <inheritdoc/>
		public Task AddAliasAsync(string name, BlobHandle handle, int rank = 0, ReadOnlyMemory<byte> data = default, CancellationToken cancellationToken = default)
		{
			BlobLocator locator = handle.GetLocator();
			_aliases.AddOrUpdate(name, _ => new AliasListNode(locator, rank, data, null), (_, entry) => new AliasListNode(locator, rank, data, entry));
			return Task.CompletedTask;
		}

		/// <inheritdoc/>
		public Task RemoveAliasAsync(string name, BlobHandle handle, CancellationToken cancellationToken = default)
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
		public Task<BlobAlias[]> FindAliasesAsync(string alias, int? maxResults = null, CancellationToken cancellationToken = default)
		{
			List<BlobAlias> aliases = new List<BlobAlias>();
			if (_aliases.TryGetValue(alias, out AliasListNode? entry))
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
		public Task<bool> DeleteRefAsync(RefName name, CancellationToken cancellationToken) => Task.FromResult(_refs.TryRemove(name, out _));

		/// <inheritdoc/>
		public Task<BlobHandle?> TryReadRefAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			BlobLocator locator;
			if (_refs.TryGetValue(name, out locator))
			{
				return Task.FromResult<BlobHandle?>(CreateBlobHandle(locator));
			}
			else
			{
				return Task.FromResult<BlobHandle?>(null);
			}
		}

		/// <inheritdoc/>
		public async Task WriteRefAsync(RefName name, BlobHandle target, RefOptions? options = null, CancellationToken cancellationToken = default)
		{
			await target.FlushAsync(cancellationToken);
			_refs[name] = target.GetLocator();
		}

		#endregion

		/// <inheritdoc/>
		public void GetStats(StorageStats stats)
		{
		}
	}
}
