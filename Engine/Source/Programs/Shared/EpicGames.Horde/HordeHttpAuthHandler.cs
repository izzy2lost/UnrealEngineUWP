// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Configuration;
using EpicGames.OIDC;
using System.Net.Http.Json;
using System.Text.Json;
using EpicGames.Horde.Server;

namespace EpicGames.Horde
{
	/// <summary>
	/// HTTP message handler which automatically refreshes access tokens as required
	/// </summary>
	public class HordeHttpAuthHandler : DelegatingHandler
	{
		readonly HordeHttpAuthHandlerState _authState;

		AuthenticationHeaderValue? _authHeader;

		/// <summary>
		/// Constructor
		/// </summary>
		public HordeHttpAuthHandler(HordeHttpAuthHandlerState authState)
		{
			_authState = authState;
		}

		/// <inheritdoc/>
		protected override async Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken cancellationToken)
		{
			// Do not try to override the auth header if the user has specified it explicitly
			if (request.Headers.Authorization != null)
			{
				return await base.SendAsync(request, cancellationToken);
			}

			// Try to use the cached auth header
			if (_authHeader != null)
			{
				request.Headers.Authorization = _authHeader;

				HttpResponseMessage response = await base.SendAsync(request, cancellationToken);
				if (response.StatusCode != HttpStatusCode.Unauthorized)
				{
					return response;
				}

				_authState.Invalidate(_authHeader);
			}

			// Otherwise update the auth header and try again
			_authHeader = await _authState.TryGetAuthHeaderAsync(cancellationToken);
			if (_authHeader != null)
			{
				request.Headers.Authorization = _authHeader;
			}
			return await base.SendAsync(request, cancellationToken);
		}
	}

	/// <summary>
	/// Shared object used to track the latest access obtained token
	/// </summary>
	public sealed class HordeHttpAuthHandlerState : IAsyncDisposable
	{
		/// <summary>
		/// HTTP client name
		/// </summary>
		public const string HttpClientName = "HordeHttpAuthState";

		readonly object _lockObject = new object();
		readonly CancellationTokenSource _cancellationTokenSource = new CancellationTokenSource();
		Task<AuthenticationHeaderValue?>? _authHeaderTask;
		readonly IHttpClientFactory _httpClientFactory;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public HordeHttpAuthHandlerState(IHttpClientFactory httpClientFactory, ILogger<HordeHttpAuthHandler> logger)
		{
			_httpClientFactory = httpClientFactory;
			_logger = logger;
		}

		/// <inheritdoc/>
		public async ValueTask DisposeAsync()
		{
			if (_authHeaderTask != null)
			{
				_cancellationTokenSource.Cancel();
				await _authHeaderTask;
				_authHeaderTask = null;
			}
			_cancellationTokenSource.Dispose();
		}

		/// <summary>
		/// Invalidate a cached header value
		/// </summary>
		/// <param name="authHeader">The auth header to invalidate</param>
		public void Invalidate(AuthenticationHeaderValue authHeader)
		{
			lock (_lockObject)
			{
				if (_authHeaderTask != null && _authHeaderTask.IsCompleted && Object.Equals(_authHeaderTask.Result, authHeader))
				{
					_authHeaderTask = null;
				}
			}
		}

		/// <summary>
		/// Gets a new auth header
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async ValueTask<AuthenticationHeaderValue?> TryGetAuthHeaderAsync(CancellationToken cancellationToken)
		{
			Task<AuthenticationHeaderValue?>? authHeaderTask = _authHeaderTask;
			if (authHeaderTask == null)
			{
				lock (_lockObject)
				{
					_authHeaderTask ??= Task.Run(() => GetNewAuthHeaderAsync(_cancellationTokenSource.Token), _cancellationTokenSource.Token);
					authHeaderTask = _authHeaderTask;
				}
			}
			return await authHeaderTask.WaitAsync(cancellationToken);
		}

		/// <summary>
		/// Get an access token for the server specified in a config instance
		/// </summary>
		async Task<AuthenticationHeaderValue?> GetNewAuthHeaderAsync(CancellationToken cancellationToken)
		{
			Uri serverUrl;

			GetAuthConfigResponse? authConfig;
			using (HttpClient httpClient = _httpClientFactory.CreateClient(HttpClientName))
			{
				if (httpClient.BaseAddress == null)
				{
					throw new Exception("No http client is configured for Horde. Call IServiceCollection.AddHordeHttpClient().");
				}

				serverUrl = httpClient.BaseAddress;
				_logger.LogInformation("Retrieving auth configuration for {Server}", serverUrl);

				JsonSerializerOptions jsonOptions = new JsonSerializerOptions();
				HordeHttpClient.ConfigureJsonSerializer(jsonOptions);

				authConfig = await httpClient.GetFromJsonAsync<GetAuthConfigResponse>("api/v1/server/auth", jsonOptions, cancellationToken);
				if (authConfig == null)
				{
					throw new Exception($"Invalid response from server");
				}
			}

			string? localRedirectUrl = authConfig.LocalRedirectUrls?.FirstOrDefault();
			if (String.IsNullOrEmpty(authConfig.ServerUrl) || String.IsNullOrEmpty(localRedirectUrl))
			{
				return null;
			}

			const string OidcProvider = "Horde";

			Dictionary<string, string?> values = new Dictionary<string, string?>();
			values[$"Providers:{OidcProvider}:DisplayName"] = "Horde";
			values[$"Providers:{OidcProvider}:ServerUri"] = authConfig.ServerUrl;
			values[$"Providers:{OidcProvider}:ClientId"] = authConfig.ClientId;
			values[$"Providers:{OidcProvider}:RedirectUri"] = localRedirectUrl;

			ConfigurationBuilder builder = new ConfigurationBuilder();
			builder.AddInMemoryCollection(values);

			IConfiguration configuration = builder.Build();

			using ITokenStore tokenStore = TokenStoreFactory.CreateTokenStore();
			OidcTokenManager oidcTokenManager = OidcTokenManager.CreateTokenManager(configuration, tokenStore, new List<string>() { OidcProvider });

			OidcTokenInfo? result = null;
			if (oidcTokenManager.GetStatusForProvider(OidcProvider) != OidcStatus.NotLoggedIn)
			{
				try
				{
					result = await oidcTokenManager.TryGetAccessToken(OidcProvider, cancellationToken);
				}
				catch (Exception ex)
				{
					_logger.LogTrace(ex, "Unable to get access token; attempting login: {Message}", ex.Message);
				}
			}
			if (result == null)
			{
				_logger.LogInformation("Logging in to {Server}...", serverUrl);
				result = await oidcTokenManager.Login(OidcProvider, cancellationToken);
			}

			if (result.AccessToken == null)
			{
				throw new Exception($"Unable to get access token for {serverUrl}");
			}

			_logger.LogInformation("Received access token for {Server}", serverUrl);
			return new AuthenticationHeaderValue("Bearer", result.AccessToken);
		}
	}
}
