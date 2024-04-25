// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace EpicGames.Horde.Issues.Handlers
{
	/// <summary>
	/// Instance of a particular Gauntlet error
	/// </summary>
	[IssueHandler(Priority = 10)]
	public class GauntletIssueHandler : IssueHandler
	{
		/// <summary>
		/// Prefix for framework keys
		/// </summary>
		const string FrameworkPrefix = "test framework";

		/// <summary>
		/// Prefix for test keys
		/// </summary>
		const string TestPrefix = "test";

		/// <summary>
		/// Prefix for device keys
		/// </summary>
		const string DevicePrefix = "device";

		/// <summary>
		/// Prefix for build drop keys
		/// </summary>
		const string BuildDropPrefix = "build drop";

		/// <summary>
		/// Prefix for fatal failure keys
		/// </summary>
		const string FatalPrefix = "fatal";

		/// <summary>
		/// Callstack log type property
		/// </summary>
		static readonly Utf8String s_callstackLogType = new Utf8String("Callstack");

		/// <summary>
		/// Max Message Length to hash
		/// </summary>
		const int MaxMessageLength = 2000;

		readonly IssueHandlerContext _context;
		readonly List<IssueEventGroup> _issues = new List<IssueEventGroup>();

		/// <summary>
		///  Known Gauntlet events
		/// </summary>
		static readonly Dictionary<EventId, string> s_knownGauntletEvents = new Dictionary<EventId, string>
		{
			{ KnownLogEvents.Gauntlet, FrameworkPrefix},
			{ KnownLogEvents.Gauntlet_TestEvent, TestPrefix},
			{ KnownLogEvents.Gauntlet_DeviceEvent, DevicePrefix},
			{ KnownLogEvents.Gauntlet_UnrealEngineTestEvent, TestPrefix},
			{ KnownLogEvents.Gauntlet_BuildDropEvent, BuildDropPrefix},
			{ KnownLogEvents.Gauntlet_FatalEvent, FatalPrefix}
		};

		/// <summary>
		/// Constructor
		/// </summary>
		public GauntletIssueHandler(IssueHandlerContext context) => _context = context;

		/// <summary>
		/// Determines if the given event id matches
		/// </summary>
		/// <param name="eventId">The event id to compare</param>
		/// <returns>True if the given event id matches</returns>
		public static bool IsMatchingEventId(EventId eventId)
		{
			return s_knownGauntletEvents.ContainsKey(eventId);
		}

		/// <summary>
		/// Return the prefix string associate with the event id
		/// </summary>
		/// <param name="eventId">The event id to get the information from</param>
		/// <returns>The corresponding prefix as a string</returns>
		public static string GetEventPrefix(EventId eventId)
		{
			return s_knownGauntletEvents[eventId];
		}

		/// <summary>
		/// Produce a hash from error message
		/// </summary>
		/// <param name="issueEvent">The issue event</param>
		/// <param name="keys">Receives a set of the keys</param>
		/// <param name="metadata">Receives a set of metadata</param>
		private void GetHash(IssueEvent issueEvent, HashSet<IssueKey> keys, HashSet<IssueMetadata> metadata)
		{
			if (TryGetHash(issueEvent, out Md5Hash hash))
			{
				string key = $"hash:{hash}";
				if (!EventHasCallstackProperty(issueEvent))
				{
					// add job step salt if no Callstack property was found
					key += $":{_context.StreamId}:{_context.NodeName}";
				}
				keys.Add(key, IssueKeyType.None);
			}
			else
			{
				// Not enough information, make it an issue associated with only the job step
				keys.Add($"{_context.StreamId}:{_context.NodeName}", IssueKeyType.None);
			}
			metadata.Add("Node", _context.NodeName);
		}

		private static bool TryGetHash(IssueEvent issueEvent, out Md5Hash hash)
		{
			string sanitized = issueEvent.Message.ToUpperInvariant();
			sanitized = sanitized.Length > MaxMessageLength ? sanitized.Substring(0, MaxMessageLength) : sanitized;
			sanitized = Regex.Replace(sanitized, @"(?<![a-zA-Z])(?:[A-Z]:|/)[^ :]+[/\\]SYNC[/\\]", "{root}/"); // Redact things that look like workspace roots; may be different between agents
			sanitized = Regex.Replace(sanitized, @"0[xX][0-9a-fA-F]+", "H"); // Redact hex strings
			sanitized = Regex.Replace(sanitized, @"\d[\d.,:]*", "n"); // Redact numbers and timestamp like things

			if (sanitized.Length > 30)
			{
				hash = Md5Hash.Compute(Encoding.UTF8.GetBytes(sanitized));
				return true;
			}
			else
			{
				hash = Md5Hash.Zero;
				return false;
			}
		}

		private static bool EventHasCallstackProperty(IssueEvent issueEvent)
		{
			return issueEvent.Lines.Any(x => FindNestedPropertyOfType(x, s_callstackLogType) != null);
		}

		private static JsonProperty? FindNestedPropertyOfType(JsonLogEvent logEvent, Utf8String type)
		{
			JsonElement line = JsonDocument.Parse(logEvent.Data).RootElement;
			JsonElement properties;
			if (line.TryGetProperty("properties", out properties) && properties.ValueKind == JsonValueKind.Object)
			{
				foreach (JsonProperty property in properties.EnumerateObject())
				{
					if (property.NameEquals(type.Span))
					{
						return property;
					}
				}
			}

			return null;
		}

		/// <inheritdoc/>
		public override bool HandleEvent(IssueEvent issueEvent)
		{
			if (issueEvent.EventId != null && IsMatchingEventId(issueEvent.EventId.Value))
			{
				IssueEventGroup issue = new IssueEventGroup("Gauntlet", "Automation {Meta:GauntletType} {Severity} in {Meta:Node}", IssueChangeFilter.All);
				issue.Events.Add(issueEvent);
				GetHash(issueEvent, issue.Keys, issue.Metadata);
				issue.Metadata.Add("GauntletType", GetEventPrefix(issueEvent.EventId.Value));
				_issues.Add(issue);

				return true;
			}
			return false;
		}

		/// <inheritdoc/>
		public override IEnumerable<IssueEventGroup> GetIssues() => _issues;
	}
}
