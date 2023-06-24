// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Abstractions;

namespace EpicGames.Horde.Storage.Backends
{
	/// <summary>
	/// Implementation of <see cref="IStorageClient"/> which stores data in memory. Not intended for production use.
	/// </summary>
	public class MemoryStorageClient : BundleStorageClient
	{
		record class ExportEntry(BlobHandle Handle, ExportEntry? Next);

		/// <summary>
		/// Map of blob id to blob data
		/// </summary>
		readonly ConcurrentDictionary<BundleLocator, Bundle> _bundles = new ConcurrentDictionary<BundleLocator, Bundle>();

		/// <summary>
		/// Map of ref name to ref data
		/// </summary>
		readonly ConcurrentDictionary<RefName, BundleNodeLocator> _refs = new ConcurrentDictionary<RefName, BundleNodeLocator>();

		/// <summary>
		/// Content addressed data lookup
		/// </summary>
		readonly ConcurrentDictionary<Utf8String, ExportEntry> _exports = new ConcurrentDictionary<Utf8String, ExportEntry>();

		/// <inheritdoc cref="_bundles"/>
		public IReadOnlyDictionary<BundleLocator, Bundle> Bundles => _bundles;

		/// <inheritdoc cref="_refs"/>
		public IReadOnlyDictionary<RefName, BundleNodeLocator> Refs => _refs;

		/// <summary>
		/// Constructor
		/// </summary>
		public MemoryStorageClient() 
			: base(new MemoryCache(new MemoryCacheOptions()), NullLogger.Instance)
		{
		}

		#region Blobs

		/// <inheritdoc/>
		public override Task<Bundle> ReadBundleAsync(BundleLocator locator, CancellationToken cancellationToken = default)
		{
			Bundle bundle = _bundles[locator];
			return Task.FromResult(bundle);
		}

		/// <inheritdoc/>
		public override Task<ReadOnlyMemory<byte>> ReadBundleRangeAsync(BundleLocator locator, int offset, int length, CancellationToken cancellationToken = default)
		{
			ReadOnlySequence<byte> sequence = _bundles[locator].AsSequence().Slice(offset);
			if (sequence.Length > length)
			{
				sequence = sequence.Slice(0, length);
			}
			return Task.FromResult(sequence.AsSingleSegment());
		}

		/// <inheritdoc/>
		public override Task<BundleLocator> WriteBundleAsync(Bundle bundle, Utf8String prefix = default, CancellationToken cancellationToken = default)
		{
			BundleLocator locator = BundleLocator.CreateUnique(prefix);
			_bundles[locator] = new Bundle(bundle.AsSequence().ToArray());
			return Task.FromResult(locator);
		}

		#endregion

		#region Aliases

		/// <inheritdoc/>
		public override Task AddAliasAsync(Utf8String name, BlobHandle handle, CancellationToken cancellationToken = default)
		{
			_exports.AddOrUpdate(name, _ => new ExportEntry(handle, null), (_, entry) => new ExportEntry(handle, entry));
			return Task.CompletedTask;
		}

		/// <inheritdoc/>
		public override Task RemoveAliasAsync(Utf8String name, BlobHandle handle, CancellationToken cancellationToken = default)
		{
			throw new NotSupportedException();
		}

		/// <inheritdoc/>
		public override async IAsyncEnumerable<BlobHandle> FindNodesAsync(Utf8String alias, [EnumeratorCancellation] CancellationToken cancellationToken = default)
		{
			if(_exports.TryGetValue(alias, out ExportEntry? entry))
			{
				for (; entry != null; entry = entry.Next)
				{
					cancellationToken.ThrowIfCancellationRequested();
					await Task.Yield();
					yield return entry.Handle;
				}
			}
		}

		#endregion

		#region Refs

		/// <inheritdoc/>
		public override Task DeleteRefAsync(RefName name, CancellationToken cancellationToken) => Task.FromResult(_refs.TryRemove(name, out _));

		/// <inheritdoc/>
		public override Task<BlobHandle?> TryReadRefTargetAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			BundleNodeLocator hashedLocator;
			if (_refs.TryGetValue(name, out hashedLocator))
			{
				return Task.FromResult<BlobHandle?>(new FlushedNodeHandle(TreeReader, hashedLocator)); 
			}
			else
			{
				return Task.FromResult<BlobHandle?>(null);
			}
		}

		/// <inheritdoc/>
		public override async Task WriteRefTargetAsync(RefName name, BlobHandle target, RefOptions? options = null, CancellationToken cancellationToken = default)
		{
			_refs[name] = await target.FlushAsync(cancellationToken);
		}

		#endregion
	}
}
