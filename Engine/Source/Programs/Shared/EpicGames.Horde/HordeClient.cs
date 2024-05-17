// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Net.Http.Json;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Compute;
using EpicGames.Horde.Compute.Clients;
using EpicGames.Horde.Server;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Backends;
using EpicGames.Horde.Storage.Bundles;
using Grpc.Core;
using Grpc.Net.Client;
using Grpc.Net.Client.Configuration;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

#pragma warning disable CA2234 // Use URIs instead of strings

namespace EpicGames.Horde
{
	/// <summary>
	/// Default implementation of <see cref="IHordeClient"/>
	/// </summary>
	abstract class HordeClient : IHordeClient
	{
		readonly Uri _serverUrl;
		readonly IHttpClientFactory _httpClientFactory;
		readonly BundleCache _bundleCache;
		readonly HordeOptions _hordeOptions;
		readonly ILoggerFactory _loggerFactory;
		readonly ILogger _logger;

		BackgroundTask<GrpcChannel>? _grpcChannel;

		/// <inheritdoc/>
		public Uri ServerUrl => _serverUrl;

		/// <summary>
		/// Constructor
		/// </summary>
		public HordeClient(Uri serverUrl, IHttpClientFactory httpClientFactory, BundleCache bundleCache, IOptionsSnapshot<HordeOptions> hordeOptions, ILoggerFactory loggerFactory)
		{
			_serverUrl = serverUrl;
			_httpClientFactory = httpClientFactory;
			_bundleCache = bundleCache;
			_hordeOptions = hordeOptions.Value;
			_loggerFactory = loggerFactory;
			_logger = _loggerFactory.CreateLogger<HordeClient>();
		}

		/// <inheritdoc/>
		public async ValueTask DisposeAsync()
		{
			if (_grpcChannel != null)
			{
				await _grpcChannel.DisposeAsync();
				_grpcChannel = null;
			}
		}

		/// <inheritdoc/>
		public abstract Task<bool> LoginAsync(bool allowLogin, CancellationToken cancellationToken);

		/// <inheritdoc/>
		public abstract bool HasValidAccessToken();

		/// <inheritdoc/>
		public abstract Task<string?> GetAccessTokenAsync(bool interactive, CancellationToken cancellationToken);

		/// <inheritdoc/>
		public async Task<GrpcChannel> CreateGrpcChannelAsync(CancellationToken cancellationToken)
		{
			_grpcChannel ??= BackgroundTask.StartNew(ctx => CreateGrpcChannelInternalAsync(ctx));
			return await _grpcChannel.WaitAsync(cancellationToken);
		}

		async Task<GrpcChannel> CreateGrpcChannelInternalAsync(CancellationToken cancellationToken)
		{
			Uri serverUri = ServerUrl;
			bool useInsecureConnection = serverUri.Scheme.Equals("http", StringComparison.Ordinal);

			// Get the server URL for gRPC traffic. If we're using an unencrpyted connection we need to use a different port for http/2, so 
			// send a http1 request to the server to query it.
			if (useInsecureConnection)
			{
				_logger.LogInformation("Querying server {BaseUrl} for rpc port", serverUri);
				using (HttpClient httpClient = _httpClientFactory.CreateClient())
				{
					httpClient.DefaultRequestHeaders.Add("Accept", "application/json");
					httpClient.Timeout = TimeSpan.FromSeconds(210); // Need to make sure this doesn't cancel any long running gRPC streaming calls (eg. session update)
					using (HttpResponseMessage response = await httpClient.GetAsync(new Uri(serverUri, "api/v1/server/ports"), cancellationToken))
					{
						GetPortsResponse? ports = await response.Content.ReadFromJsonAsync<GetPortsResponse>(HordeHttpClient.JsonSerializerOptions, cancellationToken);
						if (ports != null && ports.UnencryptedHttp2.HasValue && ports.UnencryptedHttp2.Value != 0)
						{
							UriBuilder builder = new UriBuilder(serverUri);
							builder.Port = ports.UnencryptedHttp2.Value;
							serverUri = builder.Uri;
						}
					}
				}
			}

#pragma warning disable CA2000 // Dispose objects before losing scope
			SocketsHttpHandler httpHandler = new SocketsHttpHandler();
#pragma warning restore CA2000 // Dispose objects before losing scope

			// Create options for the new channel
			GrpcChannelOptions channelOptions = new GrpcChannelOptions();
			channelOptions.MaxReceiveMessageSize = 1024 * 1024 * 1024; // 1 GB 		// Required payloads coming from CAS service can be large
			channelOptions.MaxSendMessageSize = 1024 * 1024 * 1024; // 1 GB
			channelOptions.LoggerFactory = _loggerFactory;
			channelOptions.HttpHandler = httpHandler;
			channelOptions.DisposeHttpClient = true;
			channelOptions.ServiceConfig = new ServiceConfig();
			channelOptions.ServiceConfig.MethodConfigs.Add(new MethodConfig
			{
				Names = { MethodName.Default },
				RetryPolicy = new RetryPolicy
				{
					MaxAttempts = 3,
					InitialBackoff = TimeSpan.FromSeconds(1),
					MaxBackoff = TimeSpan.FromSeconds(10),
					BackoffMultiplier = 2.0,
					RetryableStatusCodes = { StatusCode.Unavailable },
				}
			});

			// Configure requests to send the bearer token
			string? bearerToken = await GetAccessTokenAsync(false, cancellationToken);
			if (!String.IsNullOrEmpty(bearerToken))
			{
				CallCredentials callCredentials = CallCredentials.FromInterceptor((context, metadata) =>
				{
					metadata.Add("Authorization", $"Bearer {bearerToken}");
					return Task.CompletedTask;
				});

				if (useInsecureConnection)
				{
					channelOptions.Credentials = ChannelCredentials.Create(ChannelCredentials.Insecure, callCredentials);
				}
				else
				{
					channelOptions.Credentials = ChannelCredentials.Create(ChannelCredentials.SecureSsl, callCredentials);
				}

				channelOptions.UnsafeUseInsecureChannelCallCredentials = useInsecureConnection;
			}

			// Create the channel
			_logger.LogInformation("Connecting to rpc server {BaseUrl}", serverUri);
			return GrpcChannel.ForAddress(serverUri, channelOptions);
		}

