// Copyright Epic Games, Inc. All Rights Reserved.

using System.Diagnostics;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Clients;
using EpicGames.Horde.Storage.Nodes;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

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

		public WorkspaceSync(HttpStorageClientFactory storageClientFactory, BundleCache bundleCache, IOptions<CmdConfig> config)
			: base(storageClientFactory, bundleCache, config)
		{
		}

		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			if (File != null)
			{
				using IStorageClient store = BundleStorageClient.CreateFromDirectory(File.Directory, BundleCache, logger);
				BlobHandle handle = store.CreateBlobHandle(await FileStorageClient.ReadRefAsync(File));
				return await ExecuteInternalAsync(store, handle, logger);
			}
			else if (Ref != null)
			{
				using IStorageClient store = CreateStorageClient();
				BlobHandle handle = await store.ReadRefTargetAsync(new RefName(Ref));
				return await ExecuteInternalAsync(store, handle, logger);
			}
			else if (Node != null)
			{
				using IStorageClient store = CreateStorageClient();
				BlobHandle handle = store.CreateBlobHandle(new BlobLocator(Node));
				return await ExecuteInternalAsync(store, handle, logger);
			}
			else
			{
				throw new CommandLineArgumentException("Either -File=... or -Ref=... must be specified");
			}
		}

		async Task<int> ExecuteInternalAsync(IStorageClient store, BlobHandle handle, ILogger logger)
		{
			RootDir ??= DirectoryReference.GetCurrentDirectory();
			CancellationToken cancellationToken = CancellationToken.None;

			Workspace? workspace = await Workspace.TryOpenAsync(RootDir, logger, cancellationToken);
			if (workspace == null)
			{
				logger.LogError("No workspace has been initialized in {RootDir}. Use 'workspace init' to create a new workspace.", RootDir);
				return 1;
			}

			Stopwatch timer = Stopwatch.StartNew();
			logger.LogInformation("Syncing into layer '{LayerId}'...", LayerId);

			DirectoryNode contents = await handle.ReadNodeAsync<DirectoryNode>();
			await workspace.SyncAsync(LayerId, contents, cancellationToken);
			await workspace.SaveAsync(cancellationToken);

			logger.LogInformation("Elapsed: {Time}s", timer.Elapsed.TotalSeconds);

			if (Stats)
			{
				StorageStats stats = store.GetStats();
				stats.Print(logger);
			}

			return 0;
		}
	}
}
