// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Issues.Handlers
{
	/// <summary>
	/// Instance of a particular compile error
	/// </summary>
	[IssueHandler(Priority = 10)]
	class SymbolIssueHandler : IssueHandler
	{
		const string NodeName = "Node";
		const string EventIdName = "EventId";

		readonly List<IssueEventGroup> _issues = new List<IssueEventGroup>();

		static readonly IssueMetadata s_duplicateEventIdMetadata = new IssueMetadata(EventIdName, KnownLogEvents.Linker_DuplicateSymbol.Id.ToString());

		/// <summary>
		/// Determines if the given event id matches
		/// </summary>
		/// <param name="eventId">The event id to compare</param>
		/// <returns>True if the given event id matches</returns>
		public static bool IsMatchingEventId(EventId eventId)
		{
			return eventId == KnownLogEvents.Linker_UndefinedSymbol || eventId == KnownLogEvents.Linker_DuplicateSymbol || eventId == KnownLogEvents.Linker;
		}

		/// <summary>
		/// Determines if an event should be masked by this 
		/// </summary>
		/// <param name="eventId"></param>
		/// <returns></returns>
		static bool IsMaskedEventId(EventId eventId)
		{
			return eventId == KnownLogEvents.ExitCode || eventId == KnownLogEvents.Systemic_Xge_BuildFailed;
		}

		public static string GetSummaryStatic(IIssueFingerprint fingerprint, IssueSeverity severity)
		{
			HashSet<string> symbols = new HashSet<string>(fingerprint.Keys.Where(x => x.Type == IssueKeyType.Symbol).Select(x => x.Name));
			if (symbols.Count == 0)
			{
				string[] nodes = fingerprint.Metadata?.FindValues(NodeName).ToArray() ?? Array.Empty<string>();

				StringBuilder summary = new StringBuilder("Linker ");
				summary.Append((severity == IssueSeverity.Warning) ? "warnings" : "errors");
				if (nodes.Length > 0)
				{
					summary.Append($" in {StringUtils.FormatList(nodes, 2)}");
				}

				return summary.ToString();
			}
			else
			{
				string problemType = (fingerprint.Metadata?.Contains(s_duplicateEventIdMetadata) == true) ? "Duplicate" : "Undefined";
				if (symbols.Count == 1)
				{
					return $"{problemType} symbol '{symbols.First()}'";
				}
				else
				{
					return $"{problemType} symbols: {StringUtils.FormatList(symbols.ToArray(), 3)}";
				}
			}
		}

		/// <inheritdoc/>
		public override bool HandleEvent(IssueEvent issueEvent)
		{
			if (issueEvent.EventId != null)
			{
				EventId eventId = issueEvent.EventId.Value;
				if (IsMatchingEventId(eventId))
				{
					IssueEventGroup issue = new IssueEventGroup("Symbol", "{LegacySymbolIssueHandler}", IssueChangeFilter.Code);
					issue.Events.Add(issueEvent);
					issue.Keys.AddSymbols(issueEvent);
					issue.Metadata.Add(EventIdName, eventId.Id.ToString());

					if (issue.Keys.Count > 0)
					{
						_issues.Add(issue);
						return true;
					}
					if (_issues.Count > 0)
					{
						return true;
					}
				}
				else if (_issues.Count > 0 && IsMaskedEventId(eventId))
				{
					return true;
				}
			}
			return false;
		}

		/// <inheritdoc/>
		public override IEnumerable<IssueEventGroup> GetIssues() => _issues;
	}
}
