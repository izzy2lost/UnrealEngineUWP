// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Net.Http.Json;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Artifacts;
using EpicGames.Horde.Compute;
using EpicGames.Horde.Compute.Clients;
using EpicGames.Horde.Logs;
using EpicGames.Horde.Server;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Backends;
using EpicGames.Horde.Storage.Bundles;
using Grpc.Core;
using Grpc.Net.Client;
using Grpc.Net.Client.Configuration;
using Microsoft.Extensions.Http;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using Polly;
using Polly.Retry;
using Polly.Timeout;

#pragma warning disable CA2234 // Use URIs instead of strings

namespace EpicGames.Horde
{
	/// <summary>
	/// Default implementation of <see cref="IHordeClient"/>
	/// </summary>
	abstract class HordeClient : IHordeClient
	{
		readonly Uri _serverUrl;
		readonly BundleCache _bundleCache;
		readonly HordeOptions _hordeOptions;
		readonly ILoggerFactory _loggerFactory;
		readonly ILogger _logger;

		BackgroundTask<GrpcChannel>? _grpcChannel;

		/// <inheritdoc/>
		public Uri ServerUrl => _serverUrl;

		/// <inheritdoc/>
		public IArtifactCollection Artifacts { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public HordeClient(Uri serverUrl, BundleCache bundleCache, IOptionsSnapshot<HordeOptions> hordeOptions, ILoggerFactory loggerFactory)
		{
			_serverUrl = serverUrl;
			_bundleCache = bundleCache;
			_hordeOptions = hordeOptions.Value;
			_loggerFactory = loggerFactory;
			_logger = _loggerFactory.CreateLogger<HordeClient>();

			Artifacts = new ArtifactHttpCollection(this);
		}

		/// <inheritdoc/>
		public virtual async ValueTask DisposeAsync()
		{
			if (_grpcChannel != null)
			{
				await _grpcChannel.DisposeAsync();
				_grpcChannel = null;
			}
		}

		/// <summary>
		/// Creates a new message handler with default resilience policies
		/// </summary>
		protected HttpMessageHandler CreateDefaultHttpMessageHandler()
		{
			HttpMessageHandler? httpMessageHandler = null;
			try
			{
#pragma warning disable CA2000 // Call dispose on httpMessageHandler (disposed by child handlers)
				httpMessageHandler = new SocketsHttpHandler();
				httpMessageHandler = new PolicyHttpMessageHandler(request => CreateDefaultTimeoutRetryPolicy(request)) { InnerHandler = httpMessageHandler };
				httpMessageHandler = new PolicyHttpMessageHandler(request => CreateDefaultTransientErrorPolicy(request)) { InnerHandler = httpMessageHandler };
#pragma warning restore CA2000
				return httpMessageHandler;
			}
			catch
			{
				httpMessageHandler?.Dispose();
				throw;
			}
		}

		/// <summary>
		/// Create a default timeout retry policy
		/// </summary>
		protected IAsyncPolicy<HttpResponseMessage> CreateDefaultTimeoutRetryPolicy(HttpRequestMessage request)
		{
			// Wait 30 seconds for operations to timeout
			Task OnTimeoutAsync(Context context, TimeSpan timespan, Task timeoutTask)
			{
				_logger.LogWarning(KnownLogEvents.Systemic_Horde_Http, "{Method} {Url} timed out after {Time}s.", request.Method, request.RequestUri, (int)timespan.TotalSeconds);
				return Task.CompletedTask;
			}

			AsyncTimeoutPolicy<HttpResponseMessage> timeoutPolicy = Policy.TimeoutAsync<HttpResponseMessage>(30, OnTimeoutAsync);

			// Retry twice after a timeout
			void OnRetry(Exception ex, TimeSpan timespan)
			{
				_logger.LogWarning(KnownLogEvents.Systemic_Horde_Http, ex, "{Method} {Url} retrying after {Time}s.", request.Method, request.RequestUri, timespan.TotalSeconds);
			}

			TimeSpan[] retryTimes = new[] { TimeSpan.FromSeconds(5.0), TimeSpan.FromSeconds(10.0) };
			AsyncRetryPolicy retryPolicy = Policy.Handle<TimeoutRejectedException>().WaitAndRetryAsync(retryTimes, OnRetry);
			return retryPolicy.WrapAsync(timeoutPolicy);
		}

		/// <summary>
		/// Create a default timeout retry policy
		/// </summary>
		protected IAsyncPolicy<HttpResponseMessage> CreateDefaultTransientErrorPolicy(HttpRequestMessage request)
		{
			Task OnTimeoutAsync(DelegateResult<HttpResponseMessage> outcome, TimeSpan timespan, int retryAttempt, Context context)
			{
				_logger.LogWarning(KnownLogEvents.Systemic_Horde_Http, "{Method} {Url} failed ({Result}). Delaying for {DelayMs}ms (attempt #{RetryNum}).", request.Method, request.RequestUri, outcome.Result?.StatusCode, timespan.TotalMilliseconds, retryAttempt);
				return Task.CompletedTask;
			}

			TimeSpan[] retryTimes = new[] { TimeSpan.FromSeconds(1.0), TimeSpan.FromSeconds(5.0), TimeSpan.FromSeconds(10.0), TimeSpan.FromSeconds(30.0), TimeSpan.FromSeconds(30.0) };

			// Policy for transient errors is the same as HttpPolicyExtensions.HandleTransientHttpError(), but excludes HttpStatusCode.ServiceUnavailable (which is used as a response
			// when allocating compute resources when none are available). This pathway is handled explicitly on the application side.
			return Policy<HttpResponseMessage>
				.Handle<HttpRequestException>()
				.OrResult(x => (x.StatusCode > HttpStatusCode.InternalServerError && x.StatusCode != HttpStatusCode.ServiceUnavailable) || x.StatusCode == HttpStatusCode.RequestTimeout)
				.WaitAndRetryAsync(retryTimes, OnTimeoutAsync);
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
				using (HttpClient httpClient = CreateUnauthenticatedHttpClient())
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
				RetryPolicy = new Grpc.Net.Client.Configuration.RetryPolicy
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
			=> new HordeHttpClient(CreateAuthenticatedHttpClient());

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

			return new ServerComputeClient(CreateAuthenticatedHttpClient(), sessionId, _loggerFactory.CreateLogger<ServerComputeClient>());
		}

		/// <inheritdoc/>
		public IStorageClient CreateStorageClient(string basePath, string? accessToken = null)
		{
			HttpClient CreateClient()
			{
				if (accessToken != null)
				{
					HttpClient httpClient = CreateUnauthenticatedHttpClient();
					httpClient.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Bearer", accessToken);
					return httpClient;
				}
				else
				{
					return CreateAuthenticatedHttpClient();
				}
			}

			HttpStorageBackend httpStorageBackend = new HttpStorageBackend(basePath, CreateClient, CreateUnauthenticatedHttpClient, _loggerFactory.CreateLogger<HttpStorageBackend>());
			return new BundleStorageClient(httpStorageBackend, _bundleCache, _hordeOptions.Bundle, _loggerFactory.CreateLogger<BundleStorageClient>());
		}

		/// <inheritdoc/>
		public IServerLogger CreateServerLogger(LogId logId, LogLevel minimumLevel = LogLevel.Information)
			=> new ServerLogger(this, logId, minimumLevel, _logger);

		/// <summary>
		/// Creates an http client for satisfying requests
		/// </summary>
		protected abstract HttpClient CreateAuthenticatedHttpClient();

		/// <summary>
		/// Creates an http client for satisfying requests
		/// </summary>
		protected abstract HttpClient CreateUnauthenticatedHttpClient();
	}

