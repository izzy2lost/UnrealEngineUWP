// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Clients;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Commands
{
	/// <summary>
	/// Base class for commands that require a configured storage client
	/// </summary>
	abstract class StorageCommandBase : Command
	{
		/// <summary>
		/// Namespace to use
		/// </summary>
		[CommandLine("-Namespace=", Description = "Namespace for data to manipulate")]
		public NamespaceId Namespace { get; set; } = new NamespaceId("default");

		/// <summary>
		/// Base URI to upload to
		/// </summary>
		[CommandLine("-Path=", Description = "Relative path on the server for the store to write to/from (eg. api/v1/storage/default)")]
		public string? Path { get; set; }

		/// <summary>
		/// Cache for storage
		/// </summary>
		public BundleReaderCache BundleReaderCache { get; }

		/// <summary>
		/// Configuration for the tool
		/// </summary>
		public CmdConfig Config { get; }

		readonly HordeHttpClientFactory _httpClientFactory;

		/// <summary>
		/// Constructor
		/// </summary>
		public StorageCommandBase(HordeHttpClientFactory httpClientFactory, BundleReaderCache bundleReaderCache, IOptions<CmdConfig> config)
		{
			_httpClientFactory = httpClientFactory;

			BundleReaderCache = bundleReaderCache;
			Config = config.Value;
		}

		/// <summary>
		/// Creates a new client instance
		/// </summary>
		/// <param name="logger">Logger for output messages</param>
		/// <param name="cancellationToken"></param>
		public Task<IStorageClient> CreateStorageClientAsync(ILogger logger, CancellationToken cancellationToken = default)
		{
			_ = cancellationToken;

			string? path = Path;
			if (String.IsNullOrEmpty(path))
			{
				path = $"api/v1/storage/{Namespace}/";
			}
			else if (!path.EndsWith("/", StringComparison.Ordinal))
			{
				path += "/";
			}

			HttpClient CreateClient()
			{
				HttpClient client = _httpClientFactory.CreateClient().HttpClient;
				client.BaseAddress = new Uri(client.BaseAddress!, path);
				return client;
			}

			return Task.FromResult<IStorageClient>(new HttpStorageClient(CreateClient, BundleReaderCache, logger));
		}
	}
}
