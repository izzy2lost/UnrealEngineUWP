// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Security.Claims;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Horde.Server.Acls;

namespace Horde.Server.Storage
{
	/// <summary>
	/// Interface for storage clients which includes a backend implementation. Some functionality is exposed through the backend which is not part of the regular storage API (eg. enumerating).
	/// </summary>
	public interface IServerStorageClient : IStorageClient
	{
		/// <summary>
		/// Whether the backend supports redirects
		/// </summary>
		bool SupportsRedirects { get; }

		/// <summary>
		/// Authorizes a user to perform a given action
		/// </summary>
		/// <param name="action">The action being performed</param>
		/// <param name="user">The principal to validate</param>
		bool Authorize(AclAction action, ClaimsPrincipal user);

		/// <summary>
		/// Deletes a blob from storage
		/// </summary>
		/// <param name="locator">Locator for the blob to delete</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task DeleteAsync(BlobLocator locator, CancellationToken cancellationToken = default);

		/// <summary>
		/// Enumerate all the blobs available through this client
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Sequence of locators</returns>
		IAsyncEnumerable<BlobLocator> EnumerateAsync(CancellationToken cancellationToken = default);

		/// <summary>
		/// Attempts to get a redirect URL for the given blob
		/// </summary>
		/// <param name="locator">Blob to read</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Optional url to read from</returns>
		ValueTask<Uri?> TryGetReadRedirectAsync(BlobLocator locator, CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets a write redirect for a new blob
		/// </summary>
		/// <param name="prefix">Prefix for the new blob</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Locator for the blob and url to upload to</returns>
		ValueTask<(BlobLocator, Uri)?> TryGetWriteRedirectAsync(string? prefix = null, CancellationToken cancellationToken = default);
	}

	/// <summary>
	/// Implementation of a <see cref="IServerStorageClient"/> which wraps an inner <see cref="IStorageClient"/>
	/// </summary>
	class ServerStorageClientWrapper : IServerStorageClient
	{
		readonly IStorageClient _inner;

		/// <inheritdoc/>
		public bool SupportsRedirects => false;

		public ServerStorageClientWrapper(IStorageClient inner) => _inner = inner;

		/// <inheritdoc/>
		public void Dispose() => _inner.Dispose();

		/// <inheritdoc/>
		public bool Authorize(AclAction action, ClaimsPrincipal user) => true;

		#region Blobs

		/// <inheritdoc/>
		public BlobHandle CreateBlobHandle(BlobLocator blobId) => _inner.CreateBlobHandle(blobId);

		/// <inheritdoc/>
		public IStorageWriter CreateWriter(string? basePath = null) => _inner.CreateWriter(basePath);

		/// <inheritdoc/>
		public Task DeleteAsync(BlobLocator locator, CancellationToken cancellationToken = default) => throw new NotSupportedException();

		/// <inheritdoc/>
		public IAsyncEnumerable<BlobLocator> EnumerateAsync(CancellationToken cancellationToken = default) => throw new NotSupportedException();

		/// <inheritdoc/>
		public ValueTask<Uri?> TryGetReadRedirectAsync(BlobLocator locator, CancellationToken cancellationToken = default) => default;

		/// <inheritdoc/>
		public ValueTask<(BlobLocator, Uri)?> TryGetWriteRedirectAsync(string? prefix = null, CancellationToken cancellationToken = default) => default;

		/// <inheritdoc/>
		public ValueTask<BlobHandle> WriteBlobAsync(BlobType blobType, Stream stream, IReadOnlyList<BlobHandle> references, string? basePath, CancellationToken cancellationToken) => default;

		#endregion

		#region Aliases

		/// <inheritdoc/>
		public Task AddAliasAsync(string name, BlobHandle handle, int rank = 0, ReadOnlyMemory<byte> data = default, CancellationToken cancellationToken = default) => _inner.AddAliasAsync(name, handle, rank, data, cancellationToken);

		/// <inheritdoc/>
		public Task RemoveAliasAsync(string name, BlobHandle handle, CancellationToken cancellationToken = default) => _inner.RemoveAliasAsync(name, handle, cancellationToken);

		/// <inheritdoc/>
		public Task<BlobAlias[]> FindAliasesAsync(string name, int? maxResults = null, CancellationToken cancellationToken = default) => _inner.FindAliasesAsync(name, maxResults, cancellationToken);

		#endregion

		#region Refs

		/// <inheritdoc/>
		public Task<bool> DeleteRefAsync(RefName name, CancellationToken cancellationToken = default) => _inner.DeleteRefAsync(name, cancellationToken);

		/// <inheritdoc/>
		public Task<RefValue?> TryReadRefAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default) => _inner.TryReadRefAsync(name, cacheTime, cancellationToken);

