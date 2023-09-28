// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using EpicGames.Core;
using Horde.Server.Jobs;
using Horde.Server.Jobs.Graphs;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Issues.Handlers
{
	/// <summary>
	/// Instance of a particular BuildGraph script error
	/// </summary>
	[IssueHandler(Priority = 10)]
	class BuildGraphIssueHandler : SourceFileIssueHandler
	{
		/// <inheritdoc/>
		public override string Type => "BuildGraph";

		/// <inheritdoc/>
		public override string SummaryTemplate => "BuildGraph {Severity} in {Files}";

		/// <inheritdoc/>
		public override IReadOnlyList<string> SuspectFilter => IssueSuspectFilter.All;

		/// <summary>
		/// Determines if the given event id matches
		/// </summary>
		/// <param name="eventId">The event id to compare</param>
		/// <returns>True if the given event id matches</returns>
		public static bool IsMatchingEventId(EventId eventId)
		{
			return eventId == KnownLogEvents.AutomationTool_BuildGraphScript;
		}

		static bool IsMaskedEventId(EventId id) => id == KnownLogEvents.ExitCode || id == KnownLogEvents.Systemic_Xge_BuildFailed;

		/// <inheritdoc/>
		public override void TagEvents(IJob job, INode node, IReadOnlyNodeAnnotations annotations, IReadOnlyList<IssueEvent> stepEvents)
		{
			bool hasMatches = false;
			foreach (IssueEvent stepEvent in stepEvents)
			{
				if (stepEvent.EventId.HasValue)
				{
					EventId eventId = stepEvent.EventId.Value;
					if (IsMatchingEventId(eventId))
					{
						HashSet<IssueKey> newFileNames = new HashSet<IssueKey>();
						GetSourceFiles(stepEvent.EventData, newFileNames);
						stepEvent.Fingerprint = new NewIssueFingerprint(Type, newFileNames, null, null);
						hasMatches = true;
					}
					else if (hasMatches && IsMaskedEventId(eventId))
					{
						stepEvent.Ignored = true;
					}
				}
			}
		}
	}
}
