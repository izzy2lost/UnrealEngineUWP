// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using EpicGames.Core;
using Horde.Server.Jobs;
using Horde.Server.Jobs.Graphs;
using Horde.Server.Logs;
using Horde.Server.Utilities;

namespace Horde.Server.Issues.Handlers
{
	/// <summary>
	/// Instance of a particular compile error
	/// </summary>
	class ScopedIssueHandler : IssueHandler
	{

		public override bool RequiresWorkflow { get; } = true;

		const string NodeName = "Node";
		const string ScopeName = "Scope";

		/// <inheritdoc/>
		public override string Type => "Scoped";

		/// <inheritdoc/>
		public override int Priority => 2;

		/// <inheritdoc/>
		public override void TagEvents(IJob job, INode node, IReadOnlyNodeAnnotations annotations, IReadOnlyList<IssueEvent> stepEvents)
		{			
			foreach (IssueEvent stepEvent in stepEvents)
			{
				string? scope = null;

				foreach (ILogEventLine line in stepEvent.Lines)
				{
					string? channelType;
					string? channelText;
					if (line.Data.TryGetNestedProperty("properties.channel.$type", out channelType) && line.Data.TryGetNestedProperty("properties.channel.$text", out channelText))
					{
						if (channelType != "Channel" || !channelText.StartsWith("Log", StringComparison.Ordinal))
						{
							scope = null;
							break;
						}
						
						if (scope != null && scope != channelText)
						{
							scope = null;
							break;
						}

						scope = channelText;
					}
				}

				if (scope == null)
				{
					continue;
				}

				string[] metadata = new[] { $"{NodeName}={node.Name}", $"{ScopeName}={scope}" };

				string fingerprintType = $"{Type}:{scope}";

				string hashSource = stepEvent.Message;
				
				if (TryGetHash(hashSource, out Md5Hash hash))
				{
					stepEvent.Fingerprint = new NewIssueFingerprint(fingerprintType, new[] { IssueKey.FromHash(hash) }, null, metadata);
				}
			}
		}

		static bool TryGetHash(string message, out Md5Hash hash)
		{
			string sanitized = message.ToUpperInvariant();
			sanitized = Regex.Replace(sanitized, @"(?<![a-zA-Z])(?:[A-Z]:|/)[^ :]+[/\\]SYNC[/\\]", "{root}/"); // Redact things that look like workspace roots; may be different between agents
			sanitized = Regex.Replace(sanitized, @"0[xX][0-9a-fA-F]+", "H"); // Redact hex strings
			sanitized = Regex.Replace(sanitized, @"\d[\d.,:]*", "n"); // Redact numbers and timestamp like things

			hash = Md5Hash.Compute(Encoding.UTF8.GetBytes(sanitized));
			return true;
		}

		/// <inheritdoc/>
		public override void RankSuspects(IIssueFingerprint fingerprint, List<SuspectChange> suspects)
		{
		}

		/// <inheritdoc/>
		public override string GetSummary(IIssueFingerprint fingerprint, IssueSeverity severity)
		{
			string severityText = (severity == IssueSeverity.Warning) ? "Warnings" : "Errors";
			string[] names = fingerprint.GetMetadataValues(NodeName).ToArray();
			string[] scopes = fingerprint.GetMetadataValues(ScopeName).ToArray();
			return $"{severityText} in {StringUtils.FormatList(names, 2)} - {StringUtils.FormatList(scopes, 2)}";
		}
	}
}
