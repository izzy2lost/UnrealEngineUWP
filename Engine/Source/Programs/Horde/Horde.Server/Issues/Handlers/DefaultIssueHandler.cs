// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using Horde.Server.Jobs;
using Horde.Server.Jobs.Graphs;

namespace Horde.Server.Issues.Handlers
{
	/// <summary>
	/// Instance of a particular compile error
	/// </summary>
	[IssueHandler(Priority = 0)]
	class DefaultIssueHandler : IssueHandler
	{
		/// <summary>
		/// Name of the handler
		/// </summary>
		public const string TypeConst = "Default";

		/// <inheritdoc/>
		public override string Type => TypeConst;

		/// <inheritdoc/>
		public override string SummaryTemplate => "{Severity} in {Nodes}";

		/// <inheritdoc/>
		public override void TagEvents(IJob job, INode node, IReadOnlyNodeAnnotations annotations, IReadOnlyList<IssueEvent> stepEvents)
		{
			NewIssueFingerprint fingerprint = new NewIssueFingerprint(TypeConst, new[] { IssueKey.FromStep(job.StreamId, job.TemplateId, node.Name) }, null, null);
			foreach (IssueEvent stepEvent in stepEvents)
			{
				stepEvent.Fingerprint = fingerprint;
			}
		}

		/// <inheritdoc/>
		public override void RankSuspects(IIssueFingerprint fingerprint, List<SuspectChange> suspects)
		{
		}
	}
}
