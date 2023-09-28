// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text.Json;
using EpicGames.Core;
using Horde.Server.Jobs;
using Horde.Server.Jobs.Graphs;
using Horde.Server.Logs;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Issues.Handlers
{
	/// <summary>
	/// Instance of a Perforce case mismatch error
	/// </summary>
	[IssueHandler(Priority = 10)]
	class PerforceCaseIssueHandler : IssueHandler
	{
		/// <inheritdoc/>
		public override string Type => "PerforceCase";

		/// <inheritdoc/>
		public override string SummaryTemplate => "Inconsistent case for {Files}";

		/// <inheritdoc/>
		public override IReadOnlyList<string> SuspectFilter => IssueSuspectFilter.All;

		/// <summary>
		/// Determines if the given event id matches
		/// </summary>
		/// <param name="eventId">The event id to compare</param>
		/// <returns>True if the given event id matches</returns>
		public static bool IsMatchingEventId(EventId eventId)
		{
			return eventId == KnownLogEvents.AutomationTool_PerforceCase;
		}

		/// <summary>
		/// Extracts a list of source files from an event
		/// </summary>
		/// <param name="logEventData">The event data</param>
		/// <param name="fileNames">List of source files</param>
		static void GetSourceFiles(ILogEventData logEventData, HashSet<IssueKey> fileNames)
		{
			foreach (JsonProperty property in logEventData.FindPropertiesOfType(LogValueType.DepotPath))
			{
				JsonElement value;
				if (property.Value.TryGetProperty(LogEventPropertyName.Text.Span, out value) && value.ValueKind == JsonValueKind.String)
				{
					string fileName = GetFileName(value.GetString() ?? String.Empty);
					fileNames.Add(new IssueKey(fileName, IssueKeyType.File));
				}
			}
		}

		/// <summary>
		/// Extracts the name part of a depot file
		/// </summary>
		/// <param name="path"></param>
		/// <returns></returns>
		static string GetFileName(string path)
		{
			return path.Substring(path.LastIndexOf('/') + 1);
		}

		/// <inheritdoc/>
		public override void TagEvents(IJob job, INode node, IReadOnlyNodeAnnotations annotations, IReadOnlyList<IssueEvent> stepEvents)
		{
			foreach (IssueEvent stepEvent in stepEvents)
			{
				if (stepEvent.EventId != null && IsMatchingEventId(stepEvent.EventId.Value))
				{
					HashSet<IssueKey> newFileNames = new HashSet<IssueKey>();
					GetSourceFiles(stepEvent.EventData, newFileNames);

					stepEvent.Fingerprint = new NewIssueFingerprint(Type, newFileNames, null, null);
				}
			}
		}
	}
}
