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
using EpicGames.Horde.Api;
using System.Net.Http.Json;
using System.Text.Json;

namespace EpicGames.Horde
{
	/// <summary>
	/// HTTP message handler which automatically refreshes access tokens as required
	/// </summary>
	public class HordeHttpAuthHandler : DelegatingHandler
	{
		const string ClientName = "HordeHttpAuth";

		readonly IHttpClientFactory _httpClientFactory;
		readonly ILogger _logger;

		AuthenticationHeaderValue? _authHeader;

		/// <summary>
		/// Constructor
		/// </summary>
		public HordeHttpAuthHandler(IHttpClientFactory httpClientFactory, ILogger<HordeHttpAuthHandler> logger)
		{
			_httpClientFactory = httpClientFactory;
			_logger = logger;
		}

		/// <inheritdoc/>
		protected override async Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken cancellationToken)
		{
			HttpResponseMessage? response = null;
			if (_authHeader != null)
			{
				request.Headers.Authorization = _authHeader;
				response = await base.SendAsync(request, cancellationToken);
			}
			if (response == null || response.StatusCode == HttpStatusCode.Unauthorized)
			{
				if (request.RequestUri != null)
				{
					await RefreshAccessTokenAsync(request.RequestUri, cancellationToken);
				}

				request.Headers.Authorization = _authHeader;
				response = await base.SendAsync(request, cancellationToken);
			}
			return response;
		}

		/// <summary>
		/// Get an access token for the server specified in a config instance
		/// </summary>
		async Task RefreshAccessTokenAsync(Uri requestUrl, CancellationToken cancellationToken)
		{
			Uri serverUrl = new Uri(requestUrl, "/");
			_logger.LogInformation("Getting access token for {Server}", serverUrl);

			GetAuthConfigResponse? authConfig;
			using (HttpClient httpClient = _httpClientFactory.CreateClient(ClientName))
			{
				Uri uri = new Uri(serverUrl, "api/v1/server/auth");

				JsonSerializerOptions jsonOptions = new JsonSerializerOptions();
				HordeHttpClient.ConfigureJsonSerializer(jsonOptions);

				authConfig = await httpClient.GetFromJsonAsync<GetAuthConfigResponse>(uri, jsonOptions, cancellationToken);
				if (authConfig == null)
				{
					throw new Exception($"Invalid response from {uri}");
				}
			}

			const string OidcProvider = "Horde";

			Dictionary<string, string?> values = new Dictionary<string, string?>();
			values[$"Providers:{OidcProvider}:DisplayName"] = "Horde";
			values[$"Providers:{OidcProvider}:ServerUri"] = authConfig.ServerUrl;
			values[$"Providers:{OidcProvider}:ClientId"] = authConfig.ClientId;
			values[$"Providers:{OidcProvider}:RedirectUri"] = authConfig.LocalRedirectUrls?.FirstOrDefault();

			ConfigurationBuilder builder = new ConfigurationBuilder();
			builder.AddInMemoryCollection(values);

			IConfiguration configuration = builder.Build();

			using ITokenStore tokenStore = TokenStoreFactory.CreateTokenStore();
			OidcTokenManager oidcTokenManager = OidcTokenManager.CreateTokenManager(configuration, tokenStore, new List<string>() { OidcProvider });

			OidcTokenInfo? result = null;
			try
			{
				result = await oidcTokenManager.TryGetAccessToken(OidcProvider, cancellationToken);
			}
			catch (Exception ex)
			{
				_logger.LogTrace(ex, "Unable to get access token; attempting login: {Message}", ex.Message);
			}
			if (result == null)
			{
				result = await oidcTokenManager.Login(OidcProvider, cancellationToken);
			}

			if (result.AccessToken == null)
			{
				throw new Exception($"Unable to get access token for {serverUrl}");
			}

			_logger.LogInformation("Received bearer token for {Server}", serverUrl);
			_authHeader = new AuthenticationHeaderValue("Bearer", result.AccessToken);
		}
	}
}
