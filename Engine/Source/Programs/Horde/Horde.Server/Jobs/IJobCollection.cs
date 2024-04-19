// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Jobs.Bisect;
using EpicGames.Horde.Jobs.Templates;
using EpicGames.Horde.Streams;
using EpicGames.Horde.Users;
using Horde.Server.Acls;
using Horde.Server.Jobs.Graphs;
using Horde.Server.Streams;

namespace Horde.Server.Jobs
{
	/// <summary>
	/// Options for creating a new job
	/// </summary>
	public class CreateJobOptions
	{
		/// <inheritdoc cref="IJob.PreflightChange"/>
		public int? PreflightChange { get; set; }

		/// <inheritdoc cref="IJob.ClonedPreflightChange"/>
		public int? ClonedPreflightChange { get; set; }

		/// <inheritdoc cref="IJob.PreflightDescription"/>
		public string? PreflightDescription { get; set; }

		/// <inheritdoc cref="IJob.StartedByUserId"/>
		public UserId? StartedByUserId { get; set; }

		/// <inheritdoc cref="IJob.StartedByBisectTaskId"/>
		public BisectTaskId? StartedByBisectTaskId { get; set; }

		/// <inheritdoc cref="IJob.Priority"/>
		public Priority? Priority { get; set; }

		/// <inheritdoc cref="IJob.AutoSubmit"/>
		public bool? AutoSubmit { get; set; }

		/// <inheritdoc cref="IJob.UpdateIssues"/>
		public bool? UpdateIssues { get; set; }

		/// <inheritdoc cref="IJob.PromoteIssuesByDefault"/>
		public bool? PromoteIssuesByDefault { get; set; }

		/// <inheritdoc cref="IJob.JobOptions"/>
		public JobOptions? JobOptions { get; set; }

		/// <inheritdoc cref="IJob.Claims"/>
		public List<AclClaimConfig> Claims { get; set; } = new List<AclClaimConfig>();

		/// <summary>
		/// List of downstream job triggers
		/// </summary>
		public List<ChainedJobTemplateConfig> JobTriggers { get; } = new List<ChainedJobTemplateConfig>();

		/// <inheritdoc cref="IJob.ShowUgsBadges"/>
		public bool ShowUgsBadges { get; set; }

		/// <inheritdoc cref="IJob.ShowUgsAlerts"/>
		public bool ShowUgsAlerts { get; set; }

		/// <inheritdoc cref="IJob.NotificationChannel"/>
		public string? NotificationChannel { get; set; }

		/// <inheritdoc cref="IJob.NotificationChannelFilter"/>
		public string? NotificationChannelFilter { get; set; }

		/// <inheritdoc cref="IJob.Parameters"/>
		public Dictionary<ParameterId, string> Parameters { get; } = new Dictionary<ParameterId, string>();

		/// <inheritdoc cref="IJob.Arguments"/>
		public List<string> Arguments { get; } = new List<string>();

		/// <inheritdoc cref="IJob.Environment"/>
		public Dictionary<string, string> Environment { get; } = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

		/// <summary>
		/// Default constructor
		/// </summary>
		public CreateJobOptions()
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public CreateJobOptions(TemplateRefConfig templateRefConfig)
		{
			JobOptions = templateRefConfig.JobOptions;
			PromoteIssuesByDefault = templateRefConfig.PromoteIssuesByDefault;
			if (templateRefConfig.ChainedJobs != null)
			{
				JobTriggers.AddRange(templateRefConfig.ChainedJobs);
			}
			ShowUgsBadges = templateRefConfig.ShowUgsBadges;
			ShowUgsAlerts = templateRefConfig.ShowUgsAlerts;
			NotificationChannel = templateRefConfig.NotificationChannel;
			NotificationChannelFilter = templateRefConfig.NotificationChannelFilter;
		}
	}

