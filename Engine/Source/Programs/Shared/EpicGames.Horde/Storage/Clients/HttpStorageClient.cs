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
using EpicGames.Horde.Storage.Bundles;
using EpicGames.Horde.Storage.Backends;

namespace EpicGames.Horde.Storage.Clients
{
	/// <summary>
	/// Implementation of <see cref="IStorageClient"/> which communicates with an upstream Horde instance via HTTP.
	/// </summary>
	public class HttpStorageClient : BundleStorageClient
	{
		readonly string _basePath;
		readonly Func<HttpClient> _createClient;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public HttpStorageClient(string basePath, Func<HttpClient> createClient, IStorageBackend backend, BundleReaderCache cache, ILogger logger) 
			: base(backend, cache, logger)
		{
			_basePath = basePath.TrimEnd('/');
			_createClient = createClient;
			_logger = logger;
		}

		#region Nodes

		/// <inheritdoc/>
		public override Task AddAliasAsync(string name, BundleNodeLocator locator, int rank = 0, ReadOnlyMemory<byte> data = default, CancellationToken cancellationToken = default)
		{
			throw new NotSupportedException("Http storage client does not currently support aliases.");
		}

		/// <inheritdoc/>
		public override Task RemoveAliasAsync(string name, BundleNodeLocator locator, CancellationToken cancellationToken = default)
		{
			throw new NotSupportedException("Http storage client does not currently support aliases.");
		}

		/// <inheritdoc/>
		public override async Task<BlobAlias[]> FindAliasesAsync(string alias, int? maxResults = null, CancellationToken cancellationToken = default)
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
		public override async Task<bool> DeleteRefAsync(RefName name, CancellationToken cancellationToken)
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
		public override async Task<RefValue?> TryReadRefAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default)
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

							BundleNodeHandle handle = CreateNodeHandle(BundleNodeLocator.FromBlobLocator(data.Target));
							return new RefValue(handle, data.Data);
						}
					}
				}
			}
		}

		/// <inheritdoc/>
		public override async Task WriteRefAsync(RefName name, BundleNodeLocator locator, ReadOnlyMemory<byte> data, RefOptions? options = null, CancellationToken cancellationToken = default)
		{
			_logger.LogDebug("Writing ref {RefName} -> {RefTarget}", name, locator);
			using (HttpClient httpClient = _createClient())
			{
				WriteRefRequest request = new WriteRefRequest();
				request.Target = locator.ToBlobLocator();
				request.Data = data.ToArray();
				request.Options = options;

				using (HttpResponseMessage response = await httpClient.PutAsync($"{_basePath}/refs/{name}", request, cancellationToken))
				{
					response.EnsureSuccessStatusCode();
				}
			}
		}

		#endregion
	}

	/// <summary>
	/// Factory for constructing HttpStorageClient instances
	/// </summary>
	public class HttpStorageClientFactory : IStorageClientFactory
	{
		readonly IHttpClientFactory _httpClientFactory;
		readonly StorageBackendCache _backendCache;
		readonly BundleReaderCache _readerCache;
		readonly ILogger<HttpStorageBackend> _backendLogger;
		readonly ILogger<HttpStorageClient> _clientLogger;

		/// <summary>
		/// Constructor
		/// </summary>
		public HttpStorageClientFactory(IHttpClientFactory httpClientFactory, StorageBackendCache backendCache, BundleReaderCache readerCache, ILogger<HttpStorageBackend> backendLogger, ILogger<HttpStorageClient> clientLogger)
		{
			_httpClientFactory = httpClientFactory;
			_backendCache = backendCache;
			_readerCache = readerCache;
			_backendLogger = backendLogger;
			_clientLogger = clientLogger;
		}

		/// <summary>
		/// Creates a new HTTP storage client
		/// </summary>
		/// <param name="basePath">Base path for all requests</param>
		/// <param name="accessToken">Custom access token to use for requests</param>
		public HttpStorageClient CreateClient(string basePath, string? accessToken = null)
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
			return new HttpStorageClient(basePath, CreateClient, backend, _readerCache, _clientLogger);
		}

		/// <summary>
		/// Creates a new HTTP storage client
		/// </summary>
		/// <param name="namespaceId">Namespace to create a client for</param>
		/// <param name="accessToken">Custom access token to use for requests</param>
		public HttpStorageClient CreateClient(NamespaceId namespaceId, string? accessToken = null) => CreateClient($"api/v1/storage/{namespaceId}", accessToken);

		/// <inheritdoc/>
		IStorageClient IStorageClientFactory.CreateClient(NamespaceId namespaceId) => CreateClient(namespaceId);
	}
}
