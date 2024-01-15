// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
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
		class RefCountedBackend : IDisposable
		{
			public BackendId Id { get; }
			public IoHash Hash { get; }
			public IObjectStore Backend { get; }

			public int _refCount = 1;

			public RefCountedBackend(BackendId id, IoHash hash, IObjectStore backend)
			{
				Id = id;
				Hash = hash;
				Backend = backend;
			}

			public void Dispose()
			{
				Backend.Dispose();
			}
		}

		class BackendWrapper : IObjectStore
		{
			readonly ObjectStoreFactory _owner;
			RefCountedBackend _refCountedBackend;
			IObjectStore _backend;

			public BackendWrapper(ObjectStoreFactory owner, RefCountedBackend refCountedBackend)
			{
				_owner = owner;
				_refCountedBackend = refCountedBackend;
				_backend = _refCountedBackend.Backend;
			}

			public void Dispose()
			{
				if (_refCountedBackend != null)
				{
					_owner.ReleaseBackend(_refCountedBackend);
					_refCountedBackend = null!;

					_backend = null!;
				}
			}

			#region IObjectStore Implementation

			/// <inheritdoc/>
			public bool SupportsRedirects => _backend.SupportsRedirects;

			/// <inheritdoc/>
			public Task<bool> ExistsAsync(ObjectKey key, CancellationToken cancellationToken = default) => _backend.ExistsAsync(key, cancellationToken);

			/// <inheritdoc/>
			public Task DeleteAsync(ObjectKey key, CancellationToken cancellationToken = default) => _backend.DeleteAsync(key, cancellationToken);

			/// <inheritdoc/>
			public IAsyncEnumerable<ObjectKey> EnumerateAsync(CancellationToken cancellationToken = default) => _backend.EnumerateAsync(cancellationToken);

			/// <inheritdoc/>
			public Task<Stream> OpenAsync(ObjectKey key, int offset, int? length, CancellationToken cancellationToken = default)
			{
				return _backend?.OpenAsync(key, offset, length, cancellationToken) ?? throw new InvalidOperationException("Backend has already been disposed");
			}

			/// <inheritdoc/>
			public Task<IReadOnlyMemoryOwner<byte>> ReadAsync(ObjectKey key, int offset, int? length, CancellationToken cancellationToken = default)
			{
				return _backend?.ReadAsync(key, offset, length, cancellationToken) ?? throw new InvalidOperationException("Backend has already been disposed");
			}

			/// <inheritdoc/>
			public Task WriteAsync(ObjectKey key, Stream stream, CancellationToken cancellationToken = default) => _backend.WriteAsync(key, stream, cancellationToken);

			/// <inheritdoc/>
			public ValueTask<Uri?> TryGetReadRedirectAsync(ObjectKey key, CancellationToken cancellationToken = default) => _backend.TryGetReadRedirectAsync(key, cancellationToken);

			/// <inheritdoc/>
			public ValueTask<Uri?> TryGetWriteRedirectAsync(ObjectKey key, CancellationToken cancellationToken = default) => _backend.TryGetWriteRedirectAsync(key, cancellationToken);

			/// <inheritdoc/>
			public void GetStats(StorageStats stats) => _backend.GetStats(stats);

			#endregion
		}

		readonly IServiceProvider _serviceProvider;
		readonly StorageBackendCache _storageBackendCache;
		readonly object _lockObject = new object();
		readonly Dictionary<IoHash, RefCountedBackend> _backends = new Dictionary<IoHash, RefCountedBackend>();
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
			RefCountedBackend? refCountedBackend;
			lock (_lockObject)
			{
				if (_backends.TryGetValue(hash, out refCountedBackend))
				{
					refCountedBackend._refCount++;
					_logger.LogDebug("Adding reference to storage backend {Id}@{Hash}", refCountedBackend.Id, hash);
				}
				else
				{
					IObjectStore newBackend = CreateStorageBackend(config);
					refCountedBackend = new RefCountedBackend(config.Id, hash, newBackend);
					_backends.Add(hash, refCountedBackend);
					_logger.LogInformation("Created storage backend {Id}@{Hash}", refCountedBackend.Id, hash);
				}
			}

			return new BackendWrapper(this, refCountedBackend);
		}

		void ReleaseBackend(RefCountedBackend backend)
		{
			lock (_lockObject)
			{
				_logger.LogDebug("Releasing storage backend {Id}@{Hash}", backend.Id, backend.Hash);

				if (--backend._refCount == 0)
				{
					_backends.Remove(backend.Hash);
					backend.Dispose();
					_logger.LogInformation("Disposed storage backend {Id}@{Hash}", backend.Id, backend.Hash);
				}
			}
		}

		/// <summary>
		/// Creates a storage backend with the given configuration
		/// </summary>
		/// <param name="config">Configuration for the backend</param>
		/// <returns>New storage backend instance</returns>
		IObjectStore CreateStorageBackend(BackendConfig config)
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
