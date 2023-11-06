// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Net.Http.Json;
using System.Threading;
using System.Threading.Tasks;
using System.Web;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using EpicGames.Horde.Storage.Backends;
using System.IO;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;

namespace EpicGames.Horde.Storage.Clients
{
	/// <summary>
	/// Implementation of <see cref="IStorageClient"/> which communicates with an upstream Horde instance via HTTP.
	/// </summary>
	public sealed class HttpStorageClient : IStorageClient
	{
		class Handle : BlobHandle
		{
			readonly HttpStorageClient _outer;
			readonly BlobLocator _locator;

			public Handle(HttpStorageClient outer, BlobLocator locator)
			{
				_outer = outer;
				_locator = locator;
			}

			/// <inheritdoc/>
			public override Task<Stream> OpenAsync(int offset = 0, int? length = null, CancellationToken cancellationToken = default)
				=> _outer._backend.OpenAsync(_locator.ToString(), offset, length, cancellationToken);

			/// <inheritdoc/>
			public override ValueTask<BlobData> ReadBlobDataAsync(CancellationToken cancellationToken = default)
				=> _outer.ReadBlobAsync(_locator, cancellationToken);

			/// <inheritdoc/>
			public override bool TryGetLocator([NotNullWhen(true)] out BlobLocator locator)
			{
				locator = _locator;
				return true;
			}

			/// <inheritdoc/>
			public override bool Equals(object? obj) => obj is Handle other && _locator == other._locator;

			/// <inheritdoc/>
			public override int GetHashCode() => _locator.GetHashCode();
		}

		readonly string _basePath;
		readonly Func<HttpClient> _createClient;
		readonly IStorageBackend _backend;
		readonly ILogger _logger;

		/// <inheritdoc/>
		public bool SupportsRedirects => _backend.SupportsRedirects;

		/// <summary>
		/// Constructor
		/// </summary>
		public HttpStorageClient(string basePath, Func<HttpClient> createClient, IStorageBackend backend, ILogger logger) 
		{
			_basePath = basePath.TrimEnd('/');
			_createClient = createClient;
			_backend = backend;
			_logger = logger;
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			_backend.Dispose();
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
		public IStorageWriter CreateWriter(string? basePath = null)
			=> new DefaultStorageWriter(this, basePath);

		/// <inheritdoc/>
		public async ValueTask<BlobData> ReadBlobAsync(BlobLocator locator, CancellationToken cancellationToken = default)
		{
			IReadOnlyMemoryOwner<byte> owner = await _backend.ReadAsync(locator.ToString(), cancellationToken);
			return new ReadOnlyMemoryOwnerBlobData(BlobType.Leaf, owner, Array.Empty<BlobHandle>());
		}

		/// <inheritdoc/>
		public async ValueTask<BlobHandle> WriteBlobAsync(BlobType type, Stream stream, IReadOnlyList<BlobHandle> references, string? basePath = null, CancellationToken cancellationToken = default)
		{
			string path = await _backend.WriteAsync(stream, basePath, cancellationToken);
			return new Handle(this, new BlobLocator(path));
		}

		/// <inheritdoc/>
		public ValueTask<Uri?> TryGetReadRedirectAsync(BlobLocator locator, CancellationToken cancellationToken = default)
			=> _backend.TryGetReadRedirectAsync(locator.ToString(), cancellationToken);

		/// <inheritdoc/>
		public async ValueTask<(BlobLocator, Uri)?> TryGetWriteRedirectAsync(string? prefix = null, CancellationToken cancellationToken = default)
		{
			(string Path, Uri Url)? result = await _backend.TryGetWriteRedirectAsync(prefix, cancellationToken);
			if (result == null)
			{
				return null;
			}
			return (new BlobLocator(result.Value.Path), result.Value.Url);
		}

		#endregion

		#region Aliases

		/// <inheritdoc/>
		public Task AddAliasAsync(string name, BlobHandle target, int rank = 0, ReadOnlyMemory<byte> data = default, CancellationToken cancellationToken = default)
		{
			throw new NotSupportedException("Http storage client does not currently support aliases.");
		}

		/// <inheritdoc/>
		public Task RemoveAliasAsync(string name, BlobHandle target, CancellationToken cancellationToken = default)
		{
			throw new NotSupportedException("Http storage client does not currently support aliases.");
		}

		/// <inheritdoc/>
		public async Task<BlobAlias[]> FindAliasesAsync(string alias, int? maxResults = null, CancellationToken cancellationToken = default)
		{
			_logger.LogDebug("Finding nodes with alias {Alias}", alias);
			using (HttpClient httpClient = _createClient())
			{
				string queryPath = $"{_basePath}/nodes?alias={HttpUtility.UrlEncode(alias.ToString())}";
				if (maxResults != null)
				{
					queryPath += $"&maxResults={maxResults.Value}";
				}

				using (HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Get, queryPath))
				{
					using (HttpResponseMessage response = await httpClient.SendAsync(request, cancellationToken))
					{
						response.EnsureSuccessStatusCode();

						FindNodesResponse? message = await response.Content.ReadFromJsonAsync<FindNodesResponse>(cancellationToken: cancellationToken);

						BlobAlias[] aliases = new BlobAlias[message!.Nodes.Count];
						for (int idx = 0; idx < message.Nodes.Count; idx++)
						{
							FindNodeResponse node = message.Nodes[idx];
							BlobHandle handle = CreateBlobHandle(node.Blob);
							aliases[idx] = new BlobAlias(handle, node.Rank, node.Data);
						}

						return aliases;
					}
				}
			}
		}

