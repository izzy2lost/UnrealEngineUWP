// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Data;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Threading.Tasks;
using System.Xml;
using EpicGames.Core;
using EpicGames.Horde;
using EpicGames.Horde.Artifacts;
using EpicGames.Horde.Commits;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Clients;
using EpicGames.Horde.Storage.Nodes;
using EpicGames.Horde.Streams;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;

#nullable enable

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for a CreateArtifact task
	/// </summary>
	public class CreateArtifactTaskParameters
	{
		/// <summary>
		/// Name for the artifact
		/// </summary>
		[TaskParameter]
		public string Name { get; set; } = String.Empty;

		/// <summary>
		/// Type of the artifact
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Type { get; set; } = "unknown";

		/// <summary>
		/// Description for the artifact
		/// </summary>
		[TaskParameter(Optional = true)]
		public string? Description { get; set; }

		/// <summary>
		/// Base directory to resolve relative paths for input files.
		/// </summary>
		[TaskParameter]
		public string? BaseDir { get; set; }

		/// <summary>
		/// Stream for this artifact
		/// </summary>
		[TaskParameter(Optional = true)]
		public string? StreamId { get; set; }

		/// <summary>
		/// Changelist number for this artifact
		/// </summary>
		[TaskParameter(Optional = true)]
		public int? Change { get; set; }

		/// <summary>
		/// Files to be uploaded.
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.FileSpec)]
		public string Files { get; set; } = "...";
	}

	/// <summary>
	/// Deploys a tool update through Horde
	/// </summary>
	[TaskElement("CreateArtifact", typeof(CreateArtifactTaskParameters))]
	public class CreateArtifactTask : SpawnTaskBase
	{
		class LoggerProviderAdapter : ILoggerProvider
		{
			readonly ILogger _logger;

			public LoggerProviderAdapter(ILogger logger) => _logger = logger;
			public ILogger CreateLogger(string categoryName) => _logger;
			public void Dispose() { }
		}

		readonly CreateArtifactTaskParameters _parameters;

		/// <summary>
		/// Construct a Helm task
		/// </summary>
		/// <param name="parameters">Parameters for the task</param>
		public CreateArtifactTask(CreateArtifactTaskParameters parameters)
		{
			_parameters = parameters;
		}

		/// <summary>
		/// ExecuteAsync the task.
		/// </summary>
		/// <param name="job">Information about the current job</param>
		/// <param name="buildProducts">Set of build products produced by this node.</param>
		/// <param name="tagNameToFileSet">Mapping from tag names to the set of files they include</param>
		public override async Task ExecuteAsync(JobContext job, HashSet<FileReference> buildProducts, Dictionary<string, HashSet<FileReference>> tagNameToFileSet)
		{
			// Create a DI container for building the graph
			ServiceCollection serviceCollection = new ServiceCollection();
			serviceCollection.AddHorde();
			serviceCollection.AddLogging(builder => builder.AddProvider(new LoggerProviderAdapter(Log.Logger)));
			serviceCollection.Configure<HordeOptions>(x => x.AllowAuthPrompt = !Automation.IsBuildMachine);
			serviceCollection.Configure<LoggerFilterOptions>(options => options.AddFilter(typeof(HttpClient).FullName, LogLevel.Warning));

			await using ServiceProvider serviceProvider = serviceCollection.BuildServiceProvider();

			ArtifactName artifactName = new ArtifactName(_parameters.Name);
			ArtifactType artifactType = new ArtifactType(_parameters.Type);

			HordeHttpClient hordeHttpClient = serviceProvider.GetRequiredService<HordeHttpClient>();
			StreamId streamId = new StreamId(_parameters.StreamId ?? throw new InvalidOperationException("Missing StreamId parameter")); 
			CommitId commitId = CommitId.FromPerforceChange(_parameters.Change ?? CommandUtils.P4Env.Changelist);
			CreateArtifactResponse response = await hordeHttpClient.CreateArtifactAsync(artifactName, artifactType, _parameters.Description, streamId: streamId, commitId: commitId);
			Logger.LogInformation("Creating artifact {ArtifactId} '{ArtifactName}' ({ArtifactType}) (ns: {NamespaceId}, ref: {RefName})", response.ArtifactId, artifactName, artifactType, response.NamespaceId, response.RefName);

			Stopwatch timer = Stopwatch.StartNew();

			HttpStorageClientFactory httpStorageClientFactory = serviceProvider.GetRequiredService<HttpStorageClientFactory>();
			using (IStorageClient client = httpStorageClientFactory.CreateClient(response.NamespaceId, response.Token))
			{
				await using (IBlobWriter writer = client.CreateBlobWriter(response.RefName))
				{
					DirectoryReference baseDir = ResolveDirectory(_parameters.BaseDir);
					List<FileInfo> files = ResolveFilespec(baseDir, _parameters.Files, tagNameToFileSet).Select(x => x.ToFileInfo()).ToList();

					int totalCount = files.Count;
					long totalSize = files.Sum(x => x.Length);

					IHashedBlobRef<DirectoryNode> outputNodeRef = await writer.WriteFilesAsync(baseDir.ToDirectoryInfo(), files, progress: new UpdateStatsLogger(totalCount, totalSize, Logger));
					await writer.FlushAsync();

					await client.WriteRefAsync(response.RefName, outputNodeRef);
				}
			}

			Logger.LogInformation("Completed in {Time:n1}s", timer.Elapsed.TotalSeconds);
		}

		/// <summary>
		/// Output this task out to an XML writer.
		/// </summary>
		public override void Write(XmlWriter writer)
		{
			Write(writer, _parameters);
		}

		/// <summary>
		/// Find all the tags which are used as inputs to this task
		/// </summary>
		/// <returns>The tag names which are read by this task</returns>
		public override IEnumerable<string> FindConsumedTagNames()
		{
			return FindTagNamesFromList(_parameters.Files);
		}

		/// <summary>
		/// Find all the tags which are modified by this task
		/// </summary>
		/// <returns>The tag names which are modified by this task</returns>
		public override IEnumerable<string> FindProducedTagNames()
		{
			yield break;
		}
	}
}
