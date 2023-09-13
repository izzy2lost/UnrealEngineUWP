// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Clients;
using Microsoft.Extensions.Http;
using Microsoft.Extensions.Logging;
using Polly;
using Polly.Extensions.Http;
using Polly.Retry;

#pragma warning disable CA2234 // Pass system uri objects instead of strings

namespace EpicGames.Horde
{
	/// <summary>
	/// Wraps an Http client which communicates with the Horde server
	/// </summary>
	public sealed class HordeHttpClient : IDisposable
	{
		readonly HttpMessageHandler _httpMessageHandler;
		readonly Uri _baseUri;
		readonly AuthenticationHeaderValue? _authHeader;
		readonly JsonSerializerOptions _jsonSerializerOptions;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="uri">Base URI of the server</param>
		/// <param name="token">Access token for the connection</param>
		/// <param name="logger">Logger for output</param>
		public HordeHttpClient(Uri uri, string token, ILogger logger)
		{
			_baseUri = uri;
			_authHeader = new AuthenticationHeaderValue("Bearer", token);

			_httpMessageHandler = CreateHttpMessageHandler();

			_jsonSerializerOptions = new JsonSerializerOptions();
			ConfigureJsonSerializer(_jsonSerializerOptions);

			_logger = logger;
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			_httpMessageHandler.Dispose();
		}

		/// <summary>
		/// Creates an HTTP client using the underlying message handler
		/// </summary>
		public HttpClient CreateHttpClient()
		{
			HttpClient httpClient = new HttpClient(_httpMessageHandler, false);
			httpClient.BaseAddress = _baseUri;
			httpClient.DefaultRequestHeaders.Authorization = _authHeader;
			return httpClient;
		}

		/// <summary>
		/// Creates an HTTP client using the underlying message handler
		/// </summary>
		public HttpClient CreateRedirectHttpClient()
		{
			return new HttpClient(_httpMessageHandler, false);
		}

		/// <summary>
		/// Creates a HTTP message handler
		/// </summary>
		/// <returns></returns>
		public static HttpMessageHandler CreateHttpMessageHandler()
		{
			AsyncRetryPolicy<HttpResponseMessage> retryPolicy = HttpPolicyExtensions
				.HandleTransientHttpError()
				.WaitAndRetryAsync(new[] { TimeSpan.FromSeconds(2.0), TimeSpan.FromSeconds(5.0), TimeSpan.FromSeconds(10), TimeSpan.FromSeconds(30) });

			SocketsHttpHandler socketsHandler = new SocketsHttpHandler { PooledConnectionLifetime = TimeSpan.FromMinutes(15) };
			return new PolicyHttpMessageHandler(retryPolicy) { InnerHandler = socketsHandler };
		}

		/// <summary>
		/// Configures a JSON serializer to read Horde responses
		/// </summary>
		/// <param name="options">options for the serializer</param>
		public static void ConfigureJsonSerializer(JsonSerializerOptions options)
		{
			options.AllowTrailingCommas = true;
			options.ReadCommentHandling = JsonCommentHandling.Skip;
			options.DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull;
			options.PropertyNamingPolicy = JsonNamingPolicy.CamelCase;
			options.PropertyNameCaseInsensitive = true;
			options.Converters.Add(new JsonStringEnumConverter());
			options.Converters.Add(new StringIdJsonConverterFactory());
			options.Converters.Add(new BinaryIdJsonConverterFactory());
		}

		/// <summary>
		/// Creates a storage client for this Horde instance
		/// </summary>
		/// <param name="namespaceId">Namespace to manipulate</param>
		/// <param name="storageCache">Cache for storage operations</param>
		/// <returns>New storage client instance</returns>
		public HttpStorageClient CreateStorageClient(NamespaceId namespaceId, StorageCache storageCache) => CreateStorageClient($"api/v1/storage/{namespaceId}/", storageCache);

