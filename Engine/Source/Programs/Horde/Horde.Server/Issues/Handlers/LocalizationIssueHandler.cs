// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using EpicGames.Core;
using Horde.Server.Jobs;
using Horde.Server.Jobs.Graphs;
using Horde.Server.Logs;
using Horde.Server.Utilities;
using Microsoft.Extensions.Logging;
using MongoDB.Driver;

namespace Horde.Server.Issues.Handlers
{
	/// <summary>
	/// Instance of a localization error
	/// </summary>
	[IssueHandler(Priority = 10)]
	class LocalizationIssueHandler : IssueHandler
	{
		/// <inheritdoc/>
		public override string Type => "Localization";

		/// <inheritdoc/>
		public override string SummaryTemplate => "Localization {Severity} in {Files}";

		/// <summary>
		/// Determines if the given event id matches
		/// </summary>
		/// <param name="eventId">The event id to compare</param>
		/// <returns>True if the given event id matches</returns>
		public static bool IsMatchingEventId(EventId? eventId)
		{
			return eventId == KnownLogEvents.Engine_Localization;
		}

		/// <summary>
		/// Determines if an event should be masked by this 
		/// </summary>
		/// <param name="eventId"></param>
		/// <returns></returns>
		static bool IsMaskedEventId(EventId eventId)
		{
			return eventId == KnownLogEvents.ExitCode;
		}

		/// <summary>
		/// Extracts a list of source files from an event
		/// </summary>
		/// <param name="logEventData">The event data</param>
		/// <param name="sourceFiles">List of source files</param>
		public static void GetSourceFiles(ILogEventData logEventData, HashSet<IssueKey> sourceFiles)
		{
			foreach (ILogEventLine line in logEventData.Lines)
			{
				string? relativePath;
				if (line.Data.TryGetNestedProperty("properties.file.relativePath", out relativePath) || line.Data.TryGetNestedProperty("properties.file", out relativePath))
				{
					if (!relativePath.EndsWith(".manifest", StringComparison.OrdinalIgnoreCase))
					{
						int endIdx = relativePath.LastIndexOfAny(new char[] { '/', '\\' }) + 1;
						string fileName = relativePath.Substring(endIdx);
						sourceFiles.Add(new IssueKey(fileName, IssueKeyType.File));
					}
				}
			}
		}

		/// <inheritdoc/>
		public override void TagEvents(IJob job, INode node, IReadOnlyNodeAnnotations annotations, IReadOnlyList<IssueEvent> stepEvents)
		{
			bool hasMatches = false;
			foreach (IssueEvent stepEvent in stepEvents)
			{
				if (stepEvent.EventId != null)
				{
					if (IsMatchingEventId(stepEvent.EventId.Value))
					{
						HashSet<IssueKey> newFileNames = new HashSet<IssueKey>();
						GetSourceFiles(stepEvent.EventData, newFileNames);

						if (newFileNames.Count == 0)
						{
							stepEvent.Ignored = true;
						}
						else
						{
							stepEvent.Fingerprint = new NewIssueFingerprint(Type, newFileNames, null, null);
						}

						hasMatches = true;
					}
					else if (hasMatches && IsMaskedEventId(stepEvent.EventId.Value))
					{
						stepEvent.Ignored = true;
					}
				}
			}
		}

		/// <inheritdoc/>
		public override void RankSuspects(IIssueFingerprint fingerprint, List<SuspectChange> suspects)
		{
			string[] files = fingerprint.Keys.Where(x => x.Type == IssueKeyType.File).Select(x => x.Name).ToArray();
			foreach (SuspectChange suspect in suspects)
			{
				if (suspect.ContainsCode)
				{
					if (files.Any(x => suspect.ModifiesFile(x)))
					{
						suspect.Rank += 20;
					}
					else
					{
						suspect.Rank += 10;
					}
				}
			}
		}
	}
}
