// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde;
using EpicGames.Horde.Api;
using Microsoft.Extensions.Logging;

namespace Horde.Commands.Bundles
{
	[Command("artifact", "find", "Finds artifacts matching the given query parameters")]
	class ArtifactFind : Command
	{
		[CommandLine("-Key=")]
		public List<string> Keys { get; } = new List<string>();

		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			using HordeHttpClient horde = await Settings.GetHttpCientAsync(logger);

			List<GetArtifactResponse> artifacts = await horde.FindArtifactsAsync(keys: Keys);
			foreach (GetArtifactResponse artifact in artifacts)
			{
				logger.LogInformation("");
				logger.LogInformation("Artifact {Id} ({Type})", artifact.Id, artifact.Type);
				foreach (string key in artifact.Keys)
				{
					logger.LogInformation("  {Key}", key);
				}
			}

			return 0;
		}
	}
}
