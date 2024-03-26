// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Net.Http;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Backends;
using EpicGames.Horde.Storage.Bundles;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace EpicGames.Horde
{
	/// <summary>
	/// Default implementation of <see cref="IHordeClient"/>
	/// </summary>
	class HordeClient : IHordeClient
	{
		readonly IHttpClientFactory _httpClientFactory;
		readonly HordeHttpAuthHandlerState _authHandlerState;
		readonly BundleCache _bundleCache;
		readonly HordeOptions _hordeOptions;
		readonly ILoggerFactory _loggerFactory;

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
		public async Task<bool> ConnectAsync(bool allowLogin, CancellationToken cancellationToken)
		{
			if (_authHandlerState.TryGetConfiguredAuthHeader() != null)
			{
				return true;
			}

			_authHandlerState.Invalidate();
			await _authHandlerState.RefreshAsync(allowLogin, cancellationToken);

			return IsConnected();
		}

		/// <inheritdoc/>
		public bool IsConnected()
		{
			return _authHandlerState.IsAuthenticated();
		}

		/// <inheritdoc/>
		public HordeHttpClient CreateHttpClient()
			=> _httpClientFactory.CreateHordeHttpClient();

		/// <inheritdoc/>
		public IStorageClient CreateStorageClient(string basePath)
		{
			Func<HttpClient> createClient = () => _httpClientFactory.CreateClient(HordeHttpClient.HttpClientName);
			Func<HttpClient> createUploadRedirectClient = () => _httpClientFactory.CreateClient(HordeHttpClient.UploadRedirectHttpClientName);
			HttpStorageBackend httpStorageBackend = new HttpStorageBackend(basePath, createClient, createUploadRedirectClient, _loggerFactory.CreateLogger<HttpStorageBackend>());
			return new BundleStorageClient(httpStorageBackend, _bundleCache, _hordeOptions.Bundle, _loggerFactory.CreateLogger<BundleStorageClient>());
		}
	}
}