		public async Task<TClient> CreateGrpcClientAsync<TClient>(CancellationToken cancellationToken = default)
			where TClient : ClientBase<TClient>
		{
			GrpcChannel channel = await CreateGrpcChannelAsync(cancellationToken);
			return (TClient)Activator.CreateInstance(typeof(TClient), channel)!;
		}

		/// <inheritdoc/>
		public HordeHttpClient CreateHttpClient()
			=> new HordeHttpClient(CreateDefaultHttpClient());

		public IComputeClient CreateComputeClient()
		{
			string? sessionId = null;
			string? jobId = Environment.GetEnvironmentVariable("UE_HORDE_JOBID");
			string? batchId = Environment.GetEnvironmentVariable("UE_HORDE_BATCHID");
			string? stepId = Environment.GetEnvironmentVariable("UE_HORDE_STEPID");

			if (jobId != null && batchId != null && stepId != null)
			{
				sessionId = $"{jobId}-{batchId}-{stepId}";
			}

			return new ServerComputeClient(CreateDefaultHttpClient(), sessionId, _loggerFactory.CreateLogger<ServerComputeClient>());
		}

		/// <inheritdoc/>
		public IStorageClient CreateStorageClient(string basePath, string? accessToken = null)
		{
			HttpClient CreateClient()
			{
				HttpClient httpClient = CreateDefaultHttpClient();
				if (accessToken != null)
				{
					httpClient.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Bearer", accessToken);
				}
				return httpClient;
			}

			HttpClient CreateUploadRedirectClient() => _httpClientFactory.CreateClient(HordeHttpClient.UploadRedirectHttpClientName);
			HttpStorageBackend httpStorageBackend = new HttpStorageBackend(basePath, CreateClient, CreateUploadRedirectClient, _loggerFactory.CreateLogger<HttpStorageBackend>());
			return new BundleStorageClient(httpStorageBackend, _bundleCache, _hordeOptions.Bundle, _loggerFactory.CreateLogger<BundleStorageClient>());
		}

		/// <summary>
		/// Creates an http client for satisfying requests
		/// </summary>
		protected abstract HttpClient CreateDefaultHttpClient();
	}

	/// <summary>
	/// Default implementation of <see cref="IHordeClient"/>
	/// </summary>
	class HordeClientWithStaticCredentials : HordeClient
	{
		readonly string? _accessToken;
		readonly IHttpClientFactory _httpClientFactory;

		/// <summary>
		/// Constructor
		/// </summary>
		public HordeClientWithStaticCredentials(Uri serverUrl, string? accessToken, IHttpClientFactory httpClientFactory, BundleCache bundleCache, IOptionsSnapshot<HordeOptions> hordeOptions, ILoggerFactory loggerFactory)
			: base(serverUrl, httpClientFactory, bundleCache, hordeOptions, loggerFactory)
		{
			_accessToken = accessToken;
			_httpClientFactory = httpClientFactory;
		}

