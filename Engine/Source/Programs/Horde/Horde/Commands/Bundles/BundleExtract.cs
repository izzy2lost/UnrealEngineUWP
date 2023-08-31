// Copyright Epic Games, Inc. All Rights Reserved.

using System.Diagnostics;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Backends;
using EpicGames.Horde.Storage.Bundles;
using EpicGames.Horde.Storage.Nodes;
using Microsoft.Extensions.Logging;

namespace Horde.Commands.Bundles
{
	[Command("bundle", "extract", "Extracts data from a bundle to the local hard drive")]
	internal class BundleExtract : StorageCommandBase
	{
		[CommandLine("-File=")]
		public FileReference? File { get; set; }

		[CommandLine("-Ref=")]
		public string? Ref { get; set; }

		[CommandLine("-Stats")]
		public bool Stats { get; set; }

		[CommandLine("-OutputDir=", Required = true)]
		public DirectoryReference OutputDir { get; set; } = null!;

		public BundleExtract(StorageCache storageCache)
			: base(storageCache)
		{
		}

		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			IStorageClient store;
			BlobHandle handle;
			if (File != null)
			{
				store = new FileStorageClient(File.Directory, StorageCache, logger);
				handle = await ((FileStorageClient)store).ReadRefAsync(File);
			}
			else if (Ref != null)
			{
				store = await CreateStorageClientAsync(logger);
				handle = await store.ReadRefTargetAsync(new RefName(Ref));
			}
			else
			{
				throw new CommandLineArgumentException("Either -File=... or -Ref=... must be specified");
			}

			Stopwatch timer = Stopwatch.StartNew();

			DirectoryNode node = await handle.ReadNodeAsync<DirectoryNode>();
			await node.CopyToDirectoryAsync(OutputDir.ToDirectoryInfo(), logger, CancellationToken.None);

			logger.LogInformation("Elapsed: {Time}s", timer.Elapsed.TotalSeconds);

			if (Stats)
			{
				BundleStorageClient? bundleStorageClient = store as BundleStorageClient;
				if (bundleStorageClient != null)
				{
					logger.LogInformation("Num bytes read: {NumBytes:n0}", bundleStorageClient.BundleReader.NumBytesRead);
					logger.LogInformation("Num header reads: {NumReads:n0}", bundleStorageClient.BundleReader.NumHeaderReads);
					logger.LogInformation("Num packet reads: {NumReads:n0}", bundleStorageClient.BundleReader.NumPacketReads);
				}
			}

			return 0;
		}
	}
}
