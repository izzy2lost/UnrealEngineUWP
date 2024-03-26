// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Net.Http.Json;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Server;
using EpicGames.OIDC;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace EpicGames.Horde
{
	/// <summary>
	/// HTTP message handler which automatically refreshes access tokens as required
	/// </summary>
	public class HordeHttpAuthHandler : DelegatingHandler
	{
		readonly HordeHttpAuthHandlerState _authState;
		readonly IOptions<HordeOptions> _options;

		AuthenticationHeaderValue? _authHeader;

		/// <summary>
		/// Constructor
		/// </summary>
		public HordeHttpAuthHandler(HordeHttpAuthHandlerState authState, IOptions<HordeOptions> options)
		{
			_authState = authState;
			_options = options;
		}

		/// <inheritdoc/>
		protected override async Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken cancellationToken)
		{
			if (request.Headers.Authorization == null)
			{
				AuthenticationHeaderValue? configuredAuthHeader = _authState.TryGetConfiguredAuthHeader();
				if (configuredAuthHeader != null)
				{
					// Use the configured header
					request.Headers.Authorization = configuredAuthHeader;
				}
				else if (_options.Value.AllowAuthPrompt)
				{
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
				}
				else
				{
					// Use whatever cached auth header we currently have
					request.Headers.Authorization = _authHeader;
				}
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

		record class AuthState(AuthMethod Method, OidcTokenInfo? TokenInfo)
		{
			public bool IsAuthorized()
				=> (Method == AuthMethod.Anonymous) || (TokenInfo != null && TokenInfo.IsValid && TokenInfo.TokenExpiry > DateTimeOffset.Now);
		}

		readonly object _lockObject = new object();
		readonly CancellationTokenSource _cancellationTokenSource = new CancellationTokenSource();
		Task<AuthState>? _authStateTask = null;
		readonly IHttpClientFactory _httpClientFactory;
		readonly IOptions<HordeOptions> _options;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public HordeHttpAuthHandlerState(IHttpClientFactory httpClientFactory, IOptions<HordeOptions> options, ILogger<HordeHttpAuthHandler> logger)
		{
			_httpClientFactory = httpClientFactory;
			_options = options;
			_logger = logger;
		}

		/// <inheritdoc/>
		public async ValueTask DisposeAsync()
		{
			if (_authStateTask != null && !_authStateTask.IsCompleted)
			{
				_cancellationTokenSource.Cancel();
				try
				{
#pragma warning disable VSTHRD003
					await _authStateTask;
#pragma warning restore VSTHRD003
				}
				catch (OperationCanceledException)
				{
				}
			}

			_cancellationTokenSource.Dispose();
		}

		/// <summary>
		/// Checks if we have a valid auth header at the moment
		/// </summary>
		public bool IsAuthenticated()
		{
			if (TryGetConfiguredAuthHeader() != null)
			{
				return true;
			}
			if (_authStateTask != null && _authStateTask.TryGetResult(out AuthState? authState) && authState.IsAuthorized())
			{
				return true;
			}
			return false;
		}

		/// <summary>
		/// Invalidate the cached header value
		/// </summary>
		public void Invalidate()
		{
			lock (_lockObject)
			{
#pragma warning disable VSTHRD002
				if (_authStateTask != null && _authStateTask.IsCompleted)
				{
					_authStateTask = null;
				}
#pragma warning restore VSTHRD002
			}
		}

		/// <summary>
		/// Invalidate a cached header value
		/// </summary>
		/// <param name="authHeader">The auth header to invalidate</param>
		public void Invalidate(AuthenticationHeaderValue authHeader)
		{
			lock (_lockObject)
			{
#pragma warning disable VSTHRD002
				if (_authStateTask != null && _authStateTask.IsCompleted && Object.Equals(_authStateTask.Result?.TokenInfo?.AccessToken, authHeader.Parameter))
				{
					_authStateTask = null;
				}
#pragma warning restore VSTHRD002
			}
		}

		/// <summary>
		/// Try to get a configured auth header
		/// </summary>
		public AuthenticationHeaderValue? TryGetConfiguredAuthHeader()
		{
			if (_options.Value.AccessToken != null)
			{
				// If an explicit access token is specified, just use that
				return new AuthenticationHeaderValue("Bearer", _options.Value.AccessToken);
			}
			else if (TryGetAccessTokenFromEnvironment(out string? accessToken))
			{
				// Use the access token specified in the environment
				return new AuthenticationHeaderValue("Bearer", accessToken);
			}
			else
			{
				// Will need to login asynchronously
				return null;
			}
		}

		bool TryGetAccessTokenFromEnvironment(out string? accessToken)
		{
			// Only use the token from the environment if the configured base address is missing or matches the one configured in the environment
			string? hordeUrlEnvVar = Environment.GetEnvironmentVariable(HordeHttpClient.HordeUrlEnvVarName);
			if (!String.IsNullOrEmpty(hordeUrlEnvVar))
			{
				Uri hordeUrl = new Uri(hordeUrlEnvVar);
				if (_options.Value.ServerUrl == null || String.Equals(_options.Value.ServerUrl.Host, hordeUrl.Host, StringComparison.OrdinalIgnoreCase))
				{
					string? hordeToken = Environment.GetEnvironmentVariable(HordeHttpClient.HordeTokenEnvVarName);
					if (!String.IsNullOrEmpty(hordeToken))
					{
						accessToken = hordeToken;
						return true;
					}
				}
			}

			accessToken = null;
			return false;
		}

		/// <summary>
		/// Refresh the auth state
		/// </summary>
		/// <param name="allowLogin">Whether to allow logging in</param>
		/// <param name="cancellationToken"></param>
		/// <returns></returns>
		public async Task RefreshAsync(bool allowLogin, CancellationToken cancellationToken)
		{
			Invalidate();
			await GetAuthStateAsync(allowLogin, cancellationToken);
		}

		/// <summary>
		/// Gets a new auth header
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async ValueTask<AuthenticationHeaderValue?> TryGetAuthHeaderAsync(CancellationToken cancellationToken)
		{
			AuthState? authState = await GetAuthStateAsync(true, cancellationToken);
			if (authState?.TokenInfo == null)
			{
				return null;
			}
			return new AuthenticationHeaderValue("Bearer", authState.TokenInfo.AccessToken);
		}

		async Task<AuthState?> GetAuthStateAsync(bool allowLogin, CancellationToken cancellationToken)
		{
			if (TryGetConfiguredAuthHeader() != null)
			{
				return null;
			}

			Task<AuthState> authStateTask;
			lock (_lockObject)
			{
				_authStateTask ??= Task.Run(() => GetAuthStateInternalAsync(allowLogin, _cancellationTokenSource.Token), _cancellationTokenSource.Token);
				authStateTask = _authStateTask;
			}
			return await authStateTask.WaitAsync(cancellationToken);
		}

		async Task<AuthState> GetAuthStateInternalAsync(bool allowLogin, CancellationToken cancellationToken)
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
				_logger.LogDebug("Retrieving auth configuration for {Server}", serverUrl);

				JsonSerializerOptions jsonOptions = new JsonSerializerOptions();
				HordeHttpClient.ConfigureJsonSerializer(jsonOptions);

				authConfig = await httpClient.GetFromJsonAsync<GetAuthConfigResponse>("api/v1/server/auth", jsonOptions, cancellationToken);
				if (authConfig == null)
				{
					throw new Exception($"Invalid response from server");
				}
			}

			if (authConfig.Method == AuthMethod.Anonymous)
			{
				return new AuthState(authConfig.Method, null);
			}

			string? localRedirectUrl = authConfig.LocalRedirectUrls?.FirstOrDefault();
			if (String.IsNullOrEmpty(authConfig.ServerUrl) || String.IsNullOrEmpty(localRedirectUrl))
			{
				throw new Exception("No auth server configuration found");
			}

			string oidcProvider = authConfig.ProfileName ?? "Horde";

			Dictionary<string, string?> values = new Dictionary<string, string?>();
			values[$"Providers:{oidcProvider}:DisplayName"] = "Horde";
			values[$"Providers:{oidcProvider}:ServerUri"] = authConfig.ServerUrl;
			values[$"Providers:{oidcProvider}:ClientId"] = authConfig.ClientId;
			values[$"Providers:{oidcProvider}:RedirectUri"] = localRedirectUrl;

			ConfigurationBuilder builder = new ConfigurationBuilder();
			builder.AddInMemoryCollection(values);

			IConfiguration configuration = builder.Build();

			using ITokenStore tokenStore = TokenStoreFactory.CreateTokenStore();
			OidcTokenManager oidcTokenManager = OidcTokenManager.CreateTokenManager(configuration, tokenStore, new List<string>() { oidcProvider });

			OidcTokenInfo? result = null;
			if (oidcTokenManager.GetStatusForProvider(oidcProvider) != OidcStatus.NotLoggedIn)
			{
				try
				{
					result = await oidcTokenManager.TryGetAccessToken(oidcProvider, cancellationToken);
				}
				catch (Exception ex)
				{
					_logger.LogTrace(ex, "Unable to get access token; attempting login: {Message}", ex.Message);
				}
			}
			if (result == null && allowLogin)
			{
				_logger.LogInformation("Logging in to {Server}...", serverUrl);
				result = await oidcTokenManager.Login(oidcProvider, cancellationToken);
			}

			return new AuthState(authConfig.Method, result);
		}
	}
}
