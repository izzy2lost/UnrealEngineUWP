// Copyright Epic Games, Inc. All Rights Reserved.

using System.Diagnostics;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Backends;
using EpicGames.Horde.Storage.Bundles;
using EpicGames.Horde.Storage.Nodes;
using Microsoft.Extensions.Logging;

namespace Horde.Commands.Workspace
{
	using Workspace = EpicGames.Horde.Storage.Workspace;

	[Command("workspace", "sync", "Extracts an archive into the workspace")]
	class WorkspaceSync : StorageCommandBase
	{
		[CommandLine("-Root=")]
		public DirectoryReference? RootDir { get; set; }

		[CommandLine("-File=")]
		public FileReference? File { get; set; }

		[CommandLine("-Ref=")]
		public string? Ref { get; set; }

		[CommandLine("-Node=")]
		public string? Node { get; set; }

		[CommandLine("-Layer=")]
		public WorkspaceLayerId LayerId { get; set; } = WorkspaceLayerId.Default;

		[CommandLine("-Stats")]
		public bool Stats { get; set; }

		public WorkspaceSync(StorageCache storageCache)
			: base(storageCache)
		{
		}

		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			CancellationToken cancellationToken = CancellationToken.None;
			RootDir ??= DirectoryReference.GetCurrentDirectory();

			Workspace? workspace = await Workspace.TryOpenAsync(RootDir, logger, cancellationToken);
			if (workspace == null)
			{
				logger.LogError("No workspace has been initialized in {RootDir}. Use 'workspace init' to create a new workspace.", RootDir);
				return 1;
			}

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
			else if (Node != null)
			{
				store = await CreateStorageClientAsync(logger);
				handle = ((BundleStorageClient)store).CreateNodeHandle(BundleNodeLocator.Parse(Node));
			}
			else
			{
				throw new CommandLineArgumentException("Either -File=... or -Ref=... must be specified");
			}

			Stopwatch timer = Stopwatch.StartNew();
			logger.LogInformation("Syncing into layer '{LayerId}'...", LayerId);

			DirectoryNode node = await handle.ReadNodeAsync<DirectoryNode>();
			await node.CopyToDirectoryAsync(workspace.RootDir.ToDirectoryInfo(), logger, CancellationToken.None);
			await workspace.SaveAsync(cancellationToken);

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
