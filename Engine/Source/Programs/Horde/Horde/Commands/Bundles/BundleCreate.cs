// Copyright Epic Games, Inc. All Rights Reserved.

using System.ComponentModel;
using System.Diagnostics;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Clients;
using EpicGames.Horde.Storage.Nodes;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Commands.Bundles
{
	[Command("bundle", "create", "Creates a bundle from a folder on the local hard drive")]
	class BundleCreate : StorageCommandBase
	{
		[CommandLine("-File=")]
		[Description("Output file for the bundle ref. Either -File=.. or -Ref=.. must be set.")]
		public FileReference? File { get; set; }

		[CommandLine("-Ref=")]
		[Description("Output ref for the bundled data. Either -File=.. or -Ref=.. must be set.")]
		public string? Ref { get; set; }

		[CommandLine("-Input=", Required = true)]
		[Description("Input file or directory")]
		public string Input { get; set; } = null!;

		[CommandLine("-Filter=")]
		[Description("Filter for files to include, in P4 syntax (eg. Foo/...).")]
		public string Filter { get; set; } = "...";

		public BundleCreate(HttpStorageClientFactory storageClientFactory, BundleCache bundleCache, IOptions<CmdConfig> config)
			: base(storageClientFactory, bundleCache, config)
		{
		}

		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			if (File != null)
			{
				using IStorageClient store = BundleStorageClient.CreateFromDirectory(File.Directory, BundleCache, logger);
				return await ExecuteInternalAsync(store, logger);
			}
			else if (!String.IsNullOrEmpty(Ref))
			{
				using IStorageClient store = CreateStorageClient();
				return await ExecuteInternalAsync(store, logger);
			}
			else
			{
				throw new CommandLineArgumentException("Either -File=... or -Ref=... must be specified.");
			}
		}

		async Task<int> ExecuteInternalAsync(IStorageClient store, ILogger logger)
		{
			// Gather the input files
			DirectoryReference baseDir;
			List<FileReference> files = new List<FileReference>();

			if (System.IO.File.Exists(Input))
			{
				FileReference file = new FileReference(Input);
				baseDir = file.Directory;
				files.Add(file);
			}
			else if (Directory.Exists(Input))
			{
				baseDir = new DirectoryReference(Input);
				FileFilter filter = new FileFilter(Filter.Split(';'));
				files.AddRange(filter.ApplyToDirectory(baseDir, true));
			}
			else
			{
				logger.LogError("{Path} does not exist", Input);
				return 1;
			}

			// Create the bundle
			Stopwatch timer = Stopwatch.StartNew();

			await using (IStorageWriter writer = store.CreateWriter())
			{
				ChunkingOptions options = new ChunkingOptions();

				List<FileInfo> fileInfos = files.ConvertAll(x => x.ToFileInfo());
				CopyStatsLoggerWithTotals copyStatsLogger = new CopyStatsLoggerWithTotals(files.Count, fileInfos.Sum(x => x.Length), logger);

				DirectoryNode node = await DirectoryNode.CreateAsync(baseDir, files.ConvertAll(x => x.ToFileInfo()), options, writer, copyStatsLogger, CancellationToken.None);
				HashedNodeRef<DirectoryNode> nodeRef = await writer.WriteHashedNodeAsync(node, CancellationToken.None);

				await writer.FlushAsync();

				if (File != null)
				{
					logger.LogInformation("Writing {File}", File);
					await FileReference.WriteAllTextAsync(File, nodeRef.Handle.GetLocator().ToString()!);
				}
				else
				{
					logger.LogInformation("Writing ref {Ref}", Ref);
					await store.WriteRefTargetAsync(new RefName(Ref!), nodeRef.Handle);
				}
			}

			logger.LogInformation("Time: {Time}", timer.Elapsed.TotalSeconds);
			return 0;
		}
	}
}
