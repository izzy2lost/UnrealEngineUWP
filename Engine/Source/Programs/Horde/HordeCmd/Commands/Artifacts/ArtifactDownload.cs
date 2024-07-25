// Copyright Epic Games, Inc. All Rights Reserved.

using System.ComponentModel;
using System.Diagnostics;
using EpicGames.Core;
using EpicGames.Horde;
using EpicGames.Horde.Artifacts;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Nodes;
using Microsoft.Extensions.Logging;

namespace Horde.Commands.Artifacts
{
	[Command("artifact", "download", "Downloads an artifact by id")]
	class ArtifactDownload : Command
	{
		[CommandLine("-Id=")]
		[Description("Unique identifier for the artifact")]
		public ArtifactId Id { get; set; }

		[CommandLine("-OutputDir=", Required = true)]
		[Description("Directory to write extracted files.")]
		public DirectoryReference OutputDir { get; set; } = null!;

		[CommandLine("-Stats")]
		[Description("Outputs stats about the extraction process.")]
		public bool Stats { get; set; }

		[CommandLine("-CleanOutput")]
		[Description("If set, deletes the contents of the output directory before extraction.")]
		public bool CleanOutput { get; set; }

		readonly IHordeClient _hordeClient;

		public ArtifactDownload(IHordeClient hordeClient)
		{
			_hordeClient = hordeClient;
		}

		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			HordeHttpClient httpClient = _hordeClient.CreateHttpClient();

			GetArtifactResponse artifact = await httpClient.GetArtifactAsync(Id);
			logger.LogInformation("Downloading artifact {Id}: {Description}", Id, artifact.Description);

			if (CleanOutput)
			{
				logger.LogInformation("Deleting contents of {OutputDir}...", OutputDir);
				FileUtils.ForceDeleteDirectoryContents(OutputDir);
			}

			using IStorageClient store = _hordeClient.CreateStorageClient(artifact.Id);

			Stopwatch timer = Stopwatch.StartNew();

			IBlobRef<DirectoryNode> handle = await store.ReadRefAsync<DirectoryNode>(new RefName("default"));
			await handle.ExtractAsync(OutputDir.ToDirectoryInfo(), new ExtractStatsLogger(logger), logger, CancellationToken.None);

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
