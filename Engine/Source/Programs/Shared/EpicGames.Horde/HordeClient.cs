// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Net.Http;
using System.Net.Http.Json;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Server;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Backends;
using EpicGames.Horde.Storage.Bundles;
using Grpc.Core;
using Grpc.Net.Client;
using Grpc.Net.Client.Configuration;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace EpicGames.Horde
{
	/// <summary>
	/// Default implementation of <see cref="IHordeClient"/>
	/// </summary>
	sealed class HordeClient : IHordeClient, IAsyncDisposable
	{
		readonly IHttpClientFactory _httpClientFactory;
		readonly HordeHttpAuthHandlerState _authHandlerState;
		readonly BundleCache _bundleCache;
		readonly HordeOptions _hordeOptions;
		readonly ILoggerFactory _loggerFactory;

		BackgroundTask<GrpcChannel>? _grpcChannel;
		Uri? _serverUrl;

		/// <inheritdoc/>
		public Uri ServerUrl => GetServerUrl();

		/// <summary>
		/// Constructor
		/// </summary>
		public HordeClient(IHttpClientFactory httpClientFactory, HordeHttpAuthHandlerState authHandlerState, BundleCache bundleCache, IOptionsSnapshot<HordeOptions> hordeOptions, ILoggerFactory loggerFactory)
		{
			_httpClientFactory = httpClientFactory;
			_authHandlerState = authHandlerState;
			_bundleCache = bundleCache;
			_hordeOptions = hordeOptions.Value;
			_loggerFactory = loggerFactory;
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

		Uri GetServerUrl()
		{
			if (_serverUrl == null)
			{
				using HttpClient httpClient = _httpClientFactory.CreateClient(HordeHttpClient.HttpClientName);
				_serverUrl = httpClient.BaseAddress ?? new Uri("http://horde-server");
			}
			return _serverUrl;
		}

		/// <inheritdoc/>
		public async Task<bool> ConnectAsync(bool allowLogin, CancellationToken cancellationToken)
		{
			return await _authHandlerState.LoginAsync(allowLogin, cancellationToken);
		}

		/// <inheritdoc/>
		public bool IsConnected()
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
		public async Task<GrpcChannel> GetGrpcChannelAsync(CancellationToken cancellationToken)
		{
			_grpcChannel ??= BackgroundTask.StartNew(ctx => CreateGrpcChannelInternalAsync(ctx));
			return await _grpcChannel.WaitAsync(cancellationToken);
		}

		async Task<GrpcChannel> CreateGrpcChannelInternalAsync(CancellationToken cancellationToken)
		{
			HttpClient httpClient = _httpClientFactory.CreateClient(HordeHttpClient.HttpClientName);

			// Get the server URL for gRPC traffic. If we're using an unencrpyted connection we need to use a different port for http/2, so 
			// send a http1 request to the server to query it.
			Uri serverUri = httpClient.BaseAddress ?? throw new InvalidOperationException("Horde server base address is not configured");
			if (serverUri.Scheme.Equals("http", StringComparison.Ordinal))
			{
				using (HttpResponseMessage response = await httpClient.GetAsync("api/v1/server/ports", cancellationToken))
				{
					GetPortsResponse? ports = await response.Content.ReadFromJsonAsync<GetPortsResponse>(HordeHttpClient.JsonSerializerOptions, cancellationToken);
					if (ports != null && ports.UnencryptedHttp2.HasValue && ports.UnencryptedHttp2 != 0)
					{
						UriBuilder builder = new UriBuilder(serverUri);
						builder.Port = ports.UnencryptedHttp2.Value;
						serverUri = builder.Uri;
					}
				}
			}

			ServiceConfig serviceConfig = new ServiceConfig();
			serviceConfig.MethodConfigs.Add(new MethodConfig
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

			return GrpcChannel.ForAddress(serverUri, new GrpcChannelOptions
			{
				// Required payloads coming from CAS service can be large
				MaxReceiveMessageSize = 1024 * 1024 * 1024, // 1 GB
				MaxSendMessageSize = 1024 * 1024 * 1024, // 1 GB
				LoggerFactory = _loggerFactory,
				HttpClient = httpClient,
				DisposeHttpClient = true,
				ServiceConfig = serviceConfig
			});
		}

		/// <inheritdoc/>
		public HordeHttpClient CreateHttpClient()
			=> _httpClientFactory.CreateHordeHttpClient();

		/// <inheritdoc/>
		public IStorageClient CreateStorageClient(string basePath)
		{
			HttpClient CreateClient() => _httpClientFactory.CreateClient(HordeHttpClient.HttpClientName);
			HttpClient CreateUploadRedirectClient() => _httpClientFactory.CreateClient(HordeHttpClient.UploadRedirectHttpClientName);
			HttpStorageBackend httpStorageBackend = new HttpStorageBackend(basePath, CreateClient, CreateUploadRedirectClient, _loggerFactory.CreateLogger<HttpStorageBackend>());
			return new BundleStorageClient(httpStorageBackend, _bundleCache, _hordeOptions.Bundle, _loggerFactory.CreateLogger<BundleStorageClient>());
		}
	}
}
