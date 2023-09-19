// Copyright Epic Games, Inc. All Rights Reserved.

using System.Net.Http.Headers;
using EpicGames.Horde.Storage;
using Microsoft.Extensions.Logging;
using EpicGames.Horde.Storage.Clients;
using Microsoft.Extensions.Options;
using EpicGames.Core;

namespace Horde.Agent.Utility
{
	/// <summary>
	/// Class which creates <see cref="IStorageClient"/> instances
	/// </summary>
	class StorageClientFactory : IStorageClientFactory
	{
		readonly IOptions<AgentSettings> _settings;
		readonly StorageCache _memoryCache;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public StorageClientFactory(IOptions<AgentSettings> settings, StorageCache memoryCache, ILogger<IStorageClient> logger)
		{
			_settings = settings;
			_memoryCache = memoryCache;
			_logger = logger;
		}

		/// <inheritdoc/>
		public Task<IStorageClient> CreateClientAsync(NamespaceId namespaceId, CancellationToken cancellationToken = default)
		{
			IStorageClient client;
			if (_settings.Value.UseLocalStorageClient)
			{
				client = new FileStorageClient(DirectoryReference.Combine(AgentApp.DataDir, "Storage", namespaceId.ToString()), _memoryCache, _logger);
			}
			else
			{
				client = new HttpStorageClient(() => CreateDefaultHttpClient(namespaceId), () => new HttpClient(), _memoryCache, _logger);
			}
			return Task.FromResult(client);
		}

		HttpClient CreateDefaultHttpClient(NamespaceId namespaceId)
		{
			ServerProfile profile = _settings.Value.GetCurrentServerProfile();

			HttpClient client = new HttpClient();
			client.BaseAddress = new Uri(profile.Url, $"api/v1/storage/{namespaceId}/");
			if (!String.IsNullOrEmpty(profile.Token))
			{
				client.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Bearer", profile.Token);
			}

			return client;
		}
	}
}