		/// <summary>
		/// Creates a storage client for this Horde instance
		/// </summary>
		/// <param name="path">Base path for storage requests</param>
		/// <param name="storageCache">Cache for storage operations</param>
		/// <returns>New storage client instance</returns>
		public HttpStorageClient CreateStorageClient(string path, StorageCache storageCache)
		{
			if (!path.EndsWith("/", StringComparison.Ordinal))
			{
				path += "/";
			}

			Uri baseUri = new Uri(_baseUri, path);

			HttpClient CreateStorageHttpClient()
			{
				HttpClient httpClient = new HttpClient(_httpMessageHandler, false);
				httpClient.BaseAddress = baseUri;
				httpClient.DefaultRequestHeaders.Authorization = _authHeader;
				return httpClient;
			}

			return new HttpStorageClient(CreateStorageHttpClient, CreateRedirectHttpClient, storageCache, _logger);
		}

		/// <summary>
		/// Gets a resource from an HTTP endpoint and parses it as a JSON object
		/// </summary>
		/// <typeparam name="TResponse">The object type to return</typeparam>
		/// <param name="relativePath">The url to retrieve</param>
		/// <param name="cancellationToken">Cancels the request</param>
		/// <returns>New instance of the object</returns>
		public async Task<TResponse> GetAsync<TResponse>(string relativePath, CancellationToken cancellationToken = default)
		{
			using HttpClient httpClient = CreateHttpClient();
			return await httpClient.GetAsync<TResponse>(relativePath, cancellationToken);
		}

		/// <summary>
		/// Posts an object to an HTTP endpoint as a JSON object, and parses the response object
		/// </summary>
		/// <typeparam name="TRequest">The object type to post</typeparam>
		/// <param name="relativePath">The url to retrieve</param>
		/// <param name="request">The object to post</param>
		/// <param name="cancellationToken">Cancels the request</param>
		/// <returns>The response parsed into the requested type</returns>
		public async Task<HttpResponseMessage> PostAsync<TRequest>(string relativePath, TRequest request, CancellationToken cancellationToken = default)
		{
			using HttpClient httpClient = CreateHttpClient();
			return await httpClient.PostAsync<TRequest>(relativePath, request, cancellationToken);
		}

		/// <summary>
		/// Posts an object to an HTTP endpoint as a JSON object, and parses the response object
		/// </summary>
		/// <typeparam name="TResponse">The object type to return</typeparam>
		/// <typeparam name="TRequest">The object type to post</typeparam>
		/// <param name="relativePath">The url to retrieve</param>
		/// <param name="request">The object to post</param>
		/// <param name="cancellationToken">Cancels the request</param>
		/// <returns>The response parsed into the requested type</returns>
		public async Task<TResponse> PostAsync<TResponse, TRequest>(string relativePath, TRequest request, CancellationToken cancellationToken = default)
		{
			using HttpClient httpClient = CreateHttpClient();
			return await httpClient.PostAsync<TResponse, TRequest>(relativePath, request, cancellationToken);
		}

		/// <summary>
		/// Puts an object to an HTTP endpoint as a JSON object
		/// </summary>
		/// <typeparam name="TRequest">The object type to post</typeparam>
		/// <param name="relativePath">The url to write to</param>
		/// <param name="request">The object to post</param>
		/// <param name="cancellationToken">Cancels the request</param>
		/// <returns>Response message</returns>
		public async Task<HttpResponseMessage> PutAsync<TRequest>(string relativePath, TRequest request, CancellationToken cancellationToken)
		{
			using HttpClient httpClient = CreateHttpClient();
			return await httpClient.PutAsync<TRequest>(relativePath, request, cancellationToken);
		}

		/// <summary>
		/// Puts an object to an HTTP endpoint as a JSON object
		/// </summary>
		/// <typeparam name="TResponse">The object type to return</typeparam>
		/// <typeparam name="TRequest">The object type to post</typeparam>
		/// <param name="relativePath">The url to write to</param>
		/// <param name="request">The object to post</param>
		/// <param name="cancellationToken">Cancels the request</param>
		/// <returns>Response message</returns>
		public async Task<TResponse> PutAsync<TResponse, TRequest>(string relativePath, TRequest request, CancellationToken cancellationToken)
		{
			using HttpClient httpClient = CreateHttpClient();
			return await httpClient.PutAsync<TResponse, TRequest>(relativePath, request, cancellationToken);
		}
	}
}
