// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using Horde.Server.Streams;

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

		/// <summary>
		/// Class of handler which can be explicitly enabled via a workflow
		/// </summary>
		public string? Tag { get; set; }
	}

	/// <summary>
	/// Context object for issue handlers
	/// </summary>
	public class IssueHandlerContext
	{
		/// <summary>
		/// Identifier for the current stream 
		/// </summary>
		public StreamId StreamId { get; }

		/// <summary>
		/// Identififer of the template
		/// </summary>
		public TemplateId TemplateId { get; }

		/// <summary>
		/// Identifier for the current node name 
		/// </summary>
		public string NodeName { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public IssueHandlerContext(StreamId streamId, TemplateId templateId, string nodeName)
		{
			StreamId = streamId;
			TemplateId = templateId;
			NodeName = nodeName;
		}
	}

	/// <summary>
	/// Interface for issue matchers
	/// </summary>
	abstract class IssueHandler
	{
		/// <summary>
		/// Attempts to assign a log event to an issue
		/// </summary>
		/// <param name="issueEvent">Events to process</param>
		/// <returns>Issue definition for this log event</returns>
		public abstract bool HandleEvent(IssueEvent issueEvent);

		/// <summary>
		/// Gets all the issues created by this handler
		/// </summary>
		/// <returns></returns>
		public abstract IEnumerable<IssueEventGroup> GetIssues();
	}
}