		/// <inheritdoc/>
		public Task WriteRefAsync(RefName name, BlobHandle handle, ReadOnlyMemory<byte> data = default, RefOptions? options = null, CancellationToken cancellationToken = default) => _inner.WriteRefAsync(name, handle, data, options, cancellationToken);

		#endregion

		/// <inheritdoc/>
		public void GetStats(StorageStats stats) => _inner.GetStats(stats);
	}

	/// <summary>
	/// Reference counted wrapper around a <see cref="IServerStorageClient"/>
	/// </summary>
	sealed class SharedServerStorageClient : IServerStorageClient
	{
		class RefCount
		{
			public int _value = 1;
		}

		readonly IServerStorageClient _inner;
		RefCount? _refCount;

		/// <inheritdoc/>
		public bool SupportsRedirects => _inner.SupportsRedirects;

		/// <summary>
		/// Constructor
		/// </summary>
		public SharedServerStorageClient(IServerStorageClient inner)
			: this(inner, new RefCount())
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		private SharedServerStorageClient(IServerStorageClient inner, RefCount refCount)
		{
			_inner = inner;
			_refCount = refCount;
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			if (_refCount != null)
			{
				if (Interlocked.Decrement(ref _refCount._value) == 0)
				{
					_inner.Dispose();
				}
				_refCount = null;
			}
		}

		/// <summary>
		/// Creates a new reference to 
		/// </summary>
		/// <returns></returns>
		public SharedServerStorageClient AddRef()
		{
			if (_refCount == null)
			{
				throw new ObjectDisposedException(ToString());
			}

			Interlocked.Increment(ref _refCount._value);
			return new SharedServerStorageClient(_inner, _refCount);
		}

		/// <inheritdoc/>
		public bool Authorize(AclAction action, ClaimsPrincipal user) => _inner.Authorize(action, user);

		#region Blobs

		/// <inheritdoc/>
		public BlobHandle CreateBlobHandle(BlobLocator locator) => _inner.CreateBlobHandle(locator);

		/// <inheritdoc/>
		public IStorageWriter CreateWriter(string? basePath = null) => _inner.CreateWriter(basePath);

		/// <inheritdoc/>
		public Task DeleteAsync(BlobLocator locator, CancellationToken cancellationToken) => _inner.DeleteAsync(locator, cancellationToken);

		/// <inheritdoc/>
		public IAsyncEnumerable<BlobLocator> EnumerateAsync(CancellationToken cancellationToken) => _inner.EnumerateAsync(cancellationToken);

		/// <inheritdoc/>
		public ValueTask<Uri?> TryGetReadRedirectAsync(BlobLocator locator, CancellationToken cancellationToken = default) => _inner.TryGetReadRedirectAsync(locator, cancellationToken);

		/// <inheritdoc/>
		public ValueTask<(BlobLocator, Uri)?> TryGetWriteRedirectAsync(string? prefix = null, CancellationToken cancellationToken = default) => _inner.TryGetWriteRedirectAsync(prefix, cancellationToken);

		/// <inheritdoc/>
		public ValueTask<BlobHandle> WriteBlobAsync(BlobType blobType, Stream stream, IReadOnlyList<BlobHandle> references, string? basePath, CancellationToken cancellationToken) => _inner.WriteBlobAsync(blobType, stream, references, basePath, cancellationToken);

		#endregion

		#region Aliases

		/// <inheritdoc/>
		public Task AddAliasAsync(string name, BlobHandle handle, int rank = 0, ReadOnlyMemory<byte> data = default, CancellationToken cancellationToken = default) => _inner.AddAliasAsync(name, handle, rank, data, cancellationToken);

		/// <inheritdoc/>
		public Task RemoveAliasAsync(string name, BlobHandle handle, CancellationToken cancellationToken = default) => _inner.RemoveAliasAsync(name, handle, cancellationToken);

		/// <inheritdoc/>
		public Task<BlobAlias[]> FindAliasesAsync(string name, int? maxResults = null, CancellationToken cancellationToken = default) => _inner.FindAliasesAsync(name, maxResults, cancellationToken);

		#endregion

		#region Refs

		/// <inheritdoc/>
		public Task<bool> DeleteRefAsync(RefName name, CancellationToken cancellationToken = default) => _inner.DeleteRefAsync(name, cancellationToken);

		/// <inheritdoc/>
		public Task<RefValue?> TryReadRefAsync(RefName name, RefCacheTime cacheTime, CancellationToken cancellationToken) => _inner.TryReadRefAsync(name, cacheTime, cancellationToken);

		/// <inheritdoc/>
		public Task WriteRefAsync(RefName name, BlobHandle handle, ReadOnlyMemory<byte> data = default, RefOptions? options = null, CancellationToken cancellationToken = default) => _inner.WriteRefAsync(name, handle, data, options, cancellationToken);

		#endregion

		/// <inheritdoc/>
		public void GetStats(StorageStats stats) => _inner.GetStats(stats);
	}
}
