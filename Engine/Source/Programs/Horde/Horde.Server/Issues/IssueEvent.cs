// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading;
using EpicGames.Core;
using Horde.Server.Logs;
using HordeCommon;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Issues
{
	/// <summary>
	/// Wraps a log event and allows it to be tagged by issue handlers
	/// </summary>
	public class IssueEvent
	{
		/// <summary>
		/// The underlying log event
		/// </summary>
		public ILogEvent Event { get; }

		/// <summary>
		/// The log event data
		/// </summary>
		public ILogEventData EventData { get; }

		/// <summary>
		/// Index of the line within this log
		/// </summary>
		public int LineIndex => Event.LineIndex;

		/// <summary>
		/// Severity of the event
		/// </summary>
		public EventSeverity Severity => EventData.Severity;

		/// <summary>
		/// The type of event
		/// </summary>
		public EventId? EventId => EventData.EventId;

		/// <summary>
		/// The complete rendered message, in plaintext
		/// </summary>
		public string Message => EventData.Message;

		/// <summary>
		/// Gets this event data as a BSON document
		/// </summary>
		public IReadOnlyList<JsonLogEvent> Lines => EventData.Lines;

		/// <summary>
		/// Constructor
		/// </summary>
		public IssueEvent(ILogEvent stepEvent, ILogEventData stepEventData)
		{
			Event = stepEvent;
			EventData = stepEventData;
		}

		/// <summary>
		/// Tests whether this is a systemic event id
		/// </summary>
		/// <returns>True if this is a systemic event id</returns>
		public bool IsSystemic()
		{
			return EventId.HasValue && (EventId.Value.Id >= KnownLogEvents.Systemic.Id && EventId.Value.Id <= KnownLogEvents.Systemic_Max.Id);
		}

		/// <inheritdoc/>
		public override string ToString()
		{
			return $"[{Event.LineIndex}] {EventData.Message}";
		}
	}

	/// <summary>
	/// A group of <see cref="IssueEvent"/> objects with their fingerprint
	/// </summary>
	class IssueEventGroup
	{
		/// <summary>
		/// Unique id for this group
		/// </summary>
		public int TraceId { get; }

		/// <summary>
		/// Fingerprint for the event
		/// </summary>
		public NewIssueFingerprint Fingerprint { get; }

		public HashSet<IssueKey> Keys => Fingerprint.Keys;
		public HashSet<IssueMetadata> Metadata => Fingerprint.Metadata;

		/// <summary>
		/// Individual log events
		/// </summary>
		public List<IssueEvent> Events { get; } = new List<IssueEvent>();

		static int s_nextId = 1;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="fingerprint">Fingerprint for the event</param>
		public IssueEventGroup(NewIssueFingerprint fingerprint)
		{
			TraceId = Interlocked.Increment(ref s_nextId);
			Fingerprint = fingerprint;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="fingerprint">Fingerprint for the event</param>
		/// <param name="issueEvent"></param>
		public IssueEventGroup(NewIssueFingerprint fingerprint, IssueEvent issueEvent)
			: this(fingerprint)
		{
			Events.Add(issueEvent);
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="type">The type of issue</param>
		/// <param name="summaryTemplate">Template for the summary string to display for the issue</param>
		/// <param name="changeFilter">Filter for changes covered by this issue</param>
		public IssueEventGroup(string type, string summaryTemplate, IReadOnlyList<string> changeFilter)
			: this(new NewIssueFingerprint(type, summaryTemplate, changeFilter))
		{
		}

		/// <summary>
		/// Merge with another group
		/// </summary>
		/// <param name="otherGroup">The group to merge with</param>
		/// <returns>A new group combining both groups</returns>
		public IssueEventGroup MergeWith(IssueEventGroup otherGroup)
		{
			IssueEventGroup newGroup = new IssueEventGroup(NewIssueFingerprint.Merge(Fingerprint, otherGroup.Fingerprint));
			newGroup.Events.AddRange(Events);
			newGroup.Events.AddRange(otherGroup.Events);
			return newGroup;
		}

		/// <inheritdoc/>
		public override string ToString() => Fingerprint.ToString();
	}
}
