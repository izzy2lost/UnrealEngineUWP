// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Base exception for the storage service
	/// </summary>
	public class StorageException : Exception
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public StorageException(string message)
			: base(message)
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public StorageException(string message, Exception? innerException)
			: base(message, innerException)
		{
		}
	}

	/// <summary>
	/// Exception for a ref not existing
	/// </summary>
	public sealed class RefNameNotFoundException : StorageException
	{
		/// <summary>
		/// Name of the missing ref
		/// </summary>
		public RefName Name { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="name"></param>
		public RefNameNotFoundException(RefName name)
			: base($"Ref name '{name}' not found")
		{
			Name = name;
		}
	}

	/// <summary>
	/// Options for a new ref
	/// </summary>
	public class RefOptions
	{
		/// <summary>
		/// Time until a ref is expired
		/// </summary>
		public TimeSpan? Lifetime { get; set; }

		/// <summary>
		/// Whether to extend the remaining lifetime of a ref whenever it is fetched. Defaults to true.
		/// </summary>
		public bool? Extend { get; set; }
	}

	/// <summary>
	/// Information about an alias
	/// </summary>
	/// <param name="Name">Name of the alias</param>
	/// <param name="Rank">Rank of the alias</param>
	/// <param name="Data">Inline data to be stored for the alias</param>
	public record class AliasInfo(string Name, int Rank, ReadOnlyMemory<byte> Data);

	/// <summary>
	/// Interface for the storage system.
	/// </summary>
	public interface IStorageClient : IDisposable
	{
		#region Nodes

		/// <summary>
		/// Creates a new writer for storage nodes
		/// </summary>
		/// <param name="refName">Name of the ref being written.</param>
		/// <returns>New writer instance. Must be disposed after use.</returns>
		IStorageWriter CreateWriter(RefName refName = default);

		#endregion

		#region Aliases

		/// <summary>
		/// Adds an alias to a given node
		/// </summary>
		/// <param name="name">Alias for the node</param>
		/// <param name="handle">Locator for the node</param>
		/// <param name="rank">Rank for this alias. In situations where an alias has multiple mappings, the alias with the highest rank will be returned by default.</param>
		/// <param name="data">Additional data to be stored inline with the alias</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task AddAliasAsync(string name, BlobHandle handle, int rank = 0, ReadOnlyMemory<byte> data = default, CancellationToken cancellationToken = default);

		/// <summary>
		/// Removes an alias from a node
		/// </summary>
		/// <param name="name">Name of the alias</param>
		/// <param name="handle">Locator for the node</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task RemoveAliasAsync(string name, BlobHandle handle, CancellationToken cancellationToken = default);

		/// <summary>
		/// Finds nodes with the given alias. Unlike refs, aliases do not serve as GC roots.
		/// </summary>
		/// <param name="name">Alias for the node</param>
		/// <param name="maxResults">Maximum number of aliases to return</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Nodes matching the given handle</returns>
		Task<BlobAlias[]> FindAliasesAsync(string name, int? maxResults = null, CancellationToken cancellationToken = default);

		#endregion

		#region Refs

		/// <summary>
		/// Reads data for a ref from the store
		/// </summary>
		/// <param name="name">The ref name</param>
		/// <param name="cacheTime">Minimum coherency for any cached value to be returned</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Node pointed to by the ref</returns>
		Task<BlobHandle?> TryReadRefTargetAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default);

		/// <summary>
		/// Writes a new ref to the store
		/// </summary>
		/// <param name="name">Ref to write</param>
		/// <param name="handle">Handle to the target node</param>
		/// <param name="options">Options for the new ref</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Unique identifier for the blob</returns>
		Task WriteRefTargetAsync(RefName name, BlobHandle handle, RefOptions? options = null, CancellationToken cancellationToken = default);

		/// <summary>
		/// Reads data for a ref from the store
		/// </summary>
		/// <param name="name">The ref identifier</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task<bool> DeleteRefAsync(RefName name, CancellationToken cancellationToken = default);

		#endregion

		/// <summary>
		/// Gets a snapshot of the stats for the storage client.
		/// </summary>
		void GetStats(StorageStats stats);
	}

	/// <summary>
	/// Allows creating storage clients for different namespaces
	/// </summary>
	public interface IStorageClientFactory
	{
		/// <summary>
		/// Creates a storage client for the given namespace
		/// </summary>
		/// <param name="namespaceId">Namespace to manipulate</param>
		/// <returns>Storage client instance. Must be disposed by the caller.</returns>
		IStorageClient CreateClient(NamespaceId namespaceId);
	}

	/// <summary>
	/// Indicates the maximum age of a entry returned from a cache in the hierarchy
	/// </summary>
	/// <param name="Utc">Oldest allowed timestamp for a returned result</param>
	public record struct RefCacheTime(DateTime Utc)
	{
		/// <summary>
		/// Maximum age for a cached value to be returned
		/// </summary>
		public readonly TimeSpan MaxAge => DateTime.UtcNow - Utc;

		/// <summary>
		/// Sets the earliest time at which the entry must have been valid
		/// </summary>
		/// <param name="age">Maximum age of any returned cache value. Taken from the moment that this object was created.</param>
		public RefCacheTime(TimeSpan age) : this(DateTime.UtcNow - age) { }

		/// <summary>
		/// Tests whether this value is set
		/// </summary>
		public readonly bool IsSet() => Utc != default;

		/// <summary>
		/// Determines if this cache time deems a particular cache entry stale
		/// </summary>
		/// <param name="entryTime">Time at which the cache entry was valid</param>
		/// <param name="cacheTime">Maximum cache time to test against</param>
		public static bool IsStaleCacheEntry(DateTime entryTime, RefCacheTime cacheTime) => cacheTime.IsSet() && cacheTime.Utc < entryTime;

		/// <summary>
		/// Implicit conversion operator from datetime values.
		/// </summary>
		public static implicit operator RefCacheTime(DateTime time) => new RefCacheTime(time);

		/// <summary>
		/// Implicit conversion operator from timespan values.
		/// </summary>
		public static implicit operator RefCacheTime(TimeSpan age) => new RefCacheTime(age);
	}

	/// <summary>
	/// Stats for the storage system
	/// </summary>
	public class StorageStats
	{
		/// <summary>
		/// Stat name to value
		/// </summary>
		public List<(string, long)> Values { get; } = new List<(string, long)>();

		/// <summary>
		/// Add a new stat to the list
		/// </summary>
		public void Add(string name, long value) => Values.Add((name, value));

		/// <summary>
		/// Prints the table of stats to the logger
		/// </summary>
		public void Print(ILogger logger)
		{
			foreach ((string key, long value) in Values)
			{
				logger.LogInformation("{Key}: {Value:n0}", key, value);
			}
		}

		/// <summary>
		/// Subtract a base set of stats from this one
		/// </summary>
		public static StorageStats GetDelta(StorageStats initial, StorageStats finish)
		{
			StorageStats result = new StorageStats();

			Dictionary<string, long> initialValues = initial.Values.ToDictionary(x => x.Item1, x => x.Item2, StringComparer.Ordinal);
			foreach ((string name, long value) in finish.Values.ToArray())
			{
				initialValues.TryGetValue(name, out long otherValue);
				result.Add(name, value - otherValue);
			}

			return result;
		}
	}

	/// <summary>
	/// Extension methods for <see cref="IStorageClient"/>
	/// </summary>
	public static class StorageClientExtensions
	{
		#region Aliases

		/// <summary>
		/// Finds nodes with the given alias. Unlike refs, aliases do not serve as GC roots.
		/// </summary>
		/// <param name="store">The store instance to read from</param>
		/// <param name="name">Alias for the node</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Nodes matching the given handle</returns>
		public static async Task<BlobAlias?> FindAliasAsync(this IStorageClient store, string name, CancellationToken cancellationToken = default)
		{
			BlobAlias[] aliases = await store.FindAliasesAsync(name, 1, cancellationToken);
			return aliases.FirstOrDefault();
		}

		#endregion

		#region Refs

		/// <summary>
		/// Checks if the given ref exists
		/// </summary>
		/// <param name="store">The store instance to read from</param>
		/// <param name="name">Name of the reference to look for</param>
		/// <param name="cacheTime">Minimum coherency for any cached value to be returned</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>True if the ref exists, false if it did not exist</returns>
		public static async Task<bool> HasRefAsync(this IStorageClient store, RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			BlobHandle? target = await store.TryReadRefTargetAsync(name, cacheTime, cancellationToken);
			return target != null;
		}

		/// <summary>
		/// Reads a ref from the store, throwing an exception if it does not exist
		/// </summary>
		/// <param name="store">The store instance to read from</param>
		/// <param name="name">Id for the ref</param>
		/// <param name="cacheTime">Minimum coherency of any cached result</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The ref target</returns>
		public static async Task<BlobHandle> ReadRefTargetAsync(this IStorageClient store, RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			BlobHandle? refTarget = await store.TryReadRefTargetAsync(name, cacheTime, cancellationToken);
			return refTarget ?? throw new RefNameNotFoundException(name);
		}

		#endregion

		/// <summary>
		/// Gets a snapshot of the stats for the storage client.
		/// </summary>
		public static StorageStats GetStats(this IStorageClient store)
		{
			StorageStats stats = new StorageStats();
			store.GetStats(stats);
			return stats;
		}
	}

	/// <summary>
	/// Extension methods for <see cref="IStorageWriter"/>
	/// </summary>
	public static class StorageWriterExtensions
	{
		/// <summary>
		/// Finish writing a node.
		/// </summary>
		/// <param name="writer">Writer instance to manipulate</param>
		/// <param name="size">Used size of the buffer</param>
		/// <param name="references">References to other nodes</param>
		/// <param name="type">Type of the node that was written</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Handle to the written node</returns>
		public static ValueTask<BlobHandle> WriteBlobAsync(this IStorageWriter writer, int size, IReadOnlyList<BlobHandle> references, BlobType type, CancellationToken cancellationToken = default)
		{
			return writer.WriteBlobAsync(size, references, type, Array.Empty<AliasInfo>(), cancellationToken);
		}

		/// <summary>
		/// Finish writing a node.
		/// </summary>
		/// <param name="writer">Writer instance to manipulate</param>
		/// <param name="size">Used size of the buffer</param>
		/// <param name="references">References to other nodes</param>
		/// <param name="type">Type of the node that was written</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Handle to the written node</returns>
		public static async ValueTask<HashedNodeRef<T>> WriteHashedNodeRefAsync<T>(this IStorageWriter writer, int size, IReadOnlyList<BlobHandle> references, BlobType type, CancellationToken cancellationToken = default) where T : Node
		{
			IoHash hash = IoHash.Compute(writer.GetOutputBuffer(size, size).Span.Slice(0, size));
			BlobHandle blobHandle = await WriteBlobAsync(writer, size, references, type, cancellationToken);
			return new HashedNodeRef<T>(hash, blobHandle);
		}
	}
}
