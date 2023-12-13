// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.ComponentModel;
using System.Diagnostics;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Clients;
using EpicGames.Horde.Storage.Nodes;
using Horde.Server.Agents;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Commands.Install
{
	using JsonObject = System.Text.Json.Nodes.JsonObject;

	/// <summary>
	/// Performs custom actions required by an MSI installation on Windows; creates a unique certificate, configures the agent to use it, and packages the agent into a bundle.
	/// </summary>
	[Command("setup", "Runs post-install setup actions to configure the agent bundle, etc...", Advertise = false)]
	public class SetupCommand : Command
	{
		[CommandLine("-Url=")]
		[Description("Sets the default server URL")]
		string ServerUrl { get; set; } = "http://localhost:5000";

		[CommandLine("-BaseDir")]
		[Description("Directory containing the server installation to configure.")]
		DirectoryReference ServerDir { get; set; } = ServerApp.AppDir;

		/// <inheritdoc/>
		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			// Update the agent to recognize the server certificate, and give it a custom token for being able to connect
			DirectoryReference looseAgentDir = DirectoryReference.Combine(ServerDir, "Tools", "horde-agent-loose");
			FileReference agentConfigFile = FileReference.Combine(looseAgentDir, "appsettings.json");
			{
				JsonObject agentConfig = await ReadConfigAsync(agentConfigFile);

				JsonObject hordeConfig = FindOrAddNode(agentConfig, "Horde", () => new JsonObject());
				hordeConfig["Server"] = "Default";

				JsonArray serverProfiles = FindOrAddNode(hordeConfig, "ServerProfiles", () => new JsonArray());

				JsonObject serverProfile = FindOrAddElementByKey(serverProfiles, "Name", "Default");
				serverProfile["Environment"] = "Prod";
				serverProfile["Url"] = ServerUrl;

				await SaveConfigAsync(agentConfigFile, agentConfig);
			}

			// Create the local agent bundle
			DirectoryReference bundleDir = DirectoryReference.Combine(ServerDir, "Tools", "horde-agent");
			DirectoryReference.CreateDirectory(bundleDir);

			RefName refName = new RefName("latest");

			await using (BundleCache bundleCache = new BundleCache())
			{
				using (IStorageClient client = BundleStorageClient.CreateFromDirectory(bundleDir, bundleCache, logger))
				{
					NodeRef<DirectoryNode> dirNodeRef;
					await using (IStorageWriter writer = client.CreateWriter(refName))
					{
						DirectoryNode dirNode = new DirectoryNode();
						await dirNode.AddFilesAsync(looseAgentDir.ToDirectoryInfo(), writer);
						dirNodeRef = await writer.WriteNodeAsync(dirNode);
					}
					await client.WriteRefAsync(refName, dirNodeRef.Handle);
				}
			}

			// Create the agent installer bundle
			DirectoryReference looseAgentInstallerDir = DirectoryReference.Combine(ServerDir, "Tools", "horde-agent-installer-loose");
			DirectoryReference installerBundleDir = DirectoryReference.Combine(ServerDir, "Tools", "horde-agent-installer");
			DirectoryReference.CreateDirectory(installerBundleDir);
			await using (BundleCache bundleCache = new BundleCache())
			{
				using (IStorageClient client = BundleStorageClient.CreateFromDirectory(installerBundleDir, bundleCache, logger))
				{
					NodeRef<DirectoryNode> dirNodeRef;
					await using (IStorageWriter writer = client.CreateWriter(refName))
					{
						DirectoryNode dirNode = new DirectoryNode();
						await dirNode.AddFilesAsync(looseAgentInstallerDir.ToDirectoryInfo(), writer);
						dirNodeRef = await writer.WriteNodeAsync(dirNode);
					}
					await client.WriteRefTargetAsync(refName, dirNodeRef);
				}
			}

			FileVersionInfo versionInfo = FileVersionInfo.GetVersionInfo(FileReference.Combine(looseAgentDir, "hordeagent.exe").FullName);

			// Update the server config to include the bundled tool
			FileReference serverConfigFile = FileReference.Combine(ServerDir, "appsettings.json");
			{
				JsonObject serverConfig = await ReadConfigAsync(serverConfigFile);

				JsonObject hordeConfig = FindOrAddNode(serverConfig, "Horde", () => new JsonObject());
				JsonArray bundledTools = FindOrAddNode(hordeConfig, nameof(ServerSettings.BundledTools), () => new JsonArray());

				if (ServerUrl.Contains(':', StringComparison.OrdinalIgnoreCase))
				{
					Uri uri = new Uri(ServerUrl);
					hordeConfig["HttpPort"] = uri.Port;					
				}

				JsonObject bundledTool = FindOrAddElementByKey(bundledTools, nameof(BundledToolConfig.Id), AgentExtensions.DefaultAgentSoftwareToolId.ToString());
				bundledTool[nameof(BundledToolConfig.Name)] = "Horde Agent";
				bundledTool[nameof(BundledToolConfig.Description)] = "Cross-platform build of the Horde Agent";
				bundledTool[nameof(BundledToolConfig.RefName)] = refName.ToString();
				if (!String.IsNullOrEmpty(versionInfo.ProductVersion))
				{
					bundledTool[nameof(BundledToolConfig.Version)] = versionInfo.ProductVersion;
				}

				bundledTool = FindOrAddElementByKey(bundledTools, nameof(BundledToolConfig.Id), "horde-agent-installer");
				bundledTool[nameof(BundledToolConfig.Name)] = "Horde Agent Installer (Windows)";
				bundledTool[nameof(BundledToolConfig.Description)] = "MSI installer for the Horde Agent on Windows";
				bundledTool[nameof(BundledToolConfig.RefName)] = refName.ToString();

				if (!String.IsNullOrEmpty(versionInfo.ProductVersion))
				{
					bundledTool[nameof(BundledToolConfig.Version)] = versionInfo.ProductVersion;
				}

				await SaveConfigAsync(serverConfigFile, serverConfig);
			}

			return 0;
		}

		static T FindOrAddNode<T>(JsonObject obj, string name, Func<T> factory) where T : JsonNode
		{
			JsonNode? node = obj[name];
			if (node != null)
			{
				if (node is T existingTypedNode)
				{
					return existingTypedNode;
				}
				else
				{
					obj.Remove(name);
				}
			}

			T newTypedNode = factory();
			obj.Add(name, newTypedNode);
			return newTypedNode;
		}

		static JsonObject FindOrAddElementByKey(JsonArray array, string key, string name)
		{
			foreach (JsonNode? element in array)
			{
				if (element is JsonObject obj)
				{
					JsonNode? node = obj[key];
					if (node != null && (string?)node.AsValue() == name)
					{
						return obj;
					}
				}
			}

			JsonObject newObj = new JsonObject();
			newObj[key] = name;
			array.Add(newObj);
			return newObj;
		}

		static async Task<JsonObject> ReadConfigAsync(FileReference file)
		{
			byte[] data = await FileReference.ReadAllBytesAsync(file);
			JsonObject? obj = JsonNode.Parse(data, new JsonNodeOptions { PropertyNameCaseInsensitive = true }, new JsonDocumentOptions { AllowTrailingCommas = true, CommentHandling = JsonCommentHandling.Skip }) as JsonObject;
			return obj ?? new JsonObject();
		}

		static async Task SaveConfigAsync(FileReference file, JsonNode node)
		{
			ArrayBufferWriter<byte> buffer = new ArrayBufferWriter<byte>();
			using (Utf8JsonWriter writer = new Utf8JsonWriter(buffer, new JsonWriterOptions { Indented = true }))
			{
				node.WriteTo(writer);
			}
			await FileReference.WriteAllBytesAsync(file, buffer.WrittenMemory.ToArray());
		}
	}
}
