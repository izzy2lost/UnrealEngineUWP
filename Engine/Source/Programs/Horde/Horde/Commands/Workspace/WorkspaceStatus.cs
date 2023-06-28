// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Perforce;
using EpicGames.Perforce.Managed;
using Microsoft.Extensions.Logging;

namespace Horde.Commands.Workspace
{
	[Command("workspace", "status", "Prints information about the state of the cache and workspace")]
	class WorkspaceStatus : WorkspaceBase
	{
		protected override async Task ExecuteAsync(IPerforceConnection perforce, ManagedWorkspace repo, ILogger logger)
		{
			await repo.Status();
		}
	}
}
