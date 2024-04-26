// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Linq;
using System.Text.Json;
using EpicGames.Core;

namespace EpicGames.Horde.Issues.Handlers
{
	/// <summary>
	/// Instance of a specific Thread Sanitizer error
	/// </summary>
	[IssueHandler(Priority = 10)]
	public class ThreadSanitizerIssueHandler : IssueHandler
	{
		static readonly Utf8String s_summaryReasonAnnotation = new Utf8String("SummaryReason");
		static readonly Utf8String s_summarySourceFileAnnotation = new Utf8String("SummarySourceFile");
		readonly List<IssueEventGroup> _issues = new List<IssueEventGroup>();

		/// <summary>
		/// Log value describing the thread sanitizer error summary reason
		/// </summary>
		public static LogValue SummaryReason => new LogValue(s_summaryReasonAnnotation, "");

		/// <summary>
		/// Log value describing the thread sanitizer error summary source file
		/// </summary>
		public static LogValue SummarySourceFile => new LogValue(s_summarySourceFileAnnotation, "");

		static bool FindAnnotation(IssueEvent issueEvent, Utf8String annotation, ref string result)
		{
			foreach (JsonProperty property in issueEvent.Lines.SelectMany(x => x.FindPropertiesOfType(annotation)))
			{
				JsonElement value;
				if (property.Value.TryGetProperty(LogEventPropertyName.Text.Span, out value) && value.ValueKind == JsonValueKind.String)
				{
					result = value.GetString() ?? "";
					return true;
				}
			}
			return false;
		}

		/// <inheritdoc/>
		public override bool HandleEvent(IssueEvent issueEvent)
		{
			if (issueEvent.EventId == KnownLogEvents.Sanitizer_Thread)
			{
				string summaryReason = "error";
				string summaryFile = "";

				FindAnnotation(issueEvent, s_summaryReasonAnnotation, ref summaryReason);
				FindAnnotation(issueEvent, s_summarySourceFileAnnotation, ref summaryFile);
				string fingerprint = $"ThreadSanitizer:{summaryReason}:{summaryFile}";

				IssueEventGroup issue = new IssueEventGroup(fingerprint, $"Thread sanitizer found '{{Meta:{s_summaryReasonAnnotation}}}' in {{Meta:{s_summarySourceFileAnnotation}}}", IssueChangeFilter.Code);
				issue.Keys.Add(summaryReason, IssueKeyType.Note);
				issue.Metadata.Add(s_summaryReasonAnnotation.ToString(), summaryReason);
				issue.Keys.AddSourceFile(summaryFile, IssueKeyType.File);
				issue.Metadata.Add(s_summarySourceFileAnnotation.ToString(), summaryFile);
				issue.Events.Add(issueEvent);

				_issues.Add(issue);
				return true;
			}
			return false;
		}

		/// <inheritdoc/>
		public override IEnumerable<IssueEventGroup> GetIssues() => _issues;
	}
}