	/// <summary>
	/// Default implementation of <see cref="IHordeClient"/>
	/// </summary>
	class HordeClientWithStaticCredentials : HordeClient
	{
		readonly string? _accessToken;
		readonly HttpMessageHandler _httpMessageHandler;

		/// <summary>
		/// Constructor
		/// </summary>
		public HordeClientWithStaticCredentials(Uri serverUrl, string? accessToken, BundleCache bundleCache, IOptionsSnapshot<HordeOptions> hordeOptions, ILoggerFactory loggerFactory)
			: base(serverUrl, bundleCache, hordeOptions, loggerFactory)
		{
			_accessToken = accessToken;
			_httpMessageHandler = CreateDefaultHttpMessageHandler();
		}

		/// <inheritdoc/>
		public override async ValueTask DisposeAsync()
		{
			_httpMessageHandler.Dispose();
			await base.DisposeAsync();
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

		/// <inheritdoc/>
		protected override HttpClient CreateAuthenticatedHttpClient()
		{
			HttpClient httpClient = new HttpClient(_httpMessageHandler, false);
			httpClient.BaseAddress = ServerUrl;
			if (!String.IsNullOrEmpty(_accessToken))
			{
				httpClient.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Bearer", _accessToken);
			}
			return httpClient;
		}

		/// <inheritdoc/>
		protected override HttpClient CreateUnauthenticatedHttpClient()
		{
			HttpClient httpClient = new HttpClient(_httpMessageHandler, false);
			httpClient.BaseAddress = ServerUrl;
			return httpClient;
		}
	}

