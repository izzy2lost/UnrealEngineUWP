// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Xml;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Clients;
using EpicGames.Horde.Storage.Nodes;
using Horde.Server.Acls;
using Horde.Server.Projects;
using Horde.Server.Server;
using Horde.Server.Streams;
using Horde.Server.Utilities;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Commands.Generate
{
	using JsonObject = System.Text.Json.Nodes.JsonObject;

	[Command("generate", "tooldata", "Creates bundled tool data, and registers it with the server config file")]
	class ToolDataCommand : Command
	{
		[CommandLine(Required = true)]
		[Description("Identifier for the tool")]
		public string Id { get; set; } = String.Empty;

		[CommandLine]
		[Description("Name of the tool")]
		public string? Name { get; set; }

		[CommandLine]
		[Description("Description for the tool")]
		public string? Description { get; set; }

		[CommandLine]
		[Description("Version string for the tool")]
		public string? Version { get; set; }

		[CommandLine(Required = true)]
		[Description("Source directory for tool data")]
		public DirectoryReference InputDir { get; set; } = null!;

		[CommandLine]
		[Description("Directory containing the server to modify")]
		public DirectoryReference ServerDir { get; set; } = ServerApp.AppDir;

		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			// Create the local agent bundle
			DirectoryReference bundleDir = DirectoryReference.Combine(ServerDir, "Tools", Id);
			DirectoryReference.CreateDirectory(bundleDir);

			RefName refName = new RefName("latest");
			await using (BundleCache bundleCache = new BundleCache())
			{
				using (IStorageClient client = BundleStorageClient.CreateFromDirectory(bundleDir, bundleCache, logger))
				{
					NodeRef<DirectoryNode> dirNodeRef;
					await using (IStorageWriter writer = client.CreateWriter(refName).WithDedupe())
					{
						DirectoryNode dirNode = new DirectoryNode();
						await dirNode.AddFilesAsync(InputDir.ToDirectoryInfo(), writer);
						dirNodeRef = await writer.WriteNodeAsync(dirNode);
					}
					await client.WriteRefAsync(refName, dirNodeRef.Handle);
				}
			}

			// Update the server config to include the bundled tool
			FileReference serverConfigFile = FileReference.Combine(ServerDir, "appsettings.json");
			{
				JsonConfigFile serverConfig = await JsonConfigFile.ReadAsync(serverConfigFile);

				JsonObject hordeConfig = JsonConfigFile.FindOrAddNode(serverConfig.Root, "Horde", () => new JsonObject());
				JsonArray bundledTools = JsonConfigFile.FindOrAddNode(hordeConfig, nameof(ServerSettings.BundledTools), () => new JsonArray());

				JsonObject bundledTool = JsonConfigFile.FindOrAddElementByKey(bundledTools, nameof(BundledToolConfig.Id), Id);
				bundledTool[nameof(BundledToolConfig.Name)] = Name ?? Id.ToString();
				if (!String.IsNullOrEmpty(Description))
				{
					bundledTool[nameof(BundledToolConfig.Description)] = Description;
				}
				if (!String.IsNullOrEmpty(Version))
				{
					bundledTool[nameof(BundledToolConfig.Version)] = Version;
				}

				bundledTool[nameof(BundledToolConfig.RefName)] = refName.ToString();

				logger.LogInformation("Updating {File}", serverConfigFile);
				await serverConfig.WriteAsync(serverConfigFile);
			}

			return 0;
		}
	}
}
