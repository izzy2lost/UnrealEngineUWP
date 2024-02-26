// Copyright Epic Games, Inc. All Rights Reserved.

using System.ComponentModel;
using EpicGames.Core;
using EpicGames.Horde.Tools;
using Horde.Agent.Utility;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Commands.Utilities
{
	using JsonObject = System.Text.Json.Nodes.JsonObject;

	/// <summary>
	/// Sets the tool id to use for agent updates
	/// </summary>
	[Command("setupdatechannel", "Configures the update channel used for this build")]
	class SetUpdateChannelCommand : Command
	{
		[CommandLine("-Id=", Required = true)]
		[Description("Channel to use for tool updates")]
		public ToolId Id { get; set; }

		[CommandLine("-RootDir=")]
		[Description("Root directory for the installation to update")]
		public DirectoryReference? RootDir { get; set; }

		/// <inheritdoc/>
		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			// Update the agent to recognize the server certificate, and give it a custom token for being able to connect
			FileReference agentConfigFile = FileReference.Combine(RootDir ?? AgentApp.AppDir, "appsettings.json");
			JsonObject agentConfig = await JsonConfig.ReadAsync(agentConfigFile);

			JsonObject hordeConfig = JsonConfig.FindOrAddNode(agentConfig, "Horde", () => new JsonObject());
			hordeConfig[nameof(AgentSettings.UpdateChannel)] = Id.ToString();

			await JsonConfig.WriteAsync(agentConfigFile, agentConfig);
			return 0;
		}
	}
}
