// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde;
using EpicGames.Horde.Storage;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Commands
{
	/// <summary>
	/// Base class for commands that require a configured storage client
	/// </summary>
	abstract class StorageCommandBase : Command, IDisposable
	{
		HordeHttpClient? _hordeHttpClient;

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
		public StorageCache StorageCache { get; }

		/// <summary>
		/// Configuration for the tool
		/// </summary>
		public CmdConfig Config { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public StorageCommandBase(StorageCache storageCache, IOptions<CmdConfig> config)
		{
			StorageCache = storageCache;
			Config = config.Value;
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			_hordeHttpClient?.Dispose();
		}

		/// <summary>
		/// Creates a new client instance
		/// </summary>
		/// <param name="logger">Logger for output messages</param>
		/// <param name="cancellationToken"></param>
		public async Task<IStorageClient> CreateStorageClientAsync(ILogger logger, CancellationToken cancellationToken = default)
		{
			_hordeHttpClient ??= await Config.GetHttpClientAsync(logger, cancellationToken);
			if (String.IsNullOrEmpty(Path))
			{
				return _hordeHttpClient.CreateStorageClient(Namespace, StorageCache);
			}
			else
			{
				return _hordeHttpClient.CreateStorageClient(Path, StorageCache);
			}
		}
	}
}