		/// <inheritdoc/>
		public override Task<bool> LoginAsync(bool allowLogin, CancellationToken cancellationToken)
			=> Task.FromResult(true);

		/// <inheritdoc/>
		public override bool HasValidAccessToken()
			=> true;

		/// <inheritdoc/>
		public override Task<string?> GetAccessTokenAsync(bool interactive, CancellationToken cancellationToken)
			=> Task.FromResult(_accessToken);

		protected override HttpClient CreateDefaultHttpClient()
		{
			HttpClient httpClient = _httpClientFactory.CreateClient(HordeHttpClient.HttpClientName);
			httpClient.BaseAddress = ServerUrl;
			if (!String.IsNullOrEmpty(_accessToken))
			{
				httpClient.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Bearer", _accessToken);
			}
			return httpClient;
		}
	}

	/// <summary>
	/// Default implementation of <see cref="IHordeClient"/>
	/// </summary>
	class HordeClientWithDynamicCredentials : HordeClient
	{
		readonly IHttpClientFactory _httpClientFactory;
		readonly HordeHttpAuthHandlerState _authHandler;

		/// <summary>
		/// Constructor
		/// </summary>
		public HordeClientWithDynamicCredentials(Uri serverUrl, IHttpClientFactory httpClientFactory, HordeHttpAuthHandlerState authHandler, BundleCache bundleCache, IOptionsSnapshot<HordeOptions> hordeOptions, ILoggerFactory loggerFactory)
			: base(serverUrl, httpClientFactory, bundleCache, hordeOptions, loggerFactory)
		{
			_httpClientFactory = httpClientFactory;
			_authHandler = authHandler;
		}

		/// <inheritdoc/>
		public override async Task<bool> LoginAsync(bool allowLogin, CancellationToken cancellationToken)
		{
			return await _authHandler.LoginAsync(allowLogin, cancellationToken);
		}

		/// <inheritdoc/>
		public override bool HasValidAccessToken()
		{
			try
			{
				return _authHandler.IsLoggedIn();
			}
			catch
			{
				return false;
			}
		}

		/// <inheritdoc/>
		public override Task<string?> GetAccessTokenAsync(bool interactive, CancellationToken cancellationToken)
			=> _authHandler.GetAccessTokenAsync(interactive, cancellationToken);

		/// <inheritdoc/>
		protected override HttpClient CreateDefaultHttpClient()
		{
			HttpClient httpClient = _httpClientFactory.CreateClient(HordeHttpClient.HttpClientName);
			httpClient.BaseAddress = ServerUrl;
			return httpClient;
		}
	}

	/// <summary>
	/// Implementation of <see cref="IHordeClientFactory"/>
	/// </summary>
	class HordeClientFactory : IHordeClientFactory
	{
		readonly IHttpClientFactory _httpClientFactory;
		readonly BundleCache _bundleCache;
		readonly HordeHttpAuthHandlerState _authHandlerState;
		readonly IOptionsSnapshot<HordeOptions> _hordeOptions;
		readonly ILoggerFactory _loggerFactory;

		public HordeClientFactory(IHttpClientFactory httpClientFactory, BundleCache bundleCache, HordeHttpAuthHandlerState authHandlerState, IOptionsSnapshot<HordeOptions> hordeOptions, ILoggerFactory loggerFactory)
		{
			_httpClientFactory = httpClientFactory;
			_bundleCache = bundleCache;
			_authHandlerState = authHandlerState;
			_hordeOptions = hordeOptions;
			_loggerFactory = loggerFactory;
		}

		Uri GetServerUrl()
		{
			Uri? serverUrl = _hordeOptions.Value.GetServerUrlOrDefault();
			if (serverUrl == null)
			{
				throw new Exception("No Horde server is configured, or can be detected from the environment. Consider specifying a URL when calling AddHordeHttpClient().");
			}
			return serverUrl;
		}

		/// <inheritdoc/>
		public IHordeClient Create()
		{
			Uri serverUrl = GetServerUrl();
			return new HordeClientWithDynamicCredentials(serverUrl, _httpClientFactory, _authHandlerState, _bundleCache, _hordeOptions, _loggerFactory);
		}

		/// <inheritdoc/>
		public IHordeClient Create(string? accessToken)
		{
			Uri serverUrl = GetServerUrl();
			return new HordeClientWithStaticCredentials(serverUrl, accessToken, _httpClientFactory, _bundleCache, _hordeOptions, _loggerFactory);
		}
	}
}
