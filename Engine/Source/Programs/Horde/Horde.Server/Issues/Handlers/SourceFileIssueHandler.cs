// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Text.Json;
using Horde.Server.Logs;
using Horde.Server.Utilities;

namespace Horde.Server.Issues.Handlers
{
	/// <summary>
	/// Instance of a particular compile error
	/// </summary>
	abstract class SourceFileIssueHandler : IssueHandler
	{
		/// <inheritdoc/>
		public override IReadOnlyList<string> SuspectFilter => IssueSuspectFilter.Code;

		/// <summary>
		/// Extracts a list of source files from an event
		/// </summary>
		/// <param name="logEventData">The event data</param>
		/// <param name="sourceFiles">List of source files</param>
		protected static void GetSourceFiles(ILogEventData logEventData, HashSet<IssueKey> sourceFiles)
		{
			foreach (ILogEventLine line in logEventData.Lines)
			{
				JsonElement properties;
				if (line.Data.TryGetProperty("properties", out properties) && properties.ValueKind == JsonValueKind.Object)
				{
					IssueKeyType type = IssueKeyType.File;
					if (properties.TryGetProperty("note", out JsonElement noteElement) && noteElement.GetBoolean())
					{
						type = IssueKeyType.Note;
					}

					foreach (JsonProperty property in properties.EnumerateObject())
					{
						if (property.NameEquals("file") && property.Value.ValueKind == JsonValueKind.String)
						{
							AddSourceFile(sourceFiles, property.Value.GetString()!, type);
						}
						if (property.Value.HasStringProperty("$type", "SourceFile") && property.Value.TryGetStringProperty("relativePath", out string? value))
						{
							AddSourceFile(sourceFiles, value, type);
						}
					}
				}
			}
		}

		/// <summary>
		/// Add a new source file to a list of unique source files
		/// </summary>
		/// <param name="sourceFiles">List of source files</param>
		/// <param name="relativePath">File to add</param>
		/// <param name="type">Type of key to add</param>
		static void AddSourceFile(HashSet<IssueKey> sourceFiles, string relativePath, IssueKeyType type)
		{
			int endIdx = relativePath.LastIndexOfAny(new char[] { '/', '\\' }) + 1;

			string fileName = relativePath.Substring(endIdx);
			IssueKey key = new IssueKey(fileName, type);

			sourceFiles.Add(key);
		}
	}
}