	/// <summary>
	/// Interface for a collection of job documents
	/// </summary>
	public interface IJobCollection
	{
		/// <summary>
		/// Creates a new job
		/// </summary>
		/// <param name="jobId">A requested job id</param>
		/// <param name="streamId">Unique id of the stream that this job belongs to</param>
		/// <param name="templateRefId">Name of the template ref</param>
		/// <param name="templateHash">Template for this job</param>
		/// <param name="graph">The graph for the new job</param>
		/// <param name="name">Name of the job</param>
		/// <param name="change">The change to build</param>
		/// <param name="codeChange">The corresponding code changelist number</param>
		/// <param name="options">Additional options for the new job</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The new job document</returns>
		Task<IJob> AddAsync(JobId jobId, StreamId streamId, TemplateId templateRefId, ContentHash templateHash, IGraph graph, string name, int change, int codeChange, CreateJobOptions options, CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets a job with the given unique id
		/// </summary>
		/// <param name="jobId">Job id to search for</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about the given job</returns>
		Task<IJob?> GetAsync(JobId jobId, CancellationToken cancellationToken = default);

		/// <summary>
		/// Delete all the jobs for a stream
		/// </summary>
		/// <param name="streamId">Unique id of the stream</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Async task</returns>
		Task RemoveStreamAsync(StreamId streamId, CancellationToken cancellationToken = default);

		/// <summary>
		/// Searches for jobs matching the given criteria
		/// </summary>
		/// <param name="jobIds">List of job ids to return</param>
		/// <param name="streamId">The stream containing the job</param>
		/// <param name="name">Name of the job</param>
		/// <param name="templates">Templates to look for</param>
		/// <param name="minChange">The minimum changelist number</param>
		/// <param name="maxChange">The maximum changelist number</param>
		/// <param name="preflightChange">Preflight change to find</param>
		/// <param name="preflightOnly">Whether to only include preflights</param>
		/// <param name="startedByUser">User id for which to include jobs</param>
		/// <param name="preflightStartedByUser">User for which to include preflight jobs</param>
		/// <param name="minCreateTime">The minimum creation time</param>
		/// <param name="maxCreateTime">The maximum creation time</param>
		/// <param name="modifiedBefore">Filter the results by modified time</param>
		/// <param name="modifiedAfter">Filter the results by modified time</param>
		/// <param name="batchState">One or more batches matches this state</param>
		/// <param name="index">Index of the first result to return</param>
		/// <param name="count">Number of results to return</param>
		/// <param name="consistentRead">If the database read should be made to the replica server</param>
		/// <param name="indexHint">Name of index to be specified as a hint to the database query planner</param>
		/// <param name="excludeUserJobs">Whether to exclude user jobs from the find</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of jobs matching the given criteria</returns>
		Task<IReadOnlyList<IJob>> FindAsync(JobId[]? jobIds = null, StreamId? streamId = null, string? name = null, TemplateId[]? templates = null, int? minChange = null, int? maxChange = null, int? preflightChange = null, bool? preflightOnly = null, UserId? preflightStartedByUser = null, UserId? startedByUser = null, DateTimeOffset? minCreateTime = null, DateTimeOffset? maxCreateTime = null, DateTimeOffset? modifiedBefore = null, DateTimeOffset? modifiedAfter = null, JobStepBatchState? batchState = null, int? index = null, int? count = null, bool consistentRead = true, string? indexHint = null, bool? excludeUserJobs = null, CancellationToken cancellationToken = default);

		/// <summary>
		/// Searches for jobs matching the given criteria
		/// </summary>
		/// <param name="bisectTaskId">The bisect task to find jobs for</param>
		/// <param name="running">Whether to filter by running jobs</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of jobs matching the given criteria</returns>
		IAsyncEnumerable<IJob> FindBisectTaskJobsAsync(BisectTaskId bisectTaskId, bool? running, CancellationToken cancellationToken = default);

		/// <summary>
		/// Searches for jobs in a specified stream and templates
		/// </summary>
		/// <param name="streamId">The stream containing the job</param>
		/// <param name="templates">Templates to look for</param>
		/// <param name="preflightStartedByUser">User for which to include preflight jobs</param>
		/// <param name="maxCreateTime">The maximum creation time</param>
		/// <param name="modifiedAfter">Filter the results by modified time</param>
		/// <param name="index">Index of the first result to return</param>
		/// <param name="count">Number of results to return</param>
		/// <param name="consistentRead">If the database read should be made to the replica server</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task<IReadOnlyList<IJob>> FindLatestByStreamWithTemplatesAsync(StreamId streamId, TemplateId[] templates, UserId? preflightStartedByUser = null, DateTimeOffset? maxCreateTime = null, DateTimeOffset? modifiedAfter = null, int? index = null, int? count = null, bool consistentRead = false, CancellationToken cancellationToken = default);

		/// <summary>
		/// Adds an issue to a job
		/// </summary>
		/// <param name="jobId">The job id</param>
		/// <param name="issueId">The issue to add</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Async task</returns>
		Task AddIssueToJobAsync(JobId jobId, int issueId, CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets a queue of jobs to consider for execution
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Sorted list of jobs to execute</returns>
		Task<IReadOnlyList<IJob>> GetDispatchQueueAsync(CancellationToken cancellationToken = default);

		/// <summary>
		/// Upgrade all documents in the collection
		/// </summary>
		/// <returns>Async task</returns>
		Task UpgradeDocumentsAsync();
	}
}
