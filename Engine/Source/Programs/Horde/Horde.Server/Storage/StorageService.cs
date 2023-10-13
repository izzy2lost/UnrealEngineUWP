// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Security.Claims;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Bundles;
using EpicGames.Horde.Storage.Clients;
using EpicGames.Horde.Storage.Nodes;
using EpicGames.Redis;
using EpicGames.Redis.Utility;
using Horde.Server.Acls;
using Horde.Server.Server;
using Horde.Server.Utilities;
using HordeCommon;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using MongoDB.Bson;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;
using OpenTelemetry.Trace;
using StackExchange.Redis;

namespace Horde.Server.Storage
{
	/// <summary>
	/// Exception thrown by the <see cref="StorageService"/>
	/// </summary>
	public sealed class StorageException : Exception
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public StorageException(string message, Exception? inner = null)
			: base(message, inner)
		{
		}
	}

	/// <summary>
	/// Interface for storage clients which includes a backend implementation. Some functionality is exposed through the backend which is not part of the regular storage API (eg. enumerating).
	/// </summary>
	public interface IServerStorageClient : IBundleStorageClient
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
	}

	/// <summary>
	/// Functionality related to the storage service
	/// </summary>
	public sealed class StorageService : IHostedService, IAsyncDisposable, IStorageClientFactory
	{
		sealed class StorageBackendImpl : IStorageBackend
		{
			readonly StorageService _outer;
			readonly NamespaceId _namespaceId;
			readonly string _prefix;
			readonly IStorageBackend _inner;
			readonly Tracer _tracer;

			public StorageBackendImpl(StorageService outer, NamespaceId namespaceId, string prefix, IStorageBackend inner, Tracer tracer)
			{
				_outer = outer;
				_namespaceId = namespaceId;
				_prefix = prefix;
				_inner = inner;
				_tracer = tracer;
			}

			public void Dispose()
			{
				_inner.Dispose();
			}

			/// <inheritdoc/>
			public bool SupportsRedirects => _inner.SupportsRedirects;

			/// <inheritdoc/>
			public async Task<Stream> OpenAsync(string path, int offset, int? length, CancellationToken cancellationToken = default)
			{
				string fullPath = $"{_prefix}{path}";

				using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(StorageService)}.{nameof(StorageClientImpl)}.{nameof(OpenAsync)}");
				span.SetAttribute("path", fullPath);
				span.SetAttribute("offset", offset);
				span.SetAttribute("length", length);

				if (length.HasValue && length.Value == 0)
				{
					return new MemoryStream(Array.Empty<byte>());
				}

				return await _inner.OpenAsync(fullPath, offset, length, cancellationToken);
			}

			/// <inheritdoc/>
			public async Task<IStorageObject> ReadAsync(string path, int offset, int? length, CancellationToken cancellationToken = default)
			{
				string fullPath = $"{_prefix}{path}";
				return await _inner.ReadAsync(fullPath, offset, length, cancellationToken);
			}

			/// <inheritdoc/>
			public async Task<string> WriteAsync(Stream stream, string? prefix = null, CancellationToken cancellationToken = default)
			{
				string path = await _inner.WriteAsync(stream, $"{_prefix}{prefix}", cancellationToken);

				BundleLocator locator = new BundleLocator(path);
				await _outer.AddBlobAsync(_namespaceId, locator, null, cancellationToken);

				return path;
			}

			/// <inheritdoc/>
			public Task WriteExplicitPathAsync(string path, Stream stream, CancellationToken cancellationToken = default) => throw new NotSupportedException();

			/// <inheritdoc/>
			public Task<bool> ExistsAsync(string path, CancellationToken cancellationToken = default) => _inner.ExistsAsync($"{_prefix}{path}", cancellationToken);

			/// <inheritdoc/>
			public Task DeleteAsync(string path, CancellationToken cancellationToken = default) => _inner.DeleteAsync($"{_prefix}{path}", cancellationToken);

			/// <inheritdoc/>
			public async IAsyncEnumerable<string> EnumerateAsync([EnumeratorCancellation] CancellationToken cancellationToken = default)
			{
				await foreach (string path in _inner.EnumerateAsync(cancellationToken))
				{
					if (path.StartsWith(_prefix, StringComparison.Ordinal))
					{
						yield return path.Substring(_prefix.Length);
					}
				}
			}

			/// <inheritdoc/>
			public ValueTask<Uri?> TryGetReadRedirectAsync(string path, CancellationToken cancellationToken = default)
			{
				return _inner.TryGetReadRedirectAsync($"{_prefix}{path}", cancellationToken);
			}

			/// <inheritdoc/>
			public async ValueTask<(string, Uri)?> TryGetWriteRedirectAsync(string? prefix = null, CancellationToken cancellationToken = default)
			{
				if (!_inner.SupportsRedirects)
				{
					return null;
				}

				(string Path, Uri Url)? redirect = await _inner.TryGetWriteRedirectAsync(prefix, cancellationToken);
				if (redirect == null)
				{
					return null;
				}

				BundleLocator locator = new BundleLocator(redirect.Value.Path);
				await _outer.AddBlobAsync(_namespaceId, locator, null, cancellationToken);

				return redirect;
			}

			/// <inheritdoc/>
			public void GetStats(StorageStats stats) { }
		}

		sealed class StorageClientImpl : BundleStorageClient
		{
			readonly StorageService _outer;
			int _refCount = 1;

			public NamespaceConfig Config { get; }
			public NamespaceId NamespaceId { get; }
			public bool SupportsRedirects { get; }

			public StorageClientImpl(StorageService outer, NamespaceConfig config, IStorageBackend backend, BundleReaderCache bundleReaderCache, ILogger logger)
				: base(backend, bundleReaderCache, logger)
			{
				_outer = outer;

				Config = config;
				NamespaceId = config.Id;
				SupportsRedirects = backend.SupportsRedirects && !config.EnableAliases;
			}

			public void AddRef()
			{
				Interlocked.Increment(ref _refCount);
			}

			public void Release()
			{
				if (Interlocked.Decrement(ref _refCount) == 0)
				{
					Dispose();
				}
			}

			#region Aliases

			/// <inheritdoc/>
			public override Task AddAliasAsync(string name, BundleNodeLocator locator, int rank = 0, ReadOnlyMemory<byte> data = default, CancellationToken cancellationToken = default) => _outer.AddAliasAsync(NamespaceId, name, locator, rank, data, cancellationToken);

			/// <inheritdoc/>
			public override Task RemoveAliasAsync(string name, BundleNodeLocator locator, CancellationToken cancellationToken = default) => _outer.RemoveAliasAsync(NamespaceId, name, locator, cancellationToken);

			/// <inheritdoc/>
			public override async Task<BlobAlias[]> FindAliasesAsync(string alias, int? maxResults, CancellationToken cancellationToken = default)
			{
				List<(BundleNodeLocator, AliasInfo)> aliases = await _outer.FindAliasesAsync(NamespaceId, alias, cancellationToken);
				if (maxResults != null && maxResults.Value < aliases.Count)
				{
					aliases.RemoveRange(maxResults.Value, aliases.Count - maxResults.Value);
				}
				return aliases.Select(x => new BlobAlias(CreateNodeHandle(x.Item1), x.Item2.Rank, x.Item2.Data)).ToArray();
			}

			#endregion

			#region Refs

			/// <inheritdoc/>
			public override async Task<RefValue?> TryReadRefAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default)
			{
				RefInfo? result = await _outer.TryReadRefAsync(NamespaceId, name, cacheTime, cancellationToken);
				if (result == null)
				{
					return null;
				}

				EncodedBlobData packet = new EncodedBlobData(result.Data);
				if (packet.Type != Node.GetNodeType<RedirectNode>())
				{
					throw new InvalidDataException($"Expected redirect node in ref {name}");
				}

				BlobLocator locator = packet.Refs[0];
				return new RefValue(CreateBlobHandle(locator), ReadOnlyMemory<byte>.Empty);
			}

			/// <inheritdoc/>
			public override async Task WriteRefAsync(RefName name, BundleNodeLocator locator, ReadOnlyMemory<byte> data = default, RefOptions? options = null, CancellationToken cancellationToken = default)
			{
				BlobType blobType = Node.GetNodeType<RedirectNode>();
				BlobHandle blobHandle = CreateBlobHandle(locator.ToBlobLocator());
				using BlobData blobData = new BlobData(blobType, ReadOnlyMemory<byte>.Empty, new List<BlobHandle> { blobHandle });

				await _outer.WriteRefAsync(NamespaceId, name, blobData, options, cancellationToken);
			}

			/// <inheritdoc/>
			public override Task<bool> DeleteRefAsync(RefName name, CancellationToken cancellationToken = default) => _outer.DeleteRefAsync(NamespaceId, name, cancellationToken);

			#endregion
		}

		sealed class StorageClientRef : IServerStorageClient
		{
			StorageClientImpl _impl;

			public StorageClientRef(StorageClientImpl impl)
			{
				_impl = impl;
				_impl.AddRef();
			}

			public void Dispose()
			{
				if (_impl != null)
				{
					_impl.Release();
					_impl = null!;
				}
			}

			public NamespaceConfig Config => _impl.Config;
			public NamespaceId NamespaceId => _impl.NamespaceId;
			public bool SupportsRedirects => _impl.SupportsRedirects;
			public IStorageBackend Backend => _impl.Backend;

			#region Blobs

			public BlobHandle CreateBlobHandle(BlobLocator locator) => _impl.CreateBlobHandle(locator);
			public BundleNodeHandle CreateNodeHandle(BundleNodeLocator locator) => _impl.CreateNodeHandle(locator);

			public BundleWriter CreateWriter(string? basePath = null, BundleOptions? options = null) => _impl.CreateWriter(basePath, options);
			IStorageWriter IStorageClient.CreateWriter(string? basePath) => ((IStorageClient)_impl).CreateWriter(basePath);

			#endregion

			#region Bundles

			public Task<Stream> OpenAsync(BundleLocator locator, int offset, int? length = null, CancellationToken cancellationToken = default) => _impl.OpenAsync(locator, offset, length, cancellationToken);

			public Task<BundleHeader> ReadHeaderAsync(BundleLocator locator, CancellationToken cancellationToken) => _impl.ReadHeaderAsync(locator, cancellationToken);

			#endregion

			#region Aliases

			public Task AddAliasAsync(string name, BundleNodeLocator locator, int rank = 0, ReadOnlyMemory<byte> data = default, CancellationToken cancellationToken = default) => _impl.AddAliasAsync(name, locator, rank, data, cancellationToken);
			public Task AddAliasAsync(string name, BlobHandle handle, int rank = 0, ReadOnlyMemory<byte> data = default, CancellationToken cancellationToken = default) => ((IStorageClient)_impl).AddAliasAsync(name, handle, rank, data, cancellationToken);

			public Task RemoveAliasAsync(string name, BundleNodeLocator locator, CancellationToken cancellationToken = default) => _impl.RemoveAliasAsync(name, locator, cancellationToken);
			public Task RemoveAliasAsync(string name, BlobHandle handle, CancellationToken cancellationToken = default) => ((IStorageClient)_impl).RemoveAliasAsync(name, handle, cancellationToken);

			public Task<BlobAlias[]> FindAliasesAsync(string name, int? maxResults = null, CancellationToken cancellationToken = default) => _impl.FindAliasesAsync(name, maxResults, cancellationToken);

			#endregion

			#region Refs

			public Task<bool> DeleteRefAsync(RefName name, CancellationToken cancellationToken = default) => _impl.DeleteRefAsync(name, cancellationToken);
			public Task<RefValue?> TryReadRefAsync(RefName name, RefCacheTime cacheTime, CancellationToken cancellationToken) => _impl.TryReadRefAsync(name, cacheTime, cancellationToken);

			public Task WriteRefAsync(RefName name, BundleNodeLocator target, RefOptions? options = null, CancellationToken cancellationToken = default) => _impl.WriteRefAsync(name, target, options: options, cancellationToken: cancellationToken);
			public async Task WriteRefAsync(RefName name, BlobHandle handle, ReadOnlyMemory<byte> data = default, RefOptions? options = null, CancellationToken cancellationToken = default)
			{
				await handle.FlushAsync(cancellationToken);
				await _impl.WriteRefAsync(name, ((BundleNodeHandle)handle).GetLocator(), data, options, cancellationToken);
			}

			#endregion

			/// <inheritdoc/>
			public void GetStats(StorageStats stats) => _impl.GetStats(stats);

			public bool Authorize(AclAction action, ClaimsPrincipal user) => _impl.Config.Authorize(action, user);
		}

		class State : IDisposable
		{
			public StorageConfig Config { get; }
			public Dictionary<NamespaceId, StorageClientImpl> Namespaces { get; } = new Dictionary<NamespaceId, StorageClientImpl>();

			public State(StorageConfig config)
			{
				Config = config;
			}

			public void Dispose()
			{
				foreach (StorageClientImpl client in Namespaces.Values)
				{
					client.Release();
				}
			}
		}

		class AliasInfo
		{
			[BsonElement("alias")]
			public string Alias { get; set; } = String.Empty;

			[BsonElement("rank"), BsonIgnoreIfDefault]
			public int Rank { get; set; }

			[BsonElement("data"), BsonIgnoreIfNull]
			public byte[]? Data { get; set; }

			[BsonElement("idx")]
			public int Index { get; set; }

			public AliasInfo()
			{
			}

			public AliasInfo(string alias, int index, byte[]? data, int rank)
			{
				Alias = alias;
				Index = index;
				Rank = rank;
				Data = (data == null || data.Length == 0) ? null : data;
			}
		}

		class BlobInfo
		{
			public ObjectId Id { get; set; }

			[BsonElement("ns")]
			public NamespaceId NamespaceId { get; set; }

			[BsonElement("blob")]
			public string Path { get; set; }

			[BsonElement("imp"), BsonIgnoreIfNull]
			public List<ObjectId>? Imports { get; set; }

			[BsonElement("exp"), BsonIgnoreIfNull]
			public List<AliasInfo>? Aliases { get; set; }

			[BsonIgnore]
			public BundleLocator Locator => new BundleLocator(Path);

			public BlobInfo()
			{
				Path = String.Empty;
			}

			public BlobInfo(ObjectId id, NamespaceId namespaceId, BundleLocator locator)
			{
				Id = id;
				NamespaceId = namespaceId;
				Path = locator.Path.ToString();
			}
		}

		class RefInfo : ISupportInitialize
		{
			[BsonIgnoreIfDefault]
			public ObjectId Id { get; set; }

			[BsonElement("ns")]
			public NamespaceId NamespaceId { get; set; }

			[BsonElement("name")]
			public RefName Name { get; set; }

			[BsonElement("data")]
			public byte[] Data { get; set; } = Array.Empty<byte>();

			[BsonElement("binf"), BsonIgnoreIfNull]
			public ObjectId? LegacyImport { get; set; }

			[BsonElement("imp")]
			public List<ObjectId> Imports { get; set; } = new List<ObjectId>();

			[BsonElement("xa"), BsonIgnoreIfDefault]
			public DateTime? ExpiresAtUtc { get; set; }

			[BsonElement("xt"), BsonIgnoreIfDefault]
			public TimeSpan? Lifetime { get; set; }

#pragma warning disable IDE0051 // Remove unused private members
			[BsonExtraElements]
			BsonDocument? ExtraElements { get; set; }
#pragma warning restore IDE0051 // Remove unused private members

			[BsonConstructor]
			public RefInfo()
			{
				Name = RefName.Empty;
			}

			public RefInfo(NamespaceId namespaceId, RefName name, byte[] data, IReadOnlyList<ObjectId> imports)
			{
				NamespaceId = namespaceId;
				Name = name;
				Data = data;
				Imports = new List<ObjectId>(imports);
			}

			public bool HasExpired(DateTime utcNow) => ExpiresAtUtc.HasValue && utcNow >= ExpiresAtUtc.Value;

			public bool RequiresTouch(DateTime utcNow) => ExpiresAtUtc.HasValue && Lifetime.HasValue && utcNow >= ExpiresAtUtc.Value - new TimeSpan(Lifetime.Value.Ticks / 4);

			void ISupportInitialize.BeginInit() { }

			void ISupportInitialize.EndInit()
			{
				if (ExtraElements != null)
				{
					if (ExtraElements.TryGetValue("blob", out BsonValue blob) && ExtraElements.TryGetValue("idx", out BsonValue idx))
					{
						BlobLocator locator = new BlobLocator($"{blob.AsString}#{idx.AsInt32}");
						Data = EncodedBlobData.Create(Node.GetNodeType<RedirectNode>(), new[] { locator }, ReadOnlyMemory<byte>.Empty);
					}
					ExtraElements = null;
				}
				if (LegacyImport != null)
				{
					Imports.Add(LegacyImport.Value);
					LegacyImport = null;
				}
			}
		}

		[SingletonDocument("gc-state")]
		class GcState : SingletonBase
		{
			public ObjectId LastImportBlobInfoId { get; set; }
			public List<GcNamespaceState> Namespaces { get; set; } = new List<GcNamespaceState>();

			public GcNamespaceState FindOrAddNamespace(NamespaceId namespaceId)
			{
				GcNamespaceState? namespaceState = Namespaces.FirstOrDefault(x => x.Id == namespaceId);
				if (namespaceState == null)
				{
					namespaceState = new GcNamespaceState { Id = namespaceId, LastTime = DateTime.UtcNow };
					Namespaces.Add(namespaceState);
				}
				return namespaceState;
			}
		}

		class GcNamespaceState
		{
			public NamespaceId Id { get; set; }
			public DateTime LastTime { get; set; }
		}

		readonly RedisService _redisService;
		readonly IClock _clock;
		readonly BundleReaderCache _bundleReaderCache;
		readonly IMemoryCache _memoryCache;
		readonly IStorageBackendProvider _storageBackendProvider;
		readonly IOptionsMonitor<GlobalConfig> _globalConfig;
		readonly Tracer _tracer;
		readonly ILogger _logger;

		readonly IMongoCollection<BlobInfo> _blobCollection;
		readonly IMongoCollection<RefInfo> _refCollection;

		readonly ITicker _blobTicker;
		readonly ITicker _refTicker;

		readonly SingletonDocument<GcState> _gcState;
		readonly ITicker _gcTicker;

		readonly object _lockObject = new object();

		string? _lastConfigRevision;
		State? _lastState;

		/// <summary>
		/// Constructor
		/// </summary>
		public StorageService(MongoService mongoService, RedisService redisService, IClock clock, BundleReaderCache bundleReaderCache, IMemoryCache memoryCache, IStorageBackendProvider storageBackendProvider, IOptionsMonitor<GlobalConfig> globalConfig, Tracer tracer, ILogger<StorageService> logger)
		{
			_redisService = redisService;
			_clock = clock;
			_bundleReaderCache = bundleReaderCache;
			_memoryCache = memoryCache;
			_storageBackendProvider = storageBackendProvider;
			_globalConfig = globalConfig;
			_tracer = tracer;
			_logger = logger;

			List<MongoIndex<BlobInfo>> blobIndexes = new List<MongoIndex<BlobInfo>>();
			blobIndexes.Add(keys => keys.Ascending(x => x.Imports));
			blobIndexes.Add(keys => keys.Ascending(x => x.NamespaceId).Ascending(x => x.Path), unique: true);
			blobIndexes.Add(keys => keys.Ascending(x => x.NamespaceId).Ascending($"{nameof(BlobInfo.Aliases)}.{nameof(AliasInfo.Alias)}"));
			_blobCollection = mongoService.GetCollection<BlobInfo>("Storage.Blobs", blobIndexes);

			List<MongoIndex<RefInfo>> refIndexes = new List<MongoIndex<RefInfo>>();
			refIndexes.Add(keys => keys.Ascending(x => x.NamespaceId).Ascending(x => x.Name), unique: true);
			refIndexes.Add(keys => keys.Ascending(x => x.LegacyImport));
			refIndexes.Add(keys => keys.Ascending(x => x.Imports));
			_refCollection = mongoService.GetCollection<RefInfo>("Storage.Refs", refIndexes);

			_blobTicker = clock.AddSharedTicker("Storage:Blobs", TimeSpan.FromMinutes(5.0), TickBlobsAsync, _logger);
			_refTicker = clock.AddSharedTicker("Storage:Refs", TimeSpan.FromMinutes(5.0), TickRefsAsync, _logger);

			_gcState = new SingletonDocument<GcState>(mongoService);
			_gcTicker = clock.AddTicker("Storage:GC", TimeSpan.FromMinutes(5.0), TickGcAsync, logger);
		}

		/// <inheritdoc/>
		public async ValueTask DisposeAsync()
		{
			if (_lastState != null)
			{
				_lastState.Dispose();
				_lastState = null;
			}

			await _blobTicker.DisposeAsync();
			await _refTicker.DisposeAsync();
			await _gcTicker.DisposeAsync();
		}

		/// <inheritdoc/>
		public async Task StartAsync(CancellationToken cancellationToken)
		{
			await _blobTicker.StartAsync();
			await _refTicker.StartAsync();
			await _gcTicker.StartAsync();
		}

		/// <inheritdoc/>
		public async Task StopAsync(CancellationToken cancellationToken)
		{
			await _gcTicker.StopAsync();
			await _refTicker.StopAsync();
			await _blobTicker.StopAsync();
		}

		/// <summary>
		/// Gets a storage client for the given namespace
		/// </summary>
		/// <param name="namespaceId">Namespace identifier</param>
		public IServerStorageClient CreateClient(NamespaceId namespaceId)
		{
			lock (_lockObject)
			{
				State state = GetNextState();
				StorageClientImpl impl = state.Namespaces[namespaceId];
				return new StorageClientRef(impl);
			}
		}

		/// <summary>
		/// Attempts to gets a storage client for the given namespace
		/// </summary>
		/// <param name="namespaceId">Namespace identifier</param>
		public IServerStorageClient? TryCreateClient(NamespaceId namespaceId)
		{
			lock (_lockObject)
			{
				State state = GetNextState();
				if (_lastState!.Namespaces.TryGetValue(namespaceId, out StorageClientImpl? impl))
				{
					return new StorageClientRef(impl);
				}
				else
				{
					return null;
				}
			}
		}

		/// <inheritdoc/>
		IStorageClient IStorageClientFactory.CreateClient(NamespaceId namespaceId) => CreateClient(namespaceId);

		#region Config

		State GetNextState()
		{
			GlobalConfig globalConfig = _globalConfig.CurrentValue;
			if (_lastState == null || !String.Equals(_lastConfigRevision, globalConfig.Revision, StringComparison.Ordinal))
			{
				StorageConfig storageConfig = globalConfig.Storage;

				// Configure the new clients
				State nextState = new State(storageConfig);
				try
				{
					foreach (NamespaceConfig namespaceConfig in storageConfig.Namespaces)
					{
						string prefix = namespaceConfig.Prefix;
						if (prefix.Length > 0 && !prefix.EndsWith("/", StringComparison.Ordinal))
						{
							prefix += "/";
						}

						IStorageBackend backend = _storageBackendProvider.CreateBackend(namespaceConfig.BackendConfig);

#pragma warning disable CA2000 // Dispose objects before losing scope (false positive?)
						StorageBackendImpl backendImpl = new StorageBackendImpl(this, namespaceConfig.Id, prefix, backend, _tracer);
#pragma warning restore CA2000 // Dispose objects before losing scope
						StorageClientImpl clientImpl = new StorageClientImpl(this, namespaceConfig, backendImpl, _bundleReaderCache, _logger);
						nextState.Namespaces.Add(namespaceConfig.Id, clientImpl);
					}
				}
				catch
				{
					nextState.Dispose();
					throw;
				}

				_lastState?.Dispose();
				_lastState = nextState;

				_lastConfigRevision = globalConfig.Revision;
			}
			return _lastState;
		}

		#endregion

		#region Blobs

		/// <inheritdoc/>
		async Task AddBlobAsync(NamespaceId namespaceId, BundleLocator locator, List<AliasInfo>? exports = null, CancellationToken cancellationToken = default)
		{
			ObjectId id = ObjectId.GenerateNewId(_clock.UtcNow);
			BlobInfo blobInfo = new BlobInfo(id, namespaceId, locator);
			blobInfo.Aliases = exports;
			await _blobCollection.InsertOneAsync(blobInfo, new InsertOneOptions { }, cancellationToken);
		}

		/// <inheritdoc/>
		async Task<bool> IsBlobReferencedAsync(ObjectId blobInfoId, CancellationToken cancellationToken = default)
		{
			FilterDefinition<BlobInfo> blobFilter = Builders<BlobInfo>.Filter.AnyEq(x => x.Imports, blobInfoId);
			if (await _blobCollection.Find(blobFilter).AnyAsync(cancellationToken))
			{
				return true;
			}

			FilterDefinition<RefInfo> refLegacyFilter = Builders<RefInfo>.Filter.Eq(x => x.LegacyImport, blobInfoId);
			if (await _refCollection.Find(refLegacyFilter).AnyAsync(cancellationToken))
			{
				return true;
			}

			FilterDefinition<RefInfo> refFilter = Builders<RefInfo>.Filter.AnyEq(x => x.Imports, blobInfoId);
			if (await _refCollection.Find(refFilter).AnyAsync(cancellationToken))
			{
				return true;
			}

			return false;
		}

		/// <summary>
		/// Finds blobs at least 30 minutes old and computes import metadata for them. Done with a delay to allow write redirects.
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		async ValueTask TickBlobsAsync(CancellationToken cancellationToken)
		{
			GcState state = await _gcState.GetAsync();
			DateTime utcNow = _clock.UtcNow;

			// Cached storage clients for each namespace
			Dictionary<NamespaceId, IServerStorageClient?> namespaceIdToClient = new Dictionary<NamespaceId, IServerStorageClient?>();
			try
			{
				// Compute missing import info, by searching for blobs with an ObjectId timestamp after the last import compute cycle
				ObjectId latestInfoId = ObjectId.GenerateNewId(utcNow - TimeSpan.FromMinutes(30.0));
				using (IAsyncCursor<BlobInfo> cursor = await _blobCollection.Find(x => x.Id >= state.LastImportBlobInfoId && x.Id < latestInfoId).ToCursorAsync(cancellationToken))
				{
					while (await cursor.MoveNextAsync(cancellationToken))
					{
						// Find imports, and add a check record for each new blob
						foreach (BlobInfo blobInfo in cursor.Current)
						{
							IServerStorageClient? client;
							if (!namespaceIdToClient.TryGetValue(blobInfo.NamespaceId, out client))
							{
								client = TryCreateClient(blobInfo.NamespaceId);
								namespaceIdToClient.Add(blobInfo.NamespaceId, client);
							}

							if (client != null)
							{
								BundleHeader? header = await client.ReadHeaderAsync(blobInfo.Locator, cancellationToken);
								if (header != null)
								{
									List<ObjectId> importInfoIds = new List<ObjectId>();
									foreach (BundleLocator import in header.Imports)
									{
										FilterDefinition<BlobInfo> filter = Builders<BlobInfo>.Filter.Expr(x => x.NamespaceId == blobInfo.NamespaceId && x.Path == import.Path.ToString());
										UpdateDefinition<BlobInfo> update = Builders<BlobInfo>.Update.SetOnInsert(x => x.Imports, null);
										BlobInfo blobInfoDoc = await _blobCollection.FindOneAndUpdateAsync(filter, update, new FindOneAndUpdateOptions<BlobInfo> { IsUpsert = true, ReturnDocument = ReturnDocument.After }, cancellationToken);
										importInfoIds.Add(blobInfoDoc.Id);
									}
									await _blobCollection.UpdateOneAsync(x => x.Id == blobInfo.Id, Builders<BlobInfo>.Update.Set(x => x.Imports, importInfoIds), null, cancellationToken);
								}
								AddGcCheckRecord(blobInfo.NamespaceId, blobInfo.Id);
							}
						}

						// Update the last imported blob id
						await _gcState.UpdateAsync(state => state.LastImportBlobInfoId = latestInfoId);
					}
				}
			}
			finally
			{
				foreach (IServerStorageClient? storageClient in namespaceIdToClient.Values)
				{
					storageClient?.Dispose();
				}
			}
		}

		#endregion

		#region Nodes

		/// <summary>
		/// Adds a node alias
		/// </summary>
		/// <param name="namespaceId">Namespace to search</param>
		/// <param name="alias">Alias for the node</param>
		/// <param name="target">Target node for the alias</param>
		/// <param name="rank">Rank for the alias. Higher ranked aliases are preferred by default.</param>
		/// <param name="data">Inline data to store with this alias</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Sequence of thandles</returns>
		async Task AddAliasAsync(NamespaceId namespaceId, string alias, BundleNodeLocator target, int rank, ReadOnlyMemory<byte> data, CancellationToken cancellationToken = default)
		{
			BlobInfo? blobInfo = await _blobCollection.Find(x => x.NamespaceId == namespaceId && x.Path == target.Blob.Path.ToString()).FirstOrDefaultAsync(cancellationToken);
			if (blobInfo == null)
			{
				throw new KeyNotFoundException($"Missing blob {target.Blob}");
			}

			if (blobInfo.Aliases != null && blobInfo.Aliases.Any(x => x.Alias == alias && x.Index == target.ExportIdx))
			{
				return;
			}

			FilterDefinition<BlobInfo> filter = Builders<BlobInfo>.Filter.Expr(x => x.NamespaceId == blobInfo.NamespaceId && x.Path == target.Blob.Path.ToString());
			UpdateDefinition<BlobInfo> update = Builders<BlobInfo>.Update.Push(x => x.Aliases, new AliasInfo(alias.ToString(), target.ExportIdx, data.ToArray(), rank));
			await _blobCollection.UpdateOneAsync(filter, update, cancellationToken: cancellationToken);
		}

		/// <summary>
		/// Removes a node alias
		/// </summary>
		/// <param name="namespaceId">Namespace to search</param>
		/// <param name="alias">Alias for the node</param>
		/// <param name="target">Target node for the alias</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Sequence of thandles</returns>
		async Task RemoveAliasAsync(NamespaceId namespaceId, string alias, BundleNodeLocator target, CancellationToken cancellationToken = default)
		{
			FilterDefinition<BlobInfo> filter = Builders<BlobInfo>.Filter.Expr(x => x.NamespaceId == namespaceId && x.Path == target.Blob.Path.ToString());
			UpdateDefinition<BlobInfo> update = Builders<BlobInfo>.Update.PullFilter(x => x.Aliases, Builders<AliasInfo>.Filter.Expr(x => x.Alias == alias));
			await _blobCollection.UpdateOneAsync(filter, update, cancellationToken: cancellationToken);
		}

		/// <summary>
		/// Finds nodes with the given type and hash
		/// </summary>
		/// <param name="namespaceId">Namespace to search</param>
		/// <param name="alias">Alias for the node</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Sequence of thandles</returns>
		async Task<List<(BundleNodeLocator, AliasInfo)>> FindAliasesAsync(NamespaceId namespaceId, string alias, CancellationToken cancellationToken = default)
		{
			List<(BundleNodeLocator, AliasInfo)> results = new List<(BundleNodeLocator, AliasInfo)>();
			await foreach (BlobInfo blobInfo in _blobCollection.Find(x => x.NamespaceId == namespaceId && x.Aliases!.Any(y => y.Alias == alias)).ToAsyncEnumerable(cancellationToken))
			{
				if (blobInfo.Aliases != null)
				{
					foreach (AliasInfo aliasInfo in blobInfo.Aliases)
					{
						if (String.Equals(aliasInfo.Alias, alias, StringComparison.Ordinal))
						{
							BundleNodeLocator locator = new BundleNodeLocator(blobInfo.Locator, aliasInfo.Index);
							results.Add((locator, aliasInfo));
						}
					}
				}
			}
			return results.OrderByDescending(x => x.Item2.Rank).ToList();
		}

		#endregion

		#region Refs

		record RefCacheKey(NamespaceId NamespaceId, RefName Name);
		record RefCacheValue(RefInfo? Value, DateTime Time);

		/// <summary>
		/// Adds a ref value to the cache
		/// </summary>
		/// <param name="namespaceId">Namespace containing the ref</param>
		/// <param name="name">Name of the ref</param>
		/// <param name="value">New target for the ref</param>
		/// <returns>The cached value</returns>
		RefCacheValue AddRefToCache(NamespaceId namespaceId, RefName name, RefInfo? value)
		{
			RefCacheValue cacheValue = new RefCacheValue(value, DateTime.UtcNow);
			using (ICacheEntry newEntry = _memoryCache.CreateEntry(new RefCacheKey(namespaceId, name)))
			{
				newEntry.Value = cacheValue;
				newEntry.SetSize(name.Text.Length);
				newEntry.SetSlidingExpiration(TimeSpan.FromMinutes(5.0));
			}
			return cacheValue;
		}

		/// <summary>
		/// Expires any refs that are no longer valid
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		async ValueTask TickRefsAsync(CancellationToken cancellationToken)
		{
			DateTime utcNow = DateTime.UtcNow;
			using (IAsyncCursor<RefInfo> cursor = await _refCollection.Find(x => x.ExpiresAtUtc < utcNow).ToCursorAsync(cancellationToken))
			{
				List<DeleteOneModel<RefInfo>> requests = new List<DeleteOneModel<RefInfo>>();
				while (await cursor.MoveNextAsync(cancellationToken))
				{
					requests.Clear();

					foreach (RefInfo refInfo in cursor.Current)
					{
						_logger.LogInformation("Expired ref {NamespaceId}:{RefName}", refInfo.NamespaceId, refInfo.Name);
						FilterDefinition<RefInfo> filter = Builders<RefInfo>.Filter.Expr(x => x.Id == refInfo.Id && x.ExpiresAtUtc == refInfo.ExpiresAtUtc);
						requests.Add(new DeleteOneModel<RefInfo>(filter));
						AddGcCheckRecord(refInfo.NamespaceId, refInfo.Imports);
						AddRefToCache(refInfo.NamespaceId, refInfo.Name, default);
					}

					if (requests.Count > 0)
					{
						await _refCollection.BulkWriteAsync(requests, cancellationToken: cancellationToken);
					}
				}
			}
		}

		/// <inheritdoc/>
		async Task<bool> DeleteRefAsync(NamespaceId namespaceId, RefName name, CancellationToken cancellationToken = default)
		{
			FilterDefinition<RefInfo> filter = Builders<RefInfo>.Filter.Expr(x => x.NamespaceId == namespaceId && x.Name == name);
			return await DeleteRefInternalAsync(namespaceId, name, filter, cancellationToken);
		}

		/// <summary>
		/// Deletes a ref document that has reached its expiry time
		/// </summary>
		async Task DeleteExpiredRefAsync(RefInfo refDocument, CancellationToken cancellationToken = default)
		{
			FilterDefinition<RefInfo> filter = Builders<RefInfo>.Filter.Expr(x => x.Id == refDocument.Id && x.ExpiresAtUtc == refDocument.ExpiresAtUtc);
			await DeleteRefInternalAsync(refDocument.NamespaceId, refDocument.Name, filter, cancellationToken);
		}

		async Task<bool> DeleteRefInternalAsync(NamespaceId namespaceId, RefName name, FilterDefinition<RefInfo> filter, CancellationToken cancellationToken = default)
		{
			RefInfo? oldRefInfo = await _refCollection.FindOneAndDeleteAsync<RefInfo>(filter, cancellationToken: cancellationToken);
			AddRefToCache(namespaceId, name, default);

			if (oldRefInfo != null)
			{
				_logger.LogInformation("Deleted ref {NamespaceId}:{RefName}", namespaceId, name);
				AddGcCheckRecord(namespaceId, oldRefInfo.Imports);
				return true;
			}

			return false;
		}

		/// <inheritdoc/>
		async Task<RefInfo?> TryReadRefAsync(NamespaceId namespaceId, RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			RefCacheValue entry;
			if (!_memoryCache.TryGetValue(name, out entry) || RefCacheTime.IsStaleCacheEntry(entry.Time, cacheTime))
			{
				RefInfo? refDocument = await _refCollection.Find(x => x.NamespaceId == namespaceId && x.Name == name).FirstOrDefaultAsync(cancellationToken);
				entry = AddRefToCache(namespaceId, name, refDocument);
			}

			if (entry.Value == null)
			{
				return null;
			}

			if (entry.Value.ExpiresAtUtc != null)
			{
				DateTime utcNow = _clock.UtcNow;
				if (entry.Value.HasExpired(utcNow))
				{
					await DeleteExpiredRefAsync(entry.Value, cancellationToken);
					return default;
				}
				if (entry.Value.RequiresTouch(utcNow))
				{
					await _refCollection.UpdateOneAsync(x => x.Id == entry.Value.Id, Builders<RefInfo>.Update.Set(x => x.ExpiresAtUtc, utcNow + entry.Value.Lifetime!.Value), cancellationToken: cancellationToken);
				}
			}

			return entry.Value;
		}

		/// <inheritdoc/>
		async Task WriteRefAsync(NamespaceId namespaceId, RefName name, BlobData data, RefOptions? options = null, CancellationToken cancellationToken = default)
		{
			List<ObjectId> imports = new List<ObjectId>();
			foreach (BlobHandle import in data.Refs)
			{
				string path = import.GetLocator().Outermost.ToString();

				BlobInfo? newBlobInfo = await _blobCollection.Find(x => x.NamespaceId == namespaceId && x.Path == path).FirstOrDefaultAsync(cancellationToken);
				if (newBlobInfo == null)
				{
					throw new Exception($"Invalid/unknown blob identifier '{path}' in namespace {namespaceId}");
				}

				imports.Add(newBlobInfo.Id);
			}

			byte[] packetData = EncodedBlobData.Create(data);
			RefInfo newRefInfo = new RefInfo(namespaceId, name, packetData, imports);

			if (options != null && options.Lifetime.HasValue)
			{
				newRefInfo.ExpiresAtUtc = _clock.UtcNow + options.Lifetime.Value;
				if (options.Extend ?? true)
				{
					newRefInfo.Lifetime = options.Lifetime;
				}
			}

			RefInfo? oldRefInfo = await _refCollection.FindOneAndReplaceAsync<RefInfo>(x => x.NamespaceId == namespaceId && x.Name == name, newRefInfo, new FindOneAndReplaceOptions<RefInfo> { IsUpsert = true }, cancellationToken);
			if (oldRefInfo != null)
			{
				AddGcCheckRecord(namespaceId, oldRefInfo.Imports);
			}

			_logger.LogInformation("Updated ref {NamespaceId}:{RefName}", namespaceId, name);
			AddRefToCache(namespaceId, name, newRefInfo);
		}

		#endregion

		#region GC

		uint GetGcTimestamp() => GetGcTimestamp(_clock.UtcNow);
		static uint GetGcTimestamp(DateTime utcTime) => (uint)((utcTime - DateTime.UnixEpoch).Ticks / TimeSpan.TicksPerMinute);

		static RedisKey GetRedisKey(string suffix) => $"storage:{suffix}";
		static RedisKey GetRedisKey(NamespaceId namespaceId, string suffix) => GetRedisKey($"{namespaceId}:{suffix}");

		static RedisSortedSetKey<RedisValue> GetGcCheckSet(NamespaceId namespaceId) => new RedisSortedSetKey<RedisValue>(GetRedisKey(namespaceId, "check"));

		/// <summary>
		/// Find the next namespace to run GC on
		/// </summary>
		/// <param name="cancellationToken"></param>
		/// <returns></returns>
		async ValueTask TickGcAsync(CancellationToken cancellationToken)
		{
			HashSet<NamespaceId> ranNamespaceIds = new HashSet<NamespaceId>();

			DateTime utcNow = _clock.UtcNow;
			for (; ; )
			{
				// Synchronize the list of configured namespaces with the GC state object
				List<NamespaceConfig> namespaces;
				lock (_lockObject)
				{
					namespaces = GetNextState().Namespaces.Select(x => x.Value.Config).ToList();
				}

				GcState state = await _gcState.GetAsync();
				if (!Enumerable.SequenceEqual(namespaces.Select(x => x.Id.Text.Text).OrderBy(x => x), state.Namespaces.Select(x => x.Id.Text.Text).OrderBy(x => x)))
				{
					state = await _gcState.UpdateAsync(s => SyncNamespaceList(s, namespaces));
				}

				// Find all the namespaces that need to have GC run on them
				List<(DateTime, GcNamespaceState)> pending = new List<(DateTime, GcNamespaceState)>();
				foreach (GcNamespaceState namespaceState in state.Namespaces)
				{
					if (!ranNamespaceIds.Contains(namespaceState.Id))
					{
						NamespaceConfig? config = namespaces.FirstOrDefault(x => x.Id == namespaceState.Id);
						if (config != null)
						{
							DateTime time = namespaceState.LastTime + TimeSpan.FromHours(config.GcFrequencyHrs);
							if (time < utcNow)
							{
								pending.Add((time, namespaceState));
							}
						}
					}
				}
				pending.SortBy(x => x.Item1);

				// If there's nothing left to GC, bail out
				if (pending.Count == 0)
				{
					break;
				}

				// Update the first one we can acquire a lock for
				foreach ((_, GcNamespaceState namespaceState) in pending)
				{
					NamespaceId namespaceId = namespaceState.Id;
					if (ranNamespaceIds.Add(namespaceId))
					{
						RedisKey key = GetRedisKey(namespaceId, "lock");
						using (RedisLock namespaceLock = new RedisLock(_redisService.GetDatabase(), key))
						{
							if (await namespaceLock.AcquireAsync(TimeSpan.FromMinutes(20.0)))
							{
								try
								{
									await TickGcForNamespaceAsync(namespaceId, state.LastImportBlobInfoId, utcNow, cancellationToken);
								}
								catch (Exception ex)
								{
									_logger.LogError(ex, "Exception while running garbage collection: {Message}", ex.Message);
								}
								break;
							}
						}
					}
				}
			}
		}

		async Task TickGcForNamespaceAsync(NamespaceId namespaceId, ObjectId lastImportBlobInfoId, DateTime utcNow, CancellationToken cancellationToken)
		{
			using IServerStorageClient? client = TryCreateClient(namespaceId);
			if (client == null)
			{
				return;
			}

			double score = GetGcTimestamp(utcNow);

			RedisSortedSetKey<RedisValue> checkSet = GetGcCheckSet(namespaceId);
			for (; ; )
			{
				RedisValue[] values = await _redisService.GetDatabase().SortedSetRangeByRankAsync(checkSet, 0, 0);
				if (values.Length == 0)
				{
					break;
				}

				ObjectId blobInfoId = new ObjectId(((byte[]?)values[0])!);
				if (blobInfoId < lastImportBlobInfoId && !await IsBlobReferencedAsync(blobInfoId, cancellationToken))
				{
					BlobInfo? info = await _blobCollection.FindOneAndDeleteAsync(x => x.Id == blobInfoId, cancellationToken: cancellationToken);
					if (info != null)
					{
						if (info.Imports != null)
						{
							SortedSetEntry<RedisValue>[] entries = info.Imports.Select(x => new SortedSetEntry<RedisValue>(x.ToByteArray(), score)).ToArray();
							_ = _redisService.GetDatabase().SortedSetAddAsync(checkSet, entries, flags: CommandFlags.FireAndForget);
							score = Math.BitIncrement(score);
						}
						await client.Backend.DeleteAsync(info.Locator.ToString(), cancellationToken);
					}
				}
				_ = _redisService.GetDatabase().SortedSetRemoveAsync(checkSet, values[0], CommandFlags.FireAndForget);
			}

			await _gcState.UpdateAsync(state => state.FindOrAddNamespace(namespaceId).LastTime = utcNow);
		}

		static void SyncNamespaceList(GcState state, List<NamespaceConfig> namespaces)
		{
			HashSet<NamespaceId> validNamespaceIds = new HashSet<NamespaceId>(namespaces.Select(x => x.Id));
			state.Namespaces.RemoveAll(x => !validNamespaceIds.Contains(x.Id));

			HashSet<NamespaceId> currentNamespaceIds = new HashSet<NamespaceId>(state.Namespaces.Select(x => x.Id));
			foreach (NamespaceConfig config in namespaces)
			{
				if (!currentNamespaceIds.Contains(config.Id))
				{
					state.Namespaces.Add(new GcNamespaceState { Id = config.Id });
				}
			}

			state.Namespaces.SortBy(x => x.Id);
		}

		void AddGcCheckRecord(NamespaceId namespaceId, ObjectId id)
		{
			double score = GetGcTimestamp();
			_ = _redisService.GetDatabase().SortedSetAddAsync(GetGcCheckSet(namespaceId), id.ToByteArray(), score, flags: CommandFlags.FireAndForget);
		}

		void AddGcCheckRecord(NamespaceId namespaceId, IEnumerable<ObjectId> imports)
		{
			foreach (ObjectId import in imports)
			{
				AddGcCheckRecord(namespaceId, import);
			}
		}

		#endregion
	}
}
