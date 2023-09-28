// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using EpicGames.Core;
using Horde.Server.Jobs;
using Horde.Server.Jobs.Graphs;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Issues.Handlers
{
	/// <summary>
	/// Instance of a particular compile error
	/// </summary>
	[IssueHandler(Priority = 10)]
	class CompileIssueHandler : SourceFileIssueHandler
	{
		/// <summary>
		/// Annotation describing the compile type
		/// </summary>
		const string CompileTypeAnnotation = "CompileType";

		/// <summary>
		/// Annotation specifying a group for compile issues from this node
		/// </summary>
		const string CompileGroupAnnotation = "CompileGroup";

		/// <inheritdoc/>
		public override string Type => "Compile";

		/// <inheritdoc/>
		public override string SummaryTemplate => "{Meta:CompileType} {Severity} in {Files}";

		/// <summary>
		/// Determines if the given event id matches
		/// </summary>
		/// <param name="eventId">The event id to compare</param>
		/// <returns>True if the given event id matches</returns>
		public static bool IsMatchingEventId(EventId eventId)
		{
			return eventId == KnownLogEvents.Compiler || eventId == KnownLogEvents.UHT || eventId == KnownLogEvents.AutomationTool_SourceFileLine || eventId == KnownLogEvents.MSBuild;
		}

		static bool IsMaskedEventId(EventId id) => id == KnownLogEvents.ExitCode || id == KnownLogEvents.Systemic_Xge_BuildFailed || id == KnownLogEvents.Compiler_Summary;

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

						string compileType = "Compile";
						if (annotations.TryGetValue(CompileTypeAnnotation, out string? type))
						{
							compileType = type;
						}

						string fingerprintType = Type;
						if (annotations.TryGetValue(CompileGroupAnnotation, out string? group))
						{
							fingerprintType = $"{fingerprintType}:{group}";
						}

						List<string> newMetadata = new List<string>();
						newMetadata.Add($"{CompileTypeAnnotation}={compileType}");

						stepEvent.Fingerprint = new NewIssueFingerprint(fingerprintType, newFileNames, null, newMetadata);
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
