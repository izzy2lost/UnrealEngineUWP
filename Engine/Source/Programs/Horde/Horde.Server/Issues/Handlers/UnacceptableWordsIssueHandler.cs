// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using EpicGames.Core;
using Horde.Server.Jobs;
using Horde.Server.Jobs.Graphs;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Issues.Handlers
{
	/// <summary>
	/// Instance of a Perforce case mismatch error
	/// </summary>
	[IssueHandler(Priority = 8)]
	class UnacceptableWordsIssueHandler : SourceFileIssueHandler
	{
		/// <inheritdoc/>
		public override string Type => "UnacceptableWords";

		/// <inheritdoc/>
		public override string SummaryTemplate => "Unacceptable words in {Files}";

		/// <summary>
		/// Determines if the given event id matches
		/// </summary>
		/// <param name="eventId">The event id to compare</param>
		/// <returns>True if the given event id matches</returns>
		public static bool IsMatchingEventId(EventId eventId)
		{
			return eventId == KnownLogEvents.AutomationTool_UnacceptableWords;
		}

		/// <inheritdoc/>
		public override void TagEvents(IJob job, INode node, IReadOnlyNodeAnnotations annotations, IReadOnlyList<IssueEvent> stepEvents)
		{
			foreach (IssueEvent stepEvent in stepEvents)
			{
				if(stepEvent.EventId != null && IsMatchingEventId(stepEvent.EventId.Value))
				{
					HashSet<IssueKey> newFileNames = new HashSet<IssueKey>();
					GetSourceFiles(stepEvent.EventData, newFileNames);

					stepEvent.Fingerprint = new NewIssueFingerprint(Type, newFileNames, null, null);
				}
			}
		}
	}
}
