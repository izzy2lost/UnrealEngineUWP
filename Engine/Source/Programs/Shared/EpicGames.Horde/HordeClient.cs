// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Net.Http;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Backends;
using EpicGames.Horde.Storage.Bundles;
using Microsoft.Extensions.Logging;

namespace EpicGames.Horde
{
	/// <summary>
	/// Default implementation of <see cref="IHordeClient"/>
	/// </summary>
	class HordeClient : IHordeClient
	{
		readonly IHttpClientFactory _httpClientFactory;
		readonly BundleCache _bundleCache;
		readonly ILoggerFactory _loggerFactory;

		/// <summary>
		/// Constructor
		/// </summary>
		public HordeClient(IHttpClientFactory httpClientFactory, BundleCache bundleCache, ILoggerFactory loggerFactory)
		{
			_httpClientFactory = httpClientFactory;
			_bundleCache = bundleCache;
			_loggerFactory = loggerFactory;
		}

		/// <inheritdoc/>
		public HordeHttpClient CreateHttpClient()
			=> _httpClientFactory.CreateHordeHttpClient();

		/// <inheritdoc/>
		public IStorageClient CreateStorageClient(string basePath)
		{
			Func<HttpClient> createClient = () => _httpClientFactory.CreateClient(HordeHttpClient.HttpClientName);
			HttpStorageBackend httpStorageBackend = new HttpStorageBackend(basePath, createClient, _loggerFactory.CreateLogger<HttpStorageBackend>());
			return new BundleStorageClient(httpStorageBackend, _bundleCache, _loggerFactory.CreateLogger<BundleStorageClient>());
		}
	}
}