		#endregion

		#region Refs

		/// <inheritdoc/>
		public async Task<bool> DeleteRefAsync(RefName name, CancellationToken cancellationToken)
		{
			_logger.LogDebug("Deleting ref {RefName}", name);
			using (HttpClient httpClient = _createClient())
			{
				using (HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Delete, $"{_basePath}/refs/{name}"))
				{
					using (HttpResponseMessage response = await httpClient.SendAsync(request, cancellationToken))
					{
						if (response.IsSuccessStatusCode)
						{
							return true;
						}
						if (response.StatusCode == HttpStatusCode.NotFound)
						{
							return false;
						}

						response.EnsureSuccessStatusCode();
						return false;
					}
				}
			}
		}

		/// <inheritdoc/>
		public async Task<BlobHandle?> TryReadRefAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			using (HttpClient httpClient = _createClient())
			{
				using (HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Get, $"{_basePath}/refs/{name}"))
				{
					if (cacheTime.IsSet())
					{
						request.Headers.CacheControl = new CacheControlHeaderValue { MaxAge = cacheTime.MaxAge };
					}

					using (HttpResponseMessage response = await httpClient.SendAsync(request, cancellationToken))
					{
						if (response.StatusCode == HttpStatusCode.NotFound)
						{
							_logger.LogDebug("Read ref {RefName} -> None", name);
							return null;
						}
						else if (!response.IsSuccessStatusCode)
						{
							_logger.LogError("Unable to read ref {RefName} (status: {StatusCode}, body: {Body})", name, response.StatusCode, await response.Content.ReadAsStringAsync(cancellationToken));
							throw new StorageException($"Unable to read ref '{name}'");
						}
						else
						{
							response.EnsureSuccessStatusCode();
							ReadRefResponse? data = await response.Content.ReadFromJsonAsync<ReadRefResponse>(cancellationToken: cancellationToken);
							_logger.LogDebug("Read ref {RefName} -> {Blob}", name, data!.Target);

							return CreateBlobHandle(data.Target);
						}
					}
				}
			}
		}

		/// <inheritdoc/>
		public async Task WriteRefAsync(RefName name, BlobHandle target, RefOptions? options = null, CancellationToken cancellationToken = default)
		{
			await target.FlushAsync(cancellationToken);
			BlobLocator locator = target.GetLocator();

			_logger.LogDebug("Writing ref {RefName} -> {RefTarget}", name, locator);
			using (HttpClient httpClient = _createClient())
			{
				WriteRefRequest request = new WriteRefRequest();
				request.Target = locator;
				request.Options = options;

				using (HttpResponseMessage response = await httpClient.PutAsync($"{_basePath}/refs/{name}", request, cancellationToken))
				{
					response.EnsureSuccessStatusCode();
				}
			}
		}

		#endregion

		/// <inheritdoc/>
		public void GetStats(StorageStats stats)
		{
		}
	}

	/// <summary>
	/// Factory for constructing HttpStorageClient instances
	/// </summary>
	public sealed class HttpStorageClientFactory : IStorageClientFactory
	{
		readonly IHttpClientFactory _httpClientFactory;
		readonly StorageBackendCache _backendCache;
		readonly BundleCache _bundleCache;
		readonly ILogger<HttpStorageBackend> _backendLogger;
		readonly ILogger<HttpStorageClient> _clientLogger;

		/// <summary>
		/// Constructor
		/// </summary>
		public HttpStorageClientFactory(IHttpClientFactory httpClientFactory, StorageBackendCache backendCache, BundleCache bundleCache, ILogger<HttpStorageBackend> backendLogger, ILogger<HttpStorageClient> clientLogger)
		{
			_httpClientFactory = httpClientFactory;
			_backendCache = backendCache;
			_bundleCache = bundleCache;
			_backendLogger = backendLogger;
			_clientLogger = clientLogger;
		}

		/// <summary>
		/// Creates a new HTTP storage client
		/// </summary>
		/// <param name="basePath">Base path for all requests</param>
		/// <param name="accessToken">Custom access token to use for requests</param>
		public IStorageClient CreateClientWithPath(string basePath, string? accessToken = null)
		{
			HttpClient CreateClient()
			{
				HttpClient httpClient = _httpClientFactory.CreateClient(HordeHttpClient.HttpClientName);
				if (accessToken != null)
				{
					httpClient.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Bearer", accessToken);
				}
				return httpClient;
			}

			IStorageBackend backend = new HttpStorageBackend(basePath, CreateClient, _backendLogger);
			if (_backendCache != null)
			{
				backend = _backendCache.CreateWrapper(basePath, backend);
			}

			HttpStorageClient client = new HttpStorageClient(basePath, CreateClient, backend, _clientLogger);
			return new BundleStorageClient(client, _bundleCache, _clientLogger);
		}

		/// <summary>
		/// Creates a new HTTP storage client
		/// </summary>
		/// <param name="namespaceId">Namespace to create a client for</param>
		/// <param name="accessToken">Custom access token to use for requests</param>
		public IStorageClient CreateClient(NamespaceId namespaceId, string? accessToken = null) => CreateClientWithPath($"api/v1/storage/{namespaceId}", accessToken);

		/// <inheritdoc/>
		IStorageClient? IStorageClientFactory.TryCreateClient(NamespaceId namespaceId) => CreateClient(namespaceId);
	}
}
