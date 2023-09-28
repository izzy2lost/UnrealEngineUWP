// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using Horde.Server.Jobs;
using Horde.Server.Jobs.Graphs;

namespace Horde.Server.Issues
{
	/// <summary>
	/// Marks an issue handler that should be automatically inserted into the pipeline
	/// </summary>
	[AttributeUsage(AttributeTargets.Class)]
	sealed class IssueHandlerAttribute : Attribute
	{
		/// <summary> 
		/// Priority of this handler
		/// </summary>
		public int Priority { get; set; }
	}

	/// <summary>
	/// Interface for issue matchers
	/// </summary>
	abstract class IssueHandler
	{
		/// <summary>
		/// Identifier for the type of issue
		/// </summary>
		public abstract string Type { get; }

		/// <summary>
		/// Template for the issue summary
		/// </summary>
		public abstract string SummaryTemplate { get; }

		/// <summary>
		/// Filter for changes to consider as suspects
		/// </summary>
		public abstract IReadOnlyList<string> SuspectFilter { get; }

		/// <summary>
		/// Whether this handler requires being enabled by a workflow
		/// </summary>
		public virtual bool RequiresWorkflow { get; } = false;

		/// <summary>
		/// Tag all thethe events for a step completing
		/// </summary>
		/// <param name="job">The job that spawned the event</param>
		/// <param name="node">Node that was executed</param>
		/// <param name="annotations">Combined annotations from this step</param>
		/// <param name="events">Events from this step</param>
		public abstract void TagEvents(IJob job, INode node, IReadOnlyNodeAnnotations annotations, IReadOnlyList<IssueEvent> events);
	}

	/// <summary>
	/// Predefined filters for issue suspects
	/// </summary>
	public static class IssueSuspectFilter
	{
		/// <summary>
		/// Filter exclude all changes
		/// </summary>
		public static IReadOnlyList<string> None { get; } = Array.Empty<string>();

		/// <summary>
		/// Filter including all changes
		/// </summary>
		public static IReadOnlyList<string> All { get; } = new[] { "..." };

		/// <summary>
		/// Set of extensions to treat as code
		/// </summary>
		public static IReadOnlyList<string> Code { get; } = new[]
		{
			"*.c",
			"*.cc",
			"*.cpp",
			"*.inl",
			"*.m",
			"*.mm",
			"*.rc",
			"*.cs",
			"*.csproj",
			"*.h",
			"*.hpp",
			"*.inl",
			"*.usf",
			"*.ush",
			"*.uproject",
			"*.uplugin",
			"*.sln",
			"*.verse"
		};

		/// <summary>
		/// Set of file extensions to treat as content
		/// </summary>
		public static IReadOnlyList<string> Content { get; } = new[]
		{
			"*.uasset",
			"*.umap",
			"*.ini"
		};
	}
}
