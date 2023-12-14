// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Backends;
using Horde.Server.Storage.Backends;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Storage
{
	/// <summary>
	/// Default implementation of <see cref="IStorageBackendProvider"/>
	/// </summary>
	class StorageBackendProvider : IStorageBackendProvider
	{
		class RefCountedBackend : IDisposable
		{
			public BackendId Id { get; }
			public IoHash Hash { get; }
			public IStorageBackend Backend { get; }

			public int _refCount = 1;

			public RefCountedBackend(BackendId id, IoHash hash, IStorageBackend backend)
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

		class BackendWrapper : IStorageBackend
		{
			readonly StorageBackendProvider _owner;
			RefCountedBackend _refCountedBackend;
			IStorageBackend _backend;

			public BackendWrapper(StorageBackendProvider owner, RefCountedBackend refCountedBackend)
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

			#region IStorageBackend Implementation

			/// <inheritdoc/>
			public bool SupportsRedirects => _backend.SupportsRedirects;

			/// <inheritdoc/>
			public Task<bool> ExistsAsync(string path, CancellationToken cancellationToken = default) => _backend.ExistsAsync(path, cancellationToken);

			/// <inheritdoc/>
			public Task DeleteAsync(string path, CancellationToken cancellationToken = default) => _backend.DeleteAsync(path, cancellationToken);

			/// <inheritdoc/>
			public IAsyncEnumerable<string> EnumerateAsync(CancellationToken cancellationToken = default) => _backend.EnumerateAsync(cancellationToken);

			/// <inheritdoc/>
			public Task<Stream> OpenAsync(string path, int offset, int? length, CancellationToken cancellationToken = default)
			{
				return _backend?.OpenAsync(path, offset, length, cancellationToken) ?? throw new InvalidOperationException("Backend has already been disposed");
			}

			/// <inheritdoc/>
			public Task<IReadOnlyMemoryOwner<byte>> ReadAsync(string path, int offset, int? length, CancellationToken cancellationToken = default)
			{
				return _backend?.ReadAsync(path, offset, length, cancellationToken) ?? throw new InvalidOperationException("Backend has already been disposed");
			}

			/// <inheritdoc/>
			public Task<string> WriteAsync(Stream stream, string? prefix = null, CancellationToken cancellationToken = default) => _backend.WriteAsync(stream, prefix, cancellationToken);

#pragma warning disable CS0618
			/// <inheritdoc/>
			public Task WriteExplicitPathAsync(string path, Stream stream, CancellationToken cancellationToken = default) => _backend.WriteExplicitPathAsync(path, stream, cancellationToken);
#pragma warning restore CS0618

			/// <inheritdoc/>
			public ValueTask<Uri?> TryGetReadRedirectAsync(string path, CancellationToken cancellationToken = default) => _backend.TryGetReadRedirectAsync(path, cancellationToken);

			/// <inheritdoc/>
			public ValueTask<(string, Uri)?> TryGetWriteRedirectAsync(string? prefix = null, CancellationToken cancellationToken = default) => _backend.TryGetWriteRedirectAsync(prefix, cancellationToken);

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
		public StorageBackendProvider(IServiceProvider serviceProvider, StorageBackendCache storageBackendCache, ILogger<StorageBackendProvider> logger)
		{
			_serviceProvider = serviceProvider;
			_storageBackendCache = storageBackendCache;
			_logger = logger;
		}

		/// <inheritdoc/>
		public IStorageBackend CreateBackend(BackendConfig config)
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
					IStorageBackend newBackend = CreateStorageBackend(config);
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
		IStorageBackend CreateStorageBackend(BackendConfig config)
		{
			switch (config.Type ?? StorageBackendType.FileSystem)
			{
				case StorageBackendType.FileSystem:
					return new FileStorageBackend(DirectoryReference.Combine(ServerApp.DataDir, config.BaseDir ?? "Storage"));
				case StorageBackendType.Aws:
					{
#pragma warning disable CA2000 // False positive? (Will be disposed with cache backend wrapper)
						IStorageBackend backend = new AwsStorageBackend(_serviceProvider.GetRequiredService<IConfiguration>(), config, _serviceProvider.GetRequiredService<ILogger<AwsStorageBackend>>());
						return _storageBackendCache.CreateWrapper(config.Id.ToString(), backend);
#pragma warning restore CA2000
					}
				case StorageBackendType.Memory:
					return new MemoryStorageBackend();
				default:
					throw new NotImplementedException();
			}
		}
	}
}
