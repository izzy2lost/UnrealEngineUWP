// Copyright Epic Games, Inc. All Rights Reserved.

using System.ComponentModel;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Bundles;
using EpicGames.Horde.Storage.Clients;

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
		[CommandLine("-Namespace=")]
		[Description("Namespace for data to manipulate")]
		public NamespaceId Namespace { get; set; } = new NamespaceId("default");

		/// <summary>
		/// Base URI to upload to
		/// </summary>
		[CommandLine("-Path=")]
		[Description("Relative path on the server for the store to write to/from (eg. api/v1/storage/default)")]
		public string? Path { get; set; }

		/// <summary>
		/// Cache for storage
		/// </summary>
		public BundleCache BundleCache { get; }

		readonly HttpStorageClientFactory _storageClientFactory;

		/// <summary>
		/// Constructor
		/// </summary>
		public StorageCommandBase(HttpStorageClientFactory storageClientFactory, BundleCache bundleCache)
		{
			_storageClientFactory = storageClientFactory;

			BundleCache = bundleCache;
		}

		/// <summary>
		/// Creates a new client instance
		/// </summary>
		public IStorageClient CreateStorageClient()
		{
			if (String.IsNullOrEmpty(Path))
			{
				return _storageClientFactory.CreateClient(Namespace);
			}
			else
			{
				return _storageClientFactory.CreateClientWithPath(Path);
			}
		}
	}
}
