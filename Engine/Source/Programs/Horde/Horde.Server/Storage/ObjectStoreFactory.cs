// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.ObjectStores;
using Horde.Server.Storage.ObjectStores;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Storage
{
	/// <summary>
	/// Default implementation of <see cref="IObjectStoreFactory"/>
	/// </summary>
	class ObjectStoreFactory : IObjectStoreFactory
	{
		class RefCountedObjectStore : IDisposable
		{
			public BackendId Id { get; }
			public IoHash Hash { get; }
			public IObjectStore Inner { get; }

			public int _refCount = 1;

			public RefCountedObjectStore(BackendId id, IoHash hash, IObjectStore inner)
			{
				Id = id;
				Hash = hash;
				Inner = inner;
			}

			public void Dispose()
			{
				Inner.Dispose();
			}
		}

		class ObjectStoreWrapper : IObjectStore
		{
			readonly ObjectStoreFactory _owner;
			RefCountedObjectStore _refCountedObjectStore;
			IObjectStore _inner;

			public ObjectStoreWrapper(ObjectStoreFactory owner, RefCountedObjectStore refCountedObjectStore)
			{
				_owner = owner;
				_refCountedObjectStore = refCountedObjectStore;
				_inner = _refCountedObjectStore.Inner;
			}

			public void Dispose()
			{
				if (_refCountedObjectStore != null)
				{
					_owner.ReleaseObjectStore(_refCountedObjectStore);
					_refCountedObjectStore = null!;

					_inner = null!;
				}
			}

			#region IObjectStore Implementation

			/// <inheritdoc/>
			public bool SupportsRedirects => _inner.SupportsRedirects;

			/// <inheritdoc/>
			public Task<bool> ExistsAsync(ObjectKey key, CancellationToken cancellationToken = default) => _inner.ExistsAsync(key, cancellationToken);

			/// <inheritdoc/>
			public Task DeleteAsync(ObjectKey key, CancellationToken cancellationToken = default) => _inner.DeleteAsync(key, cancellationToken);

			/// <inheritdoc/>
			public IAsyncEnumerable<ObjectKey> EnumerateAsync(CancellationToken cancellationToken = default) => _inner.EnumerateAsync(cancellationToken);

			/// <inheritdoc/>
			public Task<Stream> OpenAsync(ObjectKey key, int offset, int? length, CancellationToken cancellationToken = default)
			{
				return _inner?.OpenAsync(key, offset, length, cancellationToken) ?? throw new InvalidOperationException("Object store has already been disposed");
			}

			/// <inheritdoc/>
			public Task<IReadOnlyMemoryOwner<byte>> ReadAsync(ObjectKey key, int offset, int? length, CancellationToken cancellationToken = default)
			{
				return _inner?.ReadAsync(key, offset, length, cancellationToken) ?? throw new InvalidOperationException("Object store has already been disposed");
			}

			/// <inheritdoc/>
			public Task WriteAsync(ObjectKey key, Stream stream, CancellationToken cancellationToken = default) => _inner.WriteAsync(key, stream, cancellationToken);

			/// <inheritdoc/>
			public ValueTask<Uri?> TryGetReadRedirectAsync(ObjectKey key, CancellationToken cancellationToken = default) => _inner.TryGetReadRedirectAsync(key, cancellationToken);

			/// <inheritdoc/>
			public ValueTask<Uri?> TryGetWriteRedirectAsync(ObjectKey key, CancellationToken cancellationToken = default) => _inner.TryGetWriteRedirectAsync(key, cancellationToken);

			/// <inheritdoc/>
			public void GetStats(StorageStats stats) => _inner.GetStats(stats);

			#endregion
		}

		readonly IServiceProvider _serviceProvider;
		readonly StorageBackendCache _storageBackendCache;
		readonly object _lockObject = new object();
		readonly Dictionary<IoHash, RefCountedObjectStore> _objectStores = new Dictionary<IoHash, RefCountedObjectStore>();
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public ObjectStoreFactory(IServiceProvider serviceProvider, StorageBackendCache storageBackendCache, ILogger<ObjectStoreFactory> logger)
		{
			_serviceProvider = serviceProvider;
			_storageBackendCache = storageBackendCache;
			_logger = logger;
		}

		string GetTrace()
		{
			try
			{
				string trace = String.Join("\n", Environment.StackTrace.Split('\n').Select(x => x.Trim()));
				if (trace.Length > 4096)
				{
					trace = trace.Substring(0, 4096);
				}
				return trace;
			}
			catch (Exception ex)
			{
				return ex.ToString();
			}
		}

		/// <inheritdoc/>
		public IObjectStore CreateObjectStore(BackendConfig config)
		{
			// Compute the new hash of the configuration data
			IoHash hash;
			using (MemoryStream stream = new MemoryStream())
			{
				JsonSerializerOptions options = new JsonSerializerOptions();
				Startup.ConfigureJsonSerializer(options);

				JsonSerializer.Serialize(stream, config, options: options);

				hash = IoHash.Compute(stream.ToArray());
			}

			// See if we've got an existing backend we can use
			RefCountedObjectStore? refCountedObjectStore;
			lock (_lockObject)
			{
				if (_objectStores.TryGetValue(hash, out refCountedObjectStore))
				{
					refCountedObjectStore._refCount++;
					_logger.LogDebug("Adding reference to object store {Id}@{Hash} {Trace}", refCountedObjectStore.Id, hash, GetTrace());
				}
				else
				{
					IObjectStore newBackend = CreateObjectStoreInternal(config);
					refCountedObjectStore = new RefCountedObjectStore(config.Id, hash, newBackend);
					_objectStores.Add(hash, refCountedObjectStore);
					_logger.LogInformation("Created object store {Id}@{Hash} {Trace}", refCountedObjectStore.Id, hash, GetTrace());
				}
			}

			return new ObjectStoreWrapper(this, refCountedObjectStore);
		}

		void ReleaseObjectStore(RefCountedObjectStore backend)
		{
			lock (_lockObject)
			{
				_logger.LogDebug("Releasing object store {Id}@{Hash} {Trace}", backend.Id, backend.Hash, GetTrace());

				if (--backend._refCount == 0)
				{
					_objectStores.Remove(backend.Hash);
					backend.Dispose();
					_logger.LogInformation("Disposed object store {Id}@{Hash}", backend.Id, backend.Hash);
				}
			}
		}

		/// <summary>
		/// Creates a storage backend with the given configuration
		/// </summary>
		/// <param name="config">Configuration for the backend</param>
		/// <returns>New storage backend instance</returns>
		IObjectStore CreateObjectStoreInternal(BackendConfig config)
		{
			switch (config.Type ?? StorageBackendType.FileSystem)
			{
				case StorageBackendType.FileSystem:
					return new FileObjectStore(DirectoryReference.Combine(ServerApp.DataDir, config.BaseDir ?? "Storage"));
				case StorageBackendType.Aws:
					{
#pragma warning disable CA2000 // False positive? (Will be disposed with cache backend wrapper)
						IObjectStore backend = new AwsObjectStore(_serviceProvider.GetRequiredService<IConfiguration>(), config, _serviceProvider.GetRequiredService<ILogger<AwsObjectStore>>());
						return _storageBackendCache.CreateWrapper(config.Id.ToString(), backend);
#pragma warning restore CA2000
					}
				case StorageBackendType.Memory:
					return new MemoryObjectStore();
				default:
					throw new NotImplementedException();
			}
		}
	}
}