	/// <summary>
	/// Default implementation of <see cref="IHordeClient"/>
	/// </summary>
	class HordeClientWithDynamicCredentials : HordeClient
	{
		readonly HordeHttpAuthHandlerState _authHandlerState;
		readonly HttpMessageHandler _baseHttpMessageHandler;
		readonly HttpMessageHandler _authHttpMessageHandler;

		/// <summary>
		/// Constructor
		/// </summary>
		public HordeClientWithDynamicCredentials(Uri serverUrl, BundleCache bundleCache, IOptionsSnapshot<HordeOptions> hordeOptions, ILoggerFactory loggerFactory)
			: base(serverUrl, bundleCache, hordeOptions, loggerFactory)
		{
			_baseHttpMessageHandler = CreateDefaultHttpMessageHandler();

			_authHandlerState = new HordeHttpAuthHandlerState(_baseHttpMessageHandler, serverUrl, hordeOptions, loggerFactory.CreateLogger<HordeHttpAuthHandlerState>());
			_authHttpMessageHandler = new HordeHttpAuthHandler(_baseHttpMessageHandler, _authHandlerState, hordeOptions);
		}

		/// <inheritdoc/>
		public override async ValueTask DisposeAsync()
		{
			await _authHandlerState.DisposeAsync();
			_authHttpMessageHandler.Dispose();
			_baseHttpMessageHandler.Dispose();
			await base.DisposeAsync();
		}

		/// <inheritdoc/>
		public override async Task<bool> LoginAsync(bool allowLogin, CancellationToken cancellationToken)
		{
			return await _authHandlerState.LoginAsync(allowLogin, cancellationToken);
		}

		/// <inheritdoc/>
		public override bool HasValidAccessToken()
		{
			try
			{
				return _authHandlerState.IsLoggedIn();
			}
			catch
			{
				return false;
			}
		}

		/// <inheritdoc/>
		public override Task<string?> GetAccessTokenAsync(bool interactive, CancellationToken cancellationToken)
			=> _authHandlerState.GetAccessTokenAsync(interactive, cancellationToken);

		/// <inheritdoc/>
		protected override HttpClient CreateAuthenticatedHttpClient()
		{
			HttpClient httpClient = new HttpClient(_authHttpMessageHandler, false);
			httpClient.BaseAddress = ServerUrl;
			return httpClient;
		}

		/// <inheritdoc/>
		protected override HttpClient CreateUnauthenticatedHttpClient()
		{
			HttpClient httpClient = new HttpClient(_baseHttpMessageHandler, false);
			httpClient.BaseAddress = ServerUrl;
			return httpClient;
		}
	}

	/// <summary>
	/// Implementation of <see cref="IHordeClientFactory"/>
	/// </summary>
	class HordeClientFactory : IHordeClientFactory
	{
		readonly BundleCache _bundleCache;
		readonly IOptionsSnapshot<HordeOptions> _hordeOptions;
		readonly ILoggerFactory _loggerFactory;

		public HordeClientFactory(BundleCache bundleCache, IOptionsSnapshot<HordeOptions> hordeOptions, ILoggerFactory loggerFactory)
		{
			_bundleCache = bundleCache;
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
			return new HordeClientWithDynamicCredentials(serverUrl, _bundleCache, _hordeOptions, _loggerFactory);
		}

		/// <inheritdoc/>
		public IHordeClient Create(string? accessToken)
		{
			Uri serverUrl = GetServerUrl();
			return new HordeClientWithStaticCredentials(serverUrl, accessToken, _bundleCache, _hordeOptions, _loggerFactory);
		}
	}
}
