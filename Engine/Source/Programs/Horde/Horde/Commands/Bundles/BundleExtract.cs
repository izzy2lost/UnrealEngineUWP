// Copyright Epic Games, Inc. All Rights Reserved.

using System.Diagnostics;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Clients;
using EpicGames.Horde.Storage.Nodes;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Commands.Bundles
{
	[Command("bundle", "extract", "Extracts data from a bundle to the local hard drive")]
	internal class BundleExtract : StorageCommandBase
	{
		[CommandLine("-File=")]
		public FileReference? File { get; set; }

		[CommandLine("-Ref=")]
		public string? Ref { get; set; }

		[CommandLine("-Node=")]
		public string? Node { get; set; }

		[CommandLine("-Stats")]
		public bool Stats { get; set; }

		[CommandLine("-OutputDir=", Required = true)]
		public DirectoryReference OutputDir { get; set; } = null!;

		public BundleExtract(HttpStorageClientFactory storageClientFactory, BundleCache bundleCache, IOptions<CmdConfig> config)
			: base(storageClientFactory, bundleCache, config)
		{
		}

		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			if (File != null)
			{
				using IStorageClient store = BundleStorageClient.CreateFromDirectory(File.Directory, BundleCache, logger);
				BlobHandle handle = store.CreateBlobHandle(await FileStorageClient.ReadRefAsync(File));
				await ExecuteInternalAsync(store, handle, logger);
			}
			else if (Ref != null)
			{
				using IStorageClient store = CreateStorageClient();
				BlobHandle handle = await store.ReadRefTargetAsync(new RefName(Ref));
				await ExecuteInternalAsync(store, handle, logger);
			}
			else if (Node != null)
			{
				using IStorageClient store = CreateStorageClient();
				BlobHandle handle = store.CreateBlobHandle(new BlobLocator(Node));
				await ExecuteInternalAsync(store, handle, logger);
			}
			else
			{
				throw new CommandLineArgumentException("Either -File=... or -Ref=... must be specified");
			}

			return 0;
		}

		protected async Task ExecuteInternalAsync(IStorageClient store, BlobHandle handle, ILogger logger)
		{
			Stopwatch timer = Stopwatch.StartNew();

			DirectoryNode node = await handle.ReadNodeAsync<DirectoryNode>();
			await node.CopyToDirectoryAsync(OutputDir.ToDirectoryInfo(), new CopyStatsLogger(logger), logger, CancellationToken.None);

			logger.LogInformation("Elapsed: {Time}s", timer.Elapsed.TotalSeconds);

			if (Stats)
			{
				StorageStats stats = store.GetStats();
				stats.Print(logger);
			}
		}
	}
}
