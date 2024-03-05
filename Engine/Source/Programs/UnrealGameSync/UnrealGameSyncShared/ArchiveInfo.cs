// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde;
using EpicGames.Horde.Artifacts;
using EpicGames.Horde.Streams;
using EpicGames.Perforce;
using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace UnrealGameSync
{
	public interface IArchiveInfo
	{
		public const string EditorArchiveType = "Editor";

		string Name { get; }
		string Type { get; }
		string BasePath { get; }
		string? Target { get; }

		bool Exists();
		bool TryGetArchiveKeyForChangeNumber(int changeNumber, int maxChangeNumber, [NotNullWhen(true)] out string? archiveKey);
		Task<bool> DownloadArchive(IPerforceConnection perforce, string archiveKey, DirectoryReference localRootPath, FileReference manifestFileName, ILogger logger, ProgressValue progress, CancellationToken cancellationToken);
	}

	public abstract class BaseArchiveInfo : IArchiveInfo
	{
		public string Name { get; }
		public string Type { get; }
		public string BasePath { get; set; }
		public string? Target { get; }

		public abstract Task<bool> DownloadArchive(IPerforceConnection perforce, string archiveKey, DirectoryReference localRootPath, FileReference manifestFileName, ILogger logger, ProgressValue progress, CancellationToken cancellationToken);

		// Abstract BaseArchiveInfo Helper Functions
		public abstract Task FindArtifacts(IPerforceConnection perforce, CancellationToken cancellationToken);

		// TODO: executable/configuration?
		public SortedList<int, string> ChangeNumberToArchiveKey { get; } = new SortedList<int, string>();

		protected BaseArchiveInfo(string name, string type, string basePath, string? target)
		{
			Name = name;
			Type = type;
			BasePath = basePath;
			Target = target;
		}
		
		public override bool Equals(object? other)
		{
			BaseArchiveInfo? otherArchive = other as BaseArchiveInfo;
			return otherArchive != null && Name == otherArchive.Name && Type == otherArchive.Type && BasePath == otherArchive.BasePath && Target == otherArchive.Target && Enumerable.SequenceEqual(ChangeNumberToArchiveKey, otherArchive.ChangeNumberToArchiveKey);
		}

		public override int GetHashCode()
		{
			throw new NotSupportedException();
		}

		public bool Exists()
		{
			return ChangeNumberToArchiveKey.Count > 0;
		}

		public static bool TryParseConfigEntry(IHordeClient hordeClient, string text, [NotNullWhen(true)] out BaseArchiveInfo? info)
		{
			ConfigObject obj = new ConfigObject(text);

			string? name = obj.GetValue("Name", null);
			if (name == null)
			{
				info = null;
				return false;
			}

			// Where to find archives, you'll have either Perforce (DepotPath) or Horde (ArchiveType)
			string? depotPath = obj.GetValue("DepotPath", null);
			string? archiveType = obj.GetValue("ArchiveType", null);

			// We only want one of the other, not both or none
			if (((depotPath == null) && (archiveType == null)) || ((depotPath != null) && (archiveType != null)))
			{
				info = null;
				return false;
			}

			string? target = obj.GetValue("Target", null);

			string type = obj.GetValue("Type", null) ?? name;

			string? streamName = obj.GetValue("StreamName", null);

			if (depotPath != null)
			{
				info = new PerforceArchiveInfo(name, type, depotPath, target);
			}
			else if ((archiveType != null) && (streamName != null))
			{
				info = new HordeArchiveInfo(hordeClient, name, type, archiveType, target, streamName);
			}
			else
			{
				info = null;
				return false;
			}
			return true;
		}

		public bool TryGetArchiveKeyForChangeNumber(int changeNumber, int maxChangeNumber, [NotNullWhen(true)] out string? archiveKey)
		{
			int idx = ChangeNumberToArchiveKey.Keys.AsReadOnlyList().BinarySearch(changeNumber);
			if (idx >= 0)
			{
				archiveKey = ChangeNumberToArchiveKey.Values[idx];
				return true;
			}

			int nextIdx = ~idx;
			if (nextIdx < ChangeNumberToArchiveKey.Count && ChangeNumberToArchiveKey.Keys[nextIdx] <= maxChangeNumber)
			{
				archiveKey = ChangeNumberToArchiveKey.Values[nextIdx];
				return true;
			}

			archiveKey = null;
			return false;
		}

		public override string ToString()
		{
			return Name;
		}
	}

	public class PerforceArchiveInfo : BaseArchiveInfo
	{
		public string DepotPath
		{
			get => BasePath;
			set => BasePath = value;
		}

		public PerforceArchiveInfo(string name, string type, string depotPath, string? target)
			: base(name, type, depotPath, target)
		{
		}

		public override bool Equals(object? other)
		{
			PerforceArchiveInfo? otherArchive = other as PerforceArchiveInfo;
			return otherArchive != null && Name == otherArchive.Name && Type == otherArchive.Type && DepotPath == otherArchive.DepotPath && Target == otherArchive.Target && Enumerable.SequenceEqual(ChangeNumberToArchiveKey, otherArchive.ChangeNumberToArchiveKey);
		}

		public override int GetHashCode()
		{
			throw new NotSupportedException();
		}

		public override async Task<bool> DownloadArchive(IPerforceConnection perforce, string archiveKey, DirectoryReference localRootPath, FileReference manifestFileName, ILogger logger, ProgressValue progress, CancellationToken cancellationToken)
		{
			DirectoryReference configDir = UserSettings.GetConfigDir(localRootPath);
			UserSettings.CreateConfigDir(configDir);

			FileReference tempZipFileName = FileReference.Combine(configDir, "archive.zip");
			try
			{
				PrintRecord record = await perforce.PrintAsync(tempZipFileName.FullName, archiveKey, cancellationToken);

				if (tempZipFileName.ToFileInfo().Length == 0)
				{
					return false;
				}
				ArchiveUtils.ExtractFiles(tempZipFileName, localRootPath, manifestFileName, progress, logger);
			}
			finally
			{
				FileReference.SetAttributes(tempZipFileName, FileAttributes.Normal);
				FileReference.Delete(tempZipFileName);
			}

			return true;
		}

		public override async Task FindArtifacts(IPerforceConnection perforce, CancellationToken cancellationToken)
		{
			PerforceResponseList<FileLogRecord> response = await perforce.TryFileLogAsync(128, FileLogOptions.FullDescriptions, DepotPath, cancellationToken);
			if (response.Succeeded)
			{
				// Build a new list of zipped binaries
				foreach (FileLogRecord file in response.Data)
				{
					foreach (RevisionRecord revision in file.Revisions)
					{
						if (revision.Action != FileAction.Purge)
						{
							string[] tokens = revision.Description.Split(' ');
							if (tokens[0].StartsWith("[CL", StringComparison.Ordinal) && tokens[1].EndsWith("]", StringComparison.Ordinal))
							{
								int originalChangeNumber;
								if (Int32.TryParse(tokens[1].Substring(0, tokens[1].Length - 1), out originalChangeNumber) && !ChangeNumberToArchiveKey.ContainsKey(originalChangeNumber))
								{
									ChangeNumberToArchiveKey[originalChangeNumber] = $"{DepotPath}#{revision.RevisionNumber}";
								}
							}
						}
					}
				}
			}
		}
	}

	public class HordeArchiveInfo : BaseArchiveInfo
	{
		public string ArchiveType
		{
			get => BasePath;
			set => BasePath = value;
		}

		public string StreamName { get; set; }

		private readonly IHordeClient _hordeClient;

		public HordeArchiveInfo(IHordeClient hordeClient, string name, string type, string archiveType, string? target, string streamName)
			: base(name, type, archiveType, target)
		{
			StreamName = streamName;
			_hordeClient = hordeClient;
		}
		public override bool Equals(object? other)
		{
			HordeArchiveInfo? otherArchive = other as HordeArchiveInfo;
			return otherArchive != null && Name == otherArchive.Name && Type == otherArchive.Type && ArchiveType == otherArchive.ArchiveType && Target == otherArchive.Target && StreamName == otherArchive.StreamName && Enumerable.SequenceEqual(ChangeNumberToArchiveKey, otherArchive.ChangeNumberToArchiveKey);
		}

		public override int GetHashCode()
		{
			throw new NotSupportedException();
		}

		public override async Task<bool> DownloadArchive(IPerforceConnection perforce, string archiveKey, DirectoryReference localRootPath, FileReference manifestFileName, ILogger logger, ProgressValue progress, CancellationToken cancellationToken)
		{
			DirectoryReference configDir = UserSettings.GetConfigDir(localRootPath);
			UserSettings.CreateConfigDir(configDir);

			FileReference tempZipFileName = FileReference.Combine(configDir, "archive.zip");
			try
			{
				HordeHttpClient hordeHttpClient = _hordeClient.CreateHttpClient();

				ArtifactId artifactId = ArtifactId.Parse(archiveKey);

				using (FileStream stream = FileReference.Open(tempZipFileName, FileMode.Create, FileAccess.Write, FileShare.None))
				{
					await using Stream sourceStream = await hordeHttpClient.GetArtifactZipAsync(artifactId, cancellationToken);
					await sourceStream.CopyToAsync(stream, cancellationToken);
				}

				if (tempZipFileName.ToFileInfo().Length == 0)
				{
					return false;
				}
				ArchiveUtils.ExtractFiles(tempZipFileName, localRootPath, manifestFileName, progress, logger);
			}
			finally
			{
				FileReference.SetAttributes(tempZipFileName, FileAttributes.Normal);
				FileReference.Delete(tempZipFileName);
			}

			return true;
		}

		public override async Task FindArtifacts(IPerforceConnection perforce, CancellationToken cancellationToken)
		{
			try
			{
				HordeHttpClient hordeHttpClient = _hordeClient.CreateHttpClient();

				ArtifactType artifactType = new ArtifactType(ArchiveType);
				StreamId streamId = new StreamId(StreamName);
				int? minChange = null;
				int? maxChange = null;
				List<GetArtifactResponse> artifactResponse = await hordeHttpClient.FindArtifactsByTypeAsync(artifactType, streamId, minChange, maxChange, cancellationToken);

				foreach (GetArtifactResponse response in artifactResponse)
				{
					ChangeNumberToArchiveKey[response.Change] = response.Id.ToString();
				}
			}
			catch (Exception)
			{
				return;
			}
		}
	}

	public static class BaseArchive
	{
		public static async Task<List<BaseArchiveInfo>> EnumerateAsync(IPerforceConnection perforce, IHordeClient hordeClient, ConfigFile latestProjectConfigFile, string projectIdentifier, CancellationToken cancellationToken)
		{
			List<BaseArchiveInfo> newArchives = new List<BaseArchiveInfo>();

			// Find all the zipped binaries under this stream
			ConfigSection? projectConfigSection = latestProjectConfigFile.FindSection(projectIdentifier);
			if (projectConfigSection != null)
			{
				// Legacy
				string? legacyEditorArchivePath = projectConfigSection.GetValue("ZippedBinariesPath", null);
				if (legacyEditorArchivePath != null)
				{
					// Only Perforce uses the legacy method
					newArchives.Add(new PerforceArchiveInfo("Editor", "Editor", legacyEditorArchivePath, null));
				}

				// New style
				foreach (string archiveValue in projectConfigSection.GetValues("Archives", Array.Empty<string>()))
				{
					BaseArchiveInfo? archive;
					if (BaseArchiveInfo.TryParseConfigEntry(hordeClient, archiveValue, out archive))
					{
						newArchives.Add(archive!);
					}
				}

				// Make sure the zipped binaries path exists
				foreach (BaseArchiveInfo newArchive in newArchives)
				{
					await newArchive.FindArtifacts(perforce, cancellationToken);
				}
			}

			return newArchives;
		}
	}
}
