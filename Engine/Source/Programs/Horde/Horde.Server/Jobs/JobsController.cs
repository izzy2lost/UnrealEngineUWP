// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Artifacts;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Jobs.Graphs;
using EpicGames.Horde.Jobs.Templates;
using EpicGames.Horde.Logs;
using EpicGames.Horde.Streams;
using EpicGames.Horde.Users;
using Horde.Server.Acls;
using Horde.Server.Agents;
using Horde.Server.Artifacts;
using Horde.Server.Commits;
using Horde.Server.Jobs.Artifacts;
using Horde.Server.Jobs.Graphs;
using Horde.Server.Jobs.Templates;
using Horde.Server.Jobs.Timing;
using Horde.Server.Notifications;
using Horde.Server.Perforce;
using Horde.Server.Server;
using Horde.Server.Streams;
using Horde.Server.Users;
using Horde.Server.Utilities;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;
using MongoDB.Bson;

namespace Horde.Server.Jobs
{
	/// <summary>
	/// Controller for the /api/v1/jobs endpoing
	/// </summary>
	[ApiController]
	[Authorize]
	[Route("[controller]")]
	public class JobsController : HordeControllerBase
	{
		private readonly IGraphCollection _graphs;
		private readonly ICommitService _commitService;
		private readonly IPerforceService _perforce;
		private readonly JobService _jobService;
		private readonly ITemplateCollection _templateCollection;
		private readonly IArtifactCollection _artifactCollection;
		private readonly IArtifactCollectionV1 _artifactCollectionV1;
		private readonly IUserCollection _userCollection;
		private readonly INotificationService _notificationService;
		private readonly AgentService _agentService;
		private readonly IOptionsSnapshot<GlobalConfig> _globalConfig;

		/// <summary>
		/// Constructor
		/// </summary>
		public JobsController(IGraphCollection graphs, ICommitService commitService, IPerforceService perforce, JobService jobService, ITemplateCollection templateCollection, IArtifactCollection artifactCollection, IArtifactCollectionV1 artifactCollectionV1, IUserCollection userCollection, INotificationService notificationService, AgentService agentService, IOptionsSnapshot<GlobalConfig> globalConfig)
		{
			_graphs = graphs;
			_commitService = commitService;
			_perforce = perforce;
			_jobService = jobService;
			_templateCollection = templateCollection;
			_artifactCollection = artifactCollection;
			_artifactCollectionV1 = artifactCollectionV1;
			_userCollection = userCollection;
			_notificationService = notificationService;
			_agentService = agentService;
			_globalConfig = globalConfig;
		}

		/// <summary>
		/// Creates a new job
		/// </summary>
		/// <param name="create">Properties of the new job</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Id of the new job</returns>
		[HttpPost]
		[Route("/api/v1/jobs")]
		public async Task<ActionResult<CreateJobResponse>> CreateJobAsync([FromBody] CreateJobRequest create, CancellationToken cancellationToken = default)
		{
			StreamConfig? streamConfig;
			if (!_globalConfig.Value.TryGetStream(create.StreamId, out streamConfig))
			{
				return NotFound(create.StreamId);
			}
			if (!streamConfig.Authorize(JobAclAction.CreateJob, User))
			{
				return Forbid(JobAclAction.CreateJob, streamConfig.Id);
			}

			// Get the name of the template ref
			TemplateId templateRefId = create.TemplateId;

			// Augment the request with template properties
			TemplateRefConfig? templateRefConfig;
			if (!streamConfig.TryGetTemplate(templateRefId, out templateRefConfig))
			{
				return BadRequest($"Template {create.TemplateId} is not available for stream {streamConfig.Id}");
			}
			if (!templateRefConfig.Authorize(JobAclAction.CreateJob, User))
			{
				return Forbid(JobAclAction.CreateJob, streamConfig.Id);
			}

			ITemplate? template = await _templateCollection.GetOrAddAsync(templateRefConfig);
			if (template == null)
			{
				return BadRequest("Missing template referenced by {TemplateId}", create.TemplateId);
			}
			if (!template.AllowPreflights && create.PreflightChange > 0)
			{
				return BadRequest("Template {TemplateId} does not allow preflights", create.TemplateId);
			}

			// Get the name of the new job
			string name = create.Name ?? template.Name;
			if (create.TemplateId == new TemplateId("stage-to-marketplace") && create.Arguments != null)
			{
				foreach (string argument in create.Arguments)
				{
					const string Prefix = "-set:UserContentItems=";
					if (argument.StartsWith(Prefix, StringComparison.Ordinal))
					{
						name += $" - {argument.Substring(Prefix.Length)}";
						break;
					}
				}
			}

			// Environment variables for the job
			Dictionary<string, string> environment = new Dictionary<string, string>();

			// Check the preflight change is valid
			ShelfInfo? shelfInfo = null;
			if (create.PreflightChange != null)
			{
				(CheckShelfResult result, shelfInfo) = await _perforce.CheckShelfAsync(streamConfig, create.PreflightChange.Value, HttpContext.RequestAborted);
				switch (result)
				{
					case CheckShelfResult.Ok:
						break;
					case CheckShelfResult.NoChange:
						return BadRequest(KnownLogEvents.Horde_InvalidPreflight, "CL {Change} does not exist", create.PreflightChange);
					case CheckShelfResult.NoShelvedFiles:
						return BadRequest(KnownLogEvents.Horde_InvalidPreflight, "CL {Change} does not contain any shelved files", create.PreflightChange);
					case CheckShelfResult.WrongStream:
						return BadRequest(KnownLogEvents.Horde_InvalidPreflight, "CL {Change} does not contain files in {Stream}", create.PreflightChange, streamConfig.Name);
					case CheckShelfResult.MixedStream:
						return BadRequest(KnownLogEvents.Horde_InvalidPreflight, "CL {Change} contains files from multiple streams", create.PreflightChange);
					default:
						return BadRequest(KnownLogEvents.Horde_InvalidPreflight, "CL {Change} cannot be preflighted ({Result})", create.PreflightChange, result);
				}

				if (shelfInfo!.Tags != null)
				{
					string tagList = String.Join(";", shelfInfo.Tags.Select(x => x.ToString()));
					environment.Add("UE_HORDE_PREFLIGHT_TAGS", tagList);
				}
			}

			// Get the priority of the new job
			Priority priority = create.Priority ?? template.Priority ?? Priority.Normal;

			// New groups for the job
			IGraph graph = await _graphs.AddAsync(template, streamConfig.InitialAgentType, cancellationToken);

			// Get the commits for this stream
			ICommitCollection commits = _commitService.GetCollection(streamConfig);

			// Get the change to build
			int change = await GetChangeToBuildAsync(create, streamConfig.Id, template, shelfInfo, commits, HttpContext.RequestAborted);

			// And get the matching code changelist
			ICommit? lastCodeCommit = await commits.GetLastCodeChangeAsync(change, HttpContext.RequestAborted);
			int codeChange = lastCodeCommit?.Number ?? change;

			// New properties for the job
			List<string> arguments = create.Arguments ?? template.GetDefaultArguments(false);

			bool? updateIssues = null;
			if (template.UpdateIssues)
			{
				updateIssues = true;
			}
			else if (create.UpdateIssues.HasValue)
			{
				updateIssues = create.UpdateIssues.Value;
			}

			// Create options for the new job
			CreateJobOptions options = new CreateJobOptions(templateRefConfig);
			options.PreflightChange = create.PreflightChange;
			options.PreflightDescription = shelfInfo?.Description;
			options.StartedByUserId = User.GetUserId();
			options.Priority = priority;
			options.AutoSubmit = create.AutoSubmit;
			options.UpdateIssues = updateIssues;
			options.Claims.AddRange(User.Claims.Select(x => new AclClaimConfig(x)));
			options.Arguments.AddRange(arguments);
			options.JobOptions ??= create.JobOptions;

			if (create.Parameters != null)
			{
				foreach ((ParameterId parameter, string value) in create.Parameters)
				{
					options.Parameters[parameter] = value;
				}
				foreach (Parameter parameter in template.Parameters)
				{
					parameter.GetArguments(options.Parameters, false, options.Arguments);
				}
			}

			foreach ((string key, string value) in environment)
			{
				options.Environment[key] = value;
			}

			// Create the job
			IJob job = await _jobService.CreateJobAsync(null, streamConfig, templateRefId, template.Hash, graph, name, change, codeChange, options, cancellationToken);
			await UpdateNotificationsAsync(job.Id, new UpdateNotificationsRequest { Slack = true }, cancellationToken);
			return new CreateJobResponse(job.Id.ToString());
		}

		async ValueTask<int> GetChangeToBuildAsync(CreateJobRequest create, StreamId streamId, ITemplate template, ShelfInfo? shelfInfo, ICommitCollection commits, CancellationToken cancellationToken)
		{
			// If there's an explicit change specified, use that
			if (create.Change.HasValue && create.Change.Value != -1)
			{
				return create.Change.Value;
			}

			// Evaluate the change queries
			if (create.ChangeQueries != null && create.ChangeQueries.Count > 0)
			{
				int? change = await _jobService.EvaluateChangeQueriesAsync(streamId, create.ChangeQueries, shelfInfo?.Tags, commits, cancellationToken);
				if (change != null)
				{
					return change.Value;
				}
			}

			// If we need to submit a new change, do that
			if (create.PreflightChange == null && template.SubmitNewChange != null)
			{
				return await commits.CreateNewAsync(template, cancellationToken);
			}

			// Otherwise return the latest change
			return await commits.GetLatestNumberAsync(cancellationToken);
		}

		/// <summary>
		/// Deletes a specific job.
		/// </summary>
		/// <param name="jobId">Id of the job to delete</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Async task</returns>
		[HttpDelete]
		[Route("/api/v1/jobs/{jobId}")]
		public async Task<ActionResult> DeleteJobAsync(JobId jobId, CancellationToken cancellationToken)
		{
			IJob? job = await _jobService.GetJobAsync(jobId, cancellationToken);
			if (job == null)
			{
				return NotFound(jobId);
			}
			if (!_globalConfig.Value.Authorize(job, JobAclAction.DeleteJob, User))
			{
				return Forbid(JobAclAction.DeleteJob, jobId);
			}
			if (!await _jobService.DeleteJobAsync(job, cancellationToken))
			{
				return NotFound(jobId);
			}
			return Ok();
		}

		/// <summary>
		/// Updates a specific job.
		/// </summary>
		/// <param name="jobId">Id of the job to find</param>
		/// <param name="request">Settings to update in the job</param>
		/// <returns>Async task</returns>
		[HttpPut]
		[Route("/api/v1/jobs/{jobId}")]
		public async Task<ActionResult> UpdateJobAsync(JobId jobId, [FromBody] UpdateJobRequest request)
		{
			IJob? job = await _jobService.GetJobAsync(jobId);
			if (job == null)
			{
				return NotFound(jobId);
			}

			StreamConfig? streamConfig;
			if (!_globalConfig.Value.TryGetStream(job.StreamId, out streamConfig))
			{
				return NotFound(job.StreamId);
			}
			if (!streamConfig.Authorize(JobAclAction.UpdateJob, User))
			{
				return Forbid(JobAclAction.UpdateJob, jobId);
			}

			// Convert legacy behavior of clearing out the argument to setting the aborted flag
			if (request.Arguments != null && request.Arguments.Count == 0)
			{
				request.Aborted = true;
				request.Arguments = null;
			}

			UserId? abortedByUserId = null;
			if (request.Aborted ?? false)
			{
				abortedByUserId = User.GetUserId();
			}

			IJob? newJob = await _jobService.UpdateJobAsync(job, name: request.Name, priority: request.Priority, autoSubmit: request.AutoSubmit, abortedByUserId: abortedByUserId, arguments: request.Arguments);
			if (newJob == null)
			{
				return NotFound(jobId);
			}
			return Ok();
		}

		/// <summary>
		/// Updates notifications for a specific job.
		/// </summary>
		/// <param name="jobId">Id of the job to find</param>
		/// <param name="request">The notification request</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about the requested job</returns>
		[HttpPut]
		[Route("/api/v1/jobs/{jobId}/notifications")]
		public async Task<ActionResult> UpdateNotificationsAsync(JobId jobId, [FromBody] UpdateNotificationsRequest request, CancellationToken cancellationToken)
		{
			IJob? job = await _jobService.GetJobAsync(jobId, cancellationToken);
			if (job == null)
			{
				return NotFound(jobId);
			}

			StreamConfig? streamConfig;
			if (!_globalConfig.Value.TryGetStream(job.StreamId, out streamConfig))
			{
				return NotFound(job.StreamId);
			}
			if (!streamConfig.Authorize(NotificationAclAction.CreateSubscription, User))
			{
				return Forbid(NotificationAclAction.CreateSubscription, jobId);
			}

			ObjectId triggerId = job.NotificationTriggerId ?? ObjectId.GenerateNewId();

			job = await _jobService.UpdateJobAsync(job, null, null, null, null, triggerId, null, null, cancellationToken: cancellationToken);
			if (job == null)
			{
				return NotFound(jobId);
			}

			await _notificationService.UpdateSubscriptionsAsync(triggerId, User, request.Email, request.Slack, cancellationToken);
			return Ok();
		}

		/// <summary>
		/// Gets information about a specific job.
		/// </summary>
		/// <param name="jobId">Id of the job to find</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about the requested job</returns>
		[HttpGet]
		[Route("/api/v1/jobs/{jobId}/notifications")]
		public async Task<ActionResult<GetNotificationResponse>> GetNotificationsAsync(JobId jobId, CancellationToken cancellationToken)
		{
			IJob? job = await _jobService.GetJobAsync(jobId, cancellationToken);
			if (job == null)
			{
				return NotFound(jobId);
			}

			StreamConfig? streamConfig;
			if (!_globalConfig.Value.TryGetStream(job.StreamId, out streamConfig))
			{
				return NotFound(job.StreamId);
			}
			if (!streamConfig.Authorize(NotificationAclAction.CreateSubscription, User))
			{
				return Forbid(NotificationAclAction.CreateSubscription, jobId);
			}

			INotificationSubscription? subscription;
			if (job.NotificationTriggerId == null)
			{
				subscription = null;
			}
			else
			{
				subscription = await _notificationService.GetSubscriptionsAsync(job.NotificationTriggerId.Value, User, cancellationToken);
			}
			return new GetNotificationResponse(subscription);
		}

		/// <summary>
		/// Gets information about a specific job.
		/// </summary>
		/// <param name="jobId">Id of the job to find</param>
		/// <param name="modifiedAfter">If specified, returns an empty response unless the job's update time is equal to or less than the given value</param>
		/// <param name="filter">Filter for the fields to return</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about the requested job</returns>
		[HttpGet]
		[Route("/api/v1/jobs/{jobId}")]
		[ProducesResponseType(typeof(GetJobResponse), 200)]
		public async Task<ActionResult<object>> GetJobAsync(JobId jobId, [FromQuery] DateTimeOffset? modifiedAfter = null, [FromQuery] PropertyFilter? filter = null, CancellationToken cancellationToken = default)
		{
			IJob? job = await _jobService.GetJobAsync(jobId, cancellationToken);
			if (job == null)
			{
				return NotFound(jobId);
			}

			StreamConfig? streamConfig;
			if (!_globalConfig.Value.TryGetStream(job.StreamId, out streamConfig))
			{
				return NotFound(job.StreamId);
			}
			if (!streamConfig.Authorize(JobAclAction.ViewJob, User))
			{
				return Forbid(JobAclAction.ViewJob, job.StreamId);
			}
			if (modifiedAfter != null && job.UpdateTimeUtc <= modifiedAfter.Value)
			{
				return new Dictionary<string, object>();
			}

			IGraph graph = await _jobService.GetGraphAsync(job, cancellationToken);
			bool includeCosts = streamConfig.Authorize(ServerAclAction.ViewCosts, User);
			return await CreateJobResponseAsync(job, graph, includeCosts, filter, cancellationToken);
		}

		/// <summary>
		/// Creates a response containing information about this object
		/// </summary>
		/// <param name="job">The job document</param>
		/// <param name="graph">The graph for this job</param>
		/// <param name="includeCosts">Whether to include costs in the response</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Object containing the requested properties</returns>
		async Task<object> CreateJobResponseAsync(IJob job, IGraph graph, bool includeCosts, PropertyFilter? filter, CancellationToken cancellationToken)
		{
			if (filter == null)
			{
				return await CreateJobResponseAsync(job, graph, true, true, includeCosts, true, cancellationToken);
			}
			else
			{
				return filter.ApplyTo(await CreateJobResponseAsync(job, graph, filter.Includes(nameof(GetJobResponse.Batches)), filter.Includes(nameof(GetJobResponse.Labels)) || filter.Includes(nameof(GetJobResponse.DefaultLabel)), includeCosts, filter.Includes(nameof(GetJobResponse.Artifacts)), cancellationToken));
			}
		}

		/// <summary>
		/// Creates a response containing information about this object
		/// </summary>
		/// <param name="job">The job document</param>
		/// <param name="graph">The graph definition</param>
		/// <param name="includeBatches">Whether to include the job batches in the response</param>
		/// <param name="includeLabels">Whether to include the job aggregates in the response</param>
		/// <param name="includeCosts">Whether to include costs of running particular agents</param>
		/// <param name="includeArtifacts">Whether to include artifacts in the response</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The response object</returns>
		async ValueTask<GetJobResponse> CreateJobResponseAsync(IJob job, IGraph graph, bool includeBatches, bool includeLabels, bool includeCosts, bool includeArtifacts, CancellationToken cancellationToken)
		{
			GetThinUserInfoResponse? startedByUserInfo = null;
			if (job.StartedByUserId != null)
			{
				startedByUserInfo = (await _userCollection.GetCachedUserAsync(job.StartedByUserId.Value, cancellationToken))?.ToThinApiResponse();
			}

			GetThinUserInfoResponse? abortedByUserInfo = null;
			if (job.AbortedByUserId != null)
			{
				abortedByUserInfo = (await _userCollection.GetCachedUserAsync(job.AbortedByUserId.Value, cancellationToken))?.ToThinApiResponse();
			}

			GetJobResponse response = CreateGetJobResponse(job, startedByUserInfo, abortedByUserInfo);
			if (includeBatches || includeLabels)
			{
				if (includeBatches)
				{
					response.Batches = new List<GetBatchResponse>();
					foreach (IJobStepBatch batch in job.Batches)
					{
						response.Batches.Add(await CreateBatchResponseAsync(batch, includeCosts, cancellationToken));
					}
				}
				if (includeLabels)
				{
					response.Labels = new List<GetLabelStateResponse>();
					response.DefaultLabel = job.GetLabelStateResponses(graph, response.Labels);
				}
			}
			if (includeArtifacts)
			{
				response.Artifacts = new List<GetJobArtifactResponse>();

				HashSet<(ArtifactName, JobStepId)> addedArtifacts = new HashSet<(ArtifactName, JobStepId)>();

				string artifactKey = $"job:{job.Id}";
				string artifactStepKeyPrefix = $"job:{job.Id}/step:";
				await foreach (IArtifact artifact in _artifactCollection.FindAsync(keys: new[] { artifactKey }, cancellationToken: cancellationToken))
				{
					if (IncludeArtifactInResponse(artifact))
					{
						string? stepKey = artifact.Keys.FirstOrDefault(x => x.StartsWith(artifactStepKeyPrefix, StringComparison.Ordinal));
						if (stepKey != null && JobStepId.TryParse(stepKey.Substring(artifactStepKeyPrefix.Length), out JobStepId jobStepId))
						{
							response.Artifacts.Add(new GetJobArtifactResponse(artifact.Id, artifact.Name, artifact.Type, artifact.Description, artifact.Keys.ToList(), jobStepId));
							addedArtifacts.Add((artifact.Name, jobStepId));
						}
					}
				}

				Dictionary<string, IGraphArtifact> outputNameToArtifact = new Dictionary<string, IGraphArtifact>(StringComparer.OrdinalIgnoreCase);
				foreach (IGraphArtifact artifact in graph.Artifacts)
				{
					outputNameToArtifact[artifact.OutputName] = artifact;
				}

				foreach (IJobStepBatch batch in job.Batches)
				{
					INodeGroup group = graph.Groups[batch.GroupIdx];
					foreach (IJobStep step in batch.Steps)
					{
						INode node = group.Nodes[step.NodeIdx];
						foreach (string outputName in node.OutputNames)
						{
							IGraphArtifact? graphArtifact;
							if (outputNameToArtifact.TryGetValue(outputName, out graphArtifact) && !addedArtifacts.Contains((graphArtifact.Name, step.Id)))
							{
								response.Artifacts.Add(new GetJobArtifactResponse(null, graphArtifact.Name, graphArtifact.Type, graphArtifact.Description, graphArtifact.Keys.ToList(), step.Id));
							}
						}
					}
				}
			}
			return response;
		}

		static bool IncludeArtifactInResponse(IArtifact artifact)
		{
			return artifact.Type != ArtifactType.StepOutput
				&& artifact.Type != ArtifactType.StepSaved
				&& artifact.Type != ArtifactType.StepTrace
				&& artifact.Type != ArtifactType.StepTestData;
		}

		static GetJobResponse CreateGetJobResponse(IJob job, GetThinUserInfoResponse? startedByUserInfo, GetThinUserInfoResponse? abortedByUserInfo)
		{
			GetJobResponse response = new GetJobResponse(job.Id, job.StreamId, job.TemplateId, job.Name);
			response.Change = job.Change;
			response.CodeChange = (job.CodeChange != 0) ? (int?)job.CodeChange : null;
			response.PreflightChange = (job.PreflightChange != 0) ? (int?)job.PreflightChange : null;
			response.ClonedPreflightChange = (job.ClonedPreflightChange != 0) ? (int?)job.ClonedPreflightChange : null;
			response.PreflightDescription = job.PreflightDescription;
			response.TemplateHash = job.TemplateHash?.ToString() ?? String.Empty;
			response.GraphHash = job.GraphHash.ToString();
			response.StartedByUserId = job.StartedByUserId?.ToString();
			response.StartedByUser = startedByUserInfo?.Login;
			response.StartedByUserInfo = startedByUserInfo;
			response.StartedByBisectTaskId = job.StartedByBisectTaskId;
			response.AbortedByUser = abortedByUserInfo?.Login;
			response.AbortedByUserInfo = abortedByUserInfo;
			response.CreateTime = new DateTimeOffset(job.CreateTimeUtc);
			response.State = job.GetState();
			response.Priority = job.Priority;
			response.AutoSubmit = job.AutoSubmit;
			response.AutoSubmitChange = job.AutoSubmitChange;
			response.AutoSubmitMessage = job.AutoSubmitMessage;
			response.Reports = job.Reports?.ConvertAll(x => CreateGetReportResponse(x));
			response.Parameters = job.Parameters.ToDictionary();
			response.Arguments = job.Arguments.ToList();
			response.UpdateTime = new DateTimeOffset(job.UpdateTimeUtc);
			response.UseArtifactsV2 = job.JobOptions?.UseNewTempStorage ?? true;
			response.UpdateIssues = job.UpdateIssues;
			return response;
		}

		static GetReportResponse CreateGetReportResponse(IReport report)
		{
			GetReportResponse response = new GetReportResponse(report.Name, report.Placement);
			response.ArtifactId = report.ArtifactId?.ToString();
			response.Content = report.Content;
			return response;
		}

		/// <summary>
		/// Get the response object for a batch
		/// </summary>
		/// <param name="batch"></param>
		/// <param name="includeCosts"></param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		async ValueTask<GetBatchResponse> CreateBatchResponseAsync(IJobStepBatch batch, bool includeCosts, CancellationToken cancellationToken)
		{
			List<GetStepResponse> steps = new List<GetStepResponse>();
			foreach (IJobStep step in batch.Steps)
			{
				steps.Add(await CreateStepResponseAsync(step, cancellationToken));
			}

			double? agentRate = null;
			if (batch.AgentId != null && includeCosts)
			{
				agentRate = await _agentService.GetRateAsync(batch.AgentId.Value, cancellationToken);
			}

			return CreateGetBatchResponse(batch, steps, agentRate);
		}

		/// <summary>
		/// Converts this batch into a public response object
		/// </summary>
		/// <param name="batch">The batch to construct from</param>
		/// <param name="steps">Steps in this batch</param>
		/// <param name="agentRate">Rate for this agent</param>
		/// <returns>Response instance</returns>
		static GetBatchResponse CreateGetBatchResponse(IJobStepBatch batch, List<GetStepResponse> steps, double? agentRate)
		{
			GetBatchResponse response = new GetBatchResponse();
			response.Id = batch.Id;
			response.LogId = batch.LogId?.ToString();
			response.GroupIdx = batch.GroupIdx;
			response.State = batch.State;
			response.Error = batch.Error;
			response.Steps.AddRange(steps);
			response.AgentId = batch.AgentId?.ToString();
			response.AgentRate = agentRate;
			response.SessionId = batch.SessionId?.ToString();
			response.LeaseId = batch.LeaseId?.ToString();
			response.WeightedPriority = batch.SchedulePriority;
			response.StartTime = batch.StartTimeUtc;
			response.FinishTime = batch.FinishTimeUtc;
			response.ReadyTime = batch.ReadyTimeUtc;
			return response;
		}

		/// <summary>
		/// Get the response object for a step
		/// </summary>
		/// <param name="step"></param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		async ValueTask<GetStepResponse> CreateStepResponseAsync(IJobStep step, CancellationToken cancellationToken)
		{
			GetThinUserInfoResponse? abortedByUserInfo = null;
			if (step.AbortedByUserId != null)
			{
				abortedByUserInfo = (await _userCollection.GetCachedUserAsync(step.AbortedByUserId.Value, cancellationToken))?.ToThinApiResponse();
			}

			GetThinUserInfoResponse? retriedByUserInfo = null;
			if (step.RetriedByUserId != null)
			{
				retriedByUserInfo = (await _userCollection.GetCachedUserAsync(step.RetriedByUserId.Value, cancellationToken))?.ToThinApiResponse();
			}

			return CreateGetStepResponse(step, abortedByUserInfo, retriedByUserInfo);
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="step">The step to construct from</param>
		/// <param name="abortedByUserInfo">User that aborted this step</param>
		/// <param name="retriedByUserInfo">User that retried this step</param>
		static GetStepResponse CreateGetStepResponse(IJobStep step, GetThinUserInfoResponse? abortedByUserInfo, GetThinUserInfoResponse? retriedByUserInfo)
		{
			GetStepResponse response = new GetStepResponse();
			response.Id = step.Id;
			response.NodeIdx = step.NodeIdx;
			response.State = step.State;
			response.Outcome = step.Outcome;
			response.Error = step.Error;
			response.AbortRequested = step.AbortRequested;
			response.AbortByUser = abortedByUserInfo?.Login;
			response.AbortedByUserInfo = abortedByUserInfo;
			response.RetryByUser = retriedByUserInfo?.Login;
			response.RetriedByUserInfo = retriedByUserInfo;
			response.LogId = step.LogId?.ToString();
			response.ReadyTime = step.ReadyTimeUtc;
			response.StartTime = step.StartTimeUtc;
			response.FinishTime = step.FinishTimeUtc;
			response.Reports = step.Reports?.ConvertAll(x => CreateGetReportResponse(x));

			if (step.Properties != null && step.Properties.Count > 0)
			{
				response.Properties = step.Properties;
			}
			return response;
		}

		/// <summary>
		/// Gets information about the graph for a specific job.
		/// </summary>
		/// <param name="jobId">Id of the job to find</param>
		/// <param name="filter">Filter for the fields to return</param>
		/// <returns>Information about the requested job</returns>
		[HttpGet]
		[Route("/api/v1/jobs/{jobId}/graph")]
		[ProducesResponseType(typeof(GetGraphResponse), 200)]
		public async Task<ActionResult<object>> GetJobGraphAsync(JobId jobId, [FromQuery] PropertyFilter? filter = null)
		{
			IJob? job = await _jobService.GetJobAsync(jobId);
			if (job == null)
			{
				return NotFound(jobId);
			}

			StreamConfig? streamConfig;
			if (!_globalConfig.Value.TryGetStream(job.StreamId, out streamConfig))
			{
				return NotFound(job.StreamId);
			}
			if (!streamConfig.Authorize(JobAclAction.ViewJob, User))
			{
				return Forbid(JobAclAction.ViewJob, jobId);
			}

			IGraph graph = await _jobService.GetGraphAsync(job);
			return PropertyFilter.Apply(CreateGetGraphResponse(graph), filter);
		}

		static GetGraphResponse CreateGetGraphResponse(IGraph graph)
		{
			GetGraphResponse response = new GetGraphResponse(graph.Id.ToString());
			if (graph.Groups.Count > 0)
			{
				response.Groups = graph.Groups.ConvertAll(x => CreateGetGroupResponse(x, graph.Groups));
			}
			if (graph.Aggregates.Count > 0)
			{
				response.Aggregates = graph.Aggregates.ConvertAll(x => CreateGetAggregateResponse(x, graph.Groups));
			}
			if (graph.Labels.Count > 0)
			{
				response.Labels = graph.Labels.ConvertAll(x => CreateGetLabelResponse(x, graph.Groups));
			}
			return response;
		}

		static GetGroupResponse CreateGetGroupResponse(INodeGroup group, IReadOnlyList<INodeGroup> groups)
		{
			GetGroupResponse response = new GetGroupResponse(group.AgentType);
			response.Nodes.AddRange(group.Nodes.Select(x => CreateGetNodeResponse(x, groups)));
			return response;
		}

		static GetNodeResponse CreateGetNodeResponse(INode node, IReadOnlyList<INodeGroup> groups)
		{
			GetNodeResponse response = new GetNodeResponse(node.Name);
			response.Inputs.AddRange(node.Inputs.Select(x => groups[x.NodeRef.GroupIdx].Nodes[x.NodeRef.NodeIdx].OutputNames[x.OutputIdx]));
			response.Outputs.AddRange(node.OutputNames);
			response.InputDependencies.AddRange(node.InputDependencies.Select(x => groups[x.GroupIdx].Nodes[x.NodeIdx].Name));
			response.OrderDependencies.AddRange(node.OrderDependencies.Select(x => groups[x.GroupIdx].Nodes[x.NodeIdx].Name));
			response.Priority = node.Priority;
			response.AllowRetry = node.AllowRetry;
			response.RunEarly = node.RunEarly;
			response.Warnings = node.Warnings;
			response.Credentials = node.Credentials;
			response.Properties = node.Properties;
			response.Annotations = node.Annotations;
			return response;
		}

		static GetAggregateResponse CreateGetAggregateResponse(IAggregate aggregate, IReadOnlyList<INodeGroup> groups)
		{
			GetAggregateResponse response = new GetAggregateResponse(aggregate.Name);
			response.Nodes = aggregate.Nodes.ConvertAll(x => x.ToNode(groups).Name);
			return response;
		}

		static GetLabelResponse CreateGetLabelResponse(ILabel label, IReadOnlyList<INodeGroup> groups)
		{
			GetLabelResponse response = new GetLabelResponse();
			response.DashboardName = label.DashboardName;
			response.DashboardCategory = label.DashboardCategory;
			response.UgsName = label.UgsName;
			response.UgsProject = label.UgsProject;
			response.RequiredNodes = label.RequiredNodes.ConvertAll(x => x.ToNode(groups).Name);
			response.IncludedNodes = label.IncludedNodes.ConvertAll(x => x.ToNode(groups).Name);
			return response;
		}

		/// <summary>
		/// Gets timing information about the graph for a specific job.
		/// </summary>
		/// <param name="jobId">Id of the job to find</param>
		/// <param name="filter">Filter for the fields to return</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about the requested job</returns>
		[HttpGet]
		[Route("/api/v1/jobs/{jobId}/timing")]
		[ProducesResponseType(typeof(GetJobTimingResponse), 200)]
		public async Task<ActionResult<object>> GetJobTimingAsync(JobId jobId, [FromQuery] PropertyFilter? filter = null, CancellationToken cancellationToken = default)
		{
			IJob? job = await _jobService.GetJobAsync(jobId, cancellationToken);
			if (job == null)
			{
				return NotFound(jobId);
			}

			StreamConfig? streamConfig;
			if (!_globalConfig.Value.TryGetStream(job.StreamId, out streamConfig))
			{
				return NotFound(job.StreamId);
			}
			if (!streamConfig.Authorize(JobAclAction.ViewJob, User))
			{
				return Forbid(JobAclAction.ViewJob, jobId);
			}

			IJobTiming jobTiming = await _jobService.GetJobTimingAsync(job, cancellationToken);
			IGraph graph = await _jobService.GetGraphAsync(job, cancellationToken);
			return PropertyFilter.Apply(await CreateJobTimingResponseAsync(job, graph, jobTiming, cancellationToken: cancellationToken), filter);
		}

		private async Task<GetJobTimingResponse> CreateJobTimingResponseAsync(IJob job, IGraph graph, IJobTiming jobTiming, bool includeJobResponse = false, CancellationToken cancellationToken = default)
		{
			Dictionary<INode, TimingInfo> nodeToTimingInfo = job.GetTimingInfo(graph, jobTiming);

			Dictionary<string, GetStepTimingInfoResponse> steps = new Dictionary<string, GetStepTimingInfoResponse>();
			foreach (IJobStepBatch batch in job.Batches)
			{
				foreach (IJobStep step in batch.Steps)
				{
					INode node = graph.Groups[batch.GroupIdx].Nodes[step.NodeIdx];
					steps[step.Id.ToString()] = CreateGetStepTimingInfoResponse(node.Name, nodeToTimingInfo[node]);
				}
			}

			List<GetLabelTimingInfoResponse> labels = new List<GetLabelTimingInfoResponse>();
			foreach (ILabel label in graph.Labels)
			{
				TimingInfo timingInfo = TimingInfo.Max(label.GetDependencies(graph.Groups).Select(x => nodeToTimingInfo[x]));
				labels.Add(CreateGetLabelTimingInfoResponse(label, timingInfo));
			}

			GetJobResponse? jobResponse = null;
			if (includeJobResponse)
			{
				jobResponse = await CreateJobResponseAsync(job, graph, true, true, true, true, cancellationToken);
			}

			return new GetJobTimingResponse(jobResponse, steps, labels);
		}

		static GetLabelTimingInfoResponse CreateGetLabelTimingInfoResponse(ILabel label, TimingInfo timingInfo)
		{
			GetLabelTimingInfoResponse response = new GetLabelTimingInfoResponse();
			timingInfo.CopyToResponse(response);

			response.DashboardName = label.DashboardName;
			response.DashboardCategory = label.DashboardCategory;
			response.UgsName = label.UgsName;
			response.UgsProject = label.UgsProject;
			return response;
		}

		static GetStepTimingInfoResponse CreateGetStepTimingInfoResponse(string? name, TimingInfo jobTimingInfo)
		{
			GetStepTimingInfoResponse response = new GetStepTimingInfoResponse();
			jobTimingInfo.CopyToResponse(response);

			response.Name = name;
			response.AverageStepWaitTime = jobTimingInfo.StepTiming?.AverageWaitTime;
			response.AverageStepInitTime = jobTimingInfo.StepTiming?.AverageInitTime;
			response.AverageStepDuration = jobTimingInfo.StepTiming?.AverageDuration;
			return response;
		}

		/// <summary>
		/// Find timing information about the graph for multiple jobs
		/// </summary>
		/// <param name="streamId">The stream to search in</param>
		/// <param name="templates">List of templates to find</param>
		/// <param name="filter">Filter for the fields to return</param>
		/// <param name="count">Number of results to return</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Job timings for each job ID</returns>
		[HttpGet]
		[Route("/api/v1/jobs/timing")]
		[ProducesResponseType(typeof(FindJobTimingsResponse), 200)]
		public async Task<ActionResult<object>> FindJobTimingsAsync(
			[FromQuery] string? streamId = null,
			[FromQuery(Name = "template")] string[]? templates = null,
			[FromQuery] PropertyFilter? filter = null,
			[FromQuery] int count = 100,
			CancellationToken cancellationToken = default)
		{
			if (streamId == null)
			{
				return BadRequest("Missing/invalid query parameter streamId");
			}

			TemplateId[] templateRefIds = templates switch
			{
				{ Length: > 0 } => templates.Select(x => new TemplateId(x)).ToArray(),
				_ => Array.Empty<TemplateId>()
			};

			IReadOnlyList<IJob> jobs = await _jobService.FindJobsByStreamWithTemplatesAsync(new StreamId(streamId), templateRefIds, count: count, consistentRead: false, cancellationToken: cancellationToken);

			Dictionary<string, GetJobTimingResponse> jobTimings = await jobs.ToAsyncEnumerable()
				.Where(job => _globalConfig.Value.Authorize(job, JobAclAction.ViewJob, User))
				.ToDictionaryAwaitAsync(x => ValueTask.FromResult(x.Id.ToString()), async job =>
				{
					IJobTiming jobTiming = await _jobService.GetJobTimingAsync(job, cancellationToken);
					IGraph graph = await _jobService.GetGraphAsync(job);
					return await CreateJobTimingResponseAsync(job, graph, jobTiming, true);
				}, cancellationToken);

			return PropertyFilter.Apply(new FindJobTimingsResponse(jobTimings), filter);
		}

		/// <summary>
		/// Gets information about the template for a specific job.
		/// </summary>
		/// <param name="jobId">Id of the job to find</param>
		/// <param name="filter">Filter for the fields to return</param>
		/// <returns>Information about the requested job</returns>
		[HttpGet]
		[Route("/api/v1/jobs/{jobId}/template")]
		[ProducesResponseType(typeof(GetTemplateResponse), 200)]
		public async Task<ActionResult<object>> GetJobTemplateAsync(JobId jobId, [FromQuery] PropertyFilter? filter = null)
		{
			IJob? job = await _jobService.GetJobAsync(jobId);
			if (job == null || job.TemplateHash == null)
			{
				return NotFound(jobId);
			}

			StreamConfig? streamConfig;
			if (!_globalConfig.Value.TryGetStream(job.StreamId, out streamConfig))
			{
				return NotFound(job.StreamId);
			}
			if (!streamConfig.Authorize(JobAclAction.ViewJob, User))
			{
				return Forbid(JobAclAction.ViewJob, jobId);
			}

			ITemplate? template = await _templateCollection.GetAsync(job.TemplateHash);
			if (template == null)
			{
				return NotFound(job.StreamId, job.TemplateId);
			}

			return new GetTemplateResponse(template).ApplyFilter(filter);
		}

		/// <summary>
		/// Find jobs matching a criteria
		/// </summary>
		/// <param name="ids">The job ids to return</param>
		/// <param name="name">Name of the job to find</param>
		/// <param name="templates">List of templates to find</param>
		/// <param name="streamId">The stream to search for</param>
		/// <param name="minChange">The minimum changelist number</param>
		/// <param name="maxChange">The maximum changelist number</param>
		/// <param name="includePreflight">Whether to include preflight jobs</param>
		/// <param name="preflightOnly">Whether to only include preflight jobs</param>
		/// <param name="preflightChange">The preflighted changelist</param>
		/// <param name="startedByUserId">User id for which to include jobs</param>
		/// <param name="preflightStartedByUserId">User id for which to include preflight jobs</param>
		/// <param name="minCreateTime">Minimum creation time</param>
		/// <param name="maxCreateTime">Maximum creation time</param>
		/// <param name="modifiedBefore">If specified, only jobs updated before the give time will be returned</param>
		/// <param name="modifiedAfter">If specified, only jobs updated after the give time will be returned</param>
		/// <param name="target">Target to filter the returned jobs by</param>
		/// <param name="state">Filter state of the returned jobs</param>
		/// <param name="outcome">Filter outcome of the returned jobs</param>
		/// <param name="filter">Filter for properties to return</param>
		/// <param name="index">Index of the first result to be returned</param>
		/// <param name="count">Number of results to return</param>
		/// <returns>List of jobs</returns>
		[HttpGet]
		[Route("/api/v1/jobs")]
		[ProducesResponseType(typeof(List<GetJobResponse>), 200)]
		public async Task<ActionResult<List<object>>> FindJobsAsync(
			[FromQuery(Name = "Id")] string[]? ids = null,
			[FromQuery] string? streamId = null,
			[FromQuery] string? name = null,
			[FromQuery(Name = "template")] string[]? templates = null,
			[FromQuery] int? minChange = null,
			[FromQuery] int? maxChange = null,
			[FromQuery] bool includePreflight = true,
			[FromQuery] bool? preflightOnly = null,
			[FromQuery] int? preflightChange = null,
			[FromQuery] string? preflightStartedByUserId = null,
			[FromQuery] string? startedByUserId = null,
			[FromQuery] DateTimeOffset? minCreateTime = null,
			[FromQuery] DateTimeOffset? maxCreateTime = null,
			[FromQuery] DateTimeOffset? modifiedBefore = null,
			[FromQuery] DateTimeOffset? modifiedAfter = null,
			[FromQuery] string? target = null,
			[FromQuery] JobStepState[]? state = null,
			[FromQuery] JobStepOutcome[]? outcome = null,
			[FromQuery] PropertyFilter? filter = null,
			[FromQuery] int index = 0,
			[FromQuery] int count = 100)
		{
			JobId[]? jobIdValues = (ids == null) ? (JobId[]?)null : Array.ConvertAll(ids, x => JobId.Parse(x));
			StreamId? streamIdValue = (streamId == null) ? (StreamId?)null : new StreamId(streamId);

			TemplateId[]? templateRefIds = (templates != null && templates.Length > 0) ? templates.Select(x => new TemplateId(x)).ToArray() : null;

			if (includePreflight == false)
			{
				preflightChange = 0;
			}

			UserId? preflightStartedByUserIdValue = null;

			if (preflightStartedByUserId != null)
			{
				preflightStartedByUserIdValue = UserId.Parse(preflightStartedByUserId);
			}

			UserId? startedByUserIdValue = null;

			if (startedByUserId != null)
			{
				startedByUserIdValue = UserId.Parse(startedByUserId);
			}

			IReadOnlyList<IJob> jobs;
			jobs = await _jobService.FindJobsAsync(jobIdValues, streamIdValue, name, templateRefIds, minChange,
				maxChange, preflightChange, preflightOnly, preflightStartedByUserIdValue, startedByUserIdValue, minCreateTime?.UtcDateTime, maxCreateTime?.UtcDateTime, target, null, state, outcome,
				modifiedBefore, modifiedAfter, index, count, false);

			return await CreateAuthorizedJobResponsesAsync(jobs, filter);
		}

		/// <summary>
		/// Find jobs for a stream with given templates, sorted by creation date
		/// </summary>
		/// <param name="streamId">The stream to search for</param>
		/// <param name="templates">List of templates to find</param>
		/// <param name="preflightStartedByUserId">User id for which to include preflight jobs</param>
		/// <param name="maxCreateTime">Maximum creation time</param>
		/// <param name="modifiedAfter">If specified, only jobs updated after the given time will be returned</param>
		/// <param name="filter">Filter for properties to return</param>
		/// <param name="index">Index of the first result to be returned</param>
		/// <param name="count">Number of results to return</param>
		/// <param name="consistentRead">If a read to the primary database is required, for read consistency. Usually not required.</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of jobs</returns>
		[HttpGet]
		[Route("/api/v1/jobs/streams/{streamId}")]
		[ProducesResponseType(typeof(List<GetJobResponse>), 200)]
		public async Task<ActionResult<List<object>>> FindJobsByStreamWithTemplatesAsync(
			string streamId,
			[FromQuery(Name = "template")] string[] templates,
			[FromQuery] string? preflightStartedByUserId = null,
			[FromQuery] DateTimeOffset? maxCreateTime = null,
			[FromQuery] DateTimeOffset? modifiedAfter = null,
			[FromQuery] PropertyFilter? filter = null,
			[FromQuery] int index = 0,
			[FromQuery] int count = 100,
			[FromQuery] bool consistentRead = false,
			CancellationToken cancellationToken = default)
		{
			StreamId streamIdValue = new StreamId(streamId);
			TemplateId[] templateRefIds = templates.Select(x => new TemplateId(x)).ToArray();
			UserId? preflightStartedByUserIdValue = preflightStartedByUserId != null ? UserId.Parse(preflightStartedByUserId) : null;
			count = Math.Min(1000, count);

			IReadOnlyList<IJob> jobs = await _jobService.FindJobsByStreamWithTemplatesAsync(streamIdValue, templateRefIds, preflightStartedByUserIdValue, maxCreateTime, modifiedAfter, index, count, consistentRead, cancellationToken: cancellationToken);
			return await CreateAuthorizedJobResponsesAsync(jobs, filter, cancellationToken);
		}

		private async Task<List<object>> CreateAuthorizedJobResponsesAsync(IReadOnlyList<IJob> jobs, PropertyFilter? filter = null, CancellationToken cancellationToken = default)
		{
			List<object> responses = new();
			foreach (IGrouping<StreamId, IJob> grouping in jobs.GroupBy(x => x.StreamId))
			{
				StreamConfig? streamConfig;
				if (_globalConfig.Value.TryGetStream(grouping.Key, out streamConfig) && streamConfig.Authorize(JobAclAction.ViewJob, User))
				{
					bool includeCosts = streamConfig.Authorize(ServerAclAction.ViewCosts, User);
					foreach (IJob job in grouping)
					{
						IGraph graph = await _jobService.GetGraphAsync(job, cancellationToken);
						responses.Add(await CreateJobResponseAsync(job, graph, includeCosts, filter, cancellationToken));
					}
				}
			}
			return responses;
		}

		/// <summary>
		/// Adds an array of nodes to be executed for a job
		/// </summary>
		/// <param name="jobId">Unique id for the job</param>
		/// <param name="requests">Properties of the new nodes</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Id of the new job</returns>
		[HttpPost]
		[Obsolete("Modifying graph through REST API is not supported")]
		[Route("/api/v1/jobs/{jobId}/groups")]
		public async Task<ActionResult> CreateGroupsAsync(JobId jobId, [FromBody] List<NewGroup> requests, CancellationToken cancellationToken = default)
		{
			for (; ; )
			{
				IJob? job = await _jobService.GetJobAsync(jobId, cancellationToken);
				if (job == null)
				{
					return NotFound(jobId);
				}

				StreamConfig? streamConfig;
				if (!_globalConfig.Value.TryGetStream(job.StreamId, out streamConfig))
				{
					return NotFound(job.StreamId);
				}
				if (!streamConfig.Authorize(JobAclAction.ExecuteJob, User))
				{
					return Forbid(JobAclAction.ExecuteJob, jobId);
				}

				IGraph oldGraph = await _jobService.GetGraphAsync(job, cancellationToken);
				IGraph newGraph = await _graphs.AppendAsync(oldGraph, requests, null, null, null, cancellationToken);

				IJob? newJob = await _jobService.TryUpdateGraphAsync(job, oldGraph, newGraph, cancellationToken);
				if (newJob != null)
				{
					return Ok();
				}
			}
		}

		/// <summary>
		/// Gets the nodes to be executed for a job
		/// </summary>
		/// <param name="jobId">Unique id for the job</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of nodes to be executed</returns>
		[HttpGet]
		[Obsolete("Query entire job instead")]
		[Route("/api/v1/jobs/{jobId}/groups")]
		[ProducesResponseType(typeof(List<GetGroupResponse>), 200)]
		public async Task<ActionResult<List<object>>> GetGroupsAsync(JobId jobId, [FromQuery] PropertyFilter? filter = null, CancellationToken cancellationToken = default)
		{
			IJob? job = await _jobService.GetJobAsync(jobId, cancellationToken);
			if (job == null)
			{
				return NotFound(jobId);
			}

			StreamConfig? streamConfig;
			if (!_globalConfig.Value.TryGetStream(job.StreamId, out streamConfig))
			{
				return NotFound(job.StreamId);
			}
			if (!streamConfig.Authorize(JobAclAction.ViewJob, User))
			{
				return Forbid(JobAclAction.ViewJob, jobId);
			}

			IGraph graph = await _jobService.GetGraphAsync(job, cancellationToken);
			return graph.Groups.ConvertAll(x => CreateGetGroupResponse(x, graph.Groups).ApplyFilter(filter));
		}

		/// <summary>
		/// Gets the nodes in a group to be executed for a job
		/// </summary>
		/// <param name="jobId">Unique id for the job</param>
		/// <param name="groupIdx">The group index</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of nodes to be executed</returns>
		[HttpGet]
		[Obsolete("Query entire job instead")]
		[Route("/api/v1/jobs/{jobId}/groups/{groupIdx}")]
		[ProducesResponseType(typeof(GetGroupResponse), 200)]
		public async Task<ActionResult<object>> GetGroupAsync(JobId jobId, int groupIdx, [FromQuery] PropertyFilter? filter = null, CancellationToken cancellationToken = default)
		{
			IJob? job = await _jobService.GetJobAsync(jobId, cancellationToken);
			if (job == null)
			{
				return NotFound(jobId);
			}

			StreamConfig? streamConfig;
			if (!_globalConfig.Value.TryGetStream(job.StreamId, out streamConfig))
			{
				return NotFound(job.StreamId);
			}
			if (!streamConfig.Authorize(JobAclAction.ViewJob, User))
			{
				return Forbid(JobAclAction.ViewJob, jobId);
			}

			IGraph graph = await _jobService.GetGraphAsync(job, cancellationToken);
			if (groupIdx < 0 || groupIdx >= graph.Groups.Count)
			{
				return NotFound(jobId, groupIdx);
			}

			return CreateGetGroupResponse(graph.Groups[groupIdx], graph.Groups).ApplyFilter(filter);
		}

		/// <summary>
		/// Gets the nodes for a particular group
		/// </summary>
		/// <param name="jobId">Unique id for the job</param>
		/// <param name="groupIdx">Index of the group containing the node to update</param>
		/// <param name="filter">Filter for the properties to return</param>
		[HttpGet]
		[Obsolete("Query entire job instead")]
		[Route("/api/v1/jobs/{jobId}/groups/{groupIdx}/nodes")]
		[ProducesResponseType(typeof(List<GetNodeResponse>), 200)]
		public async Task<ActionResult<List<object>>> GetNodesAsync(JobId jobId, int groupIdx, [FromQuery] PropertyFilter? filter = null)
		{
			IJob? job = await _jobService.GetJobAsync(jobId);
			if (job == null)
			{
				return NotFound(jobId);
			}

			StreamConfig? streamConfig;
			if (!_globalConfig.Value.TryGetStream(job.StreamId, out streamConfig))
			{
				return NotFound(job.StreamId);
			}
			if (!streamConfig.Authorize(JobAclAction.ViewJob, User))
			{
				return Forbid(JobAclAction.ViewJob, jobId);
			}

			IGraph graph = await _jobService.GetGraphAsync(job);
			if (groupIdx < 0 || groupIdx >= graph.Groups.Count)
			{
				return NotFound(jobId, groupIdx);
			}

			return graph.Groups[groupIdx].Nodes.ConvertAll(x => CreateGetNodeResponse(x, graph.Groups).ApplyFilter(filter));
		}

		/// <summary>
		/// Gets a particular node definition
		/// </summary>
		/// <param name="jobId">Unique id for the job</param>
		/// <param name="groupIdx">Index of the group containing the node to update</param>
		/// <param name="nodeIdx">Index of the node to update</param>
		/// <param name="filter">Filter for the properties to return</param>
		[HttpGet]
		[Obsolete("Query entire job instead")]
		[Route("/api/v1/jobs/{jobId}/groups/{groupIdx}/nodes/{nodeIdx}")]
		[ProducesResponseType(typeof(GetNodeResponse), 200)]
		public async Task<ActionResult<object>> GetNodeAsync(JobId jobId, int groupIdx, int nodeIdx, [FromQuery] PropertyFilter? filter = null)
		{
			IJob? job = await _jobService.GetJobAsync(jobId);
			if (job == null)
			{
				return NotFound(jobId);
			}

			StreamConfig? streamConfig;
			if (!_globalConfig.Value.TryGetStream(job.StreamId, out streamConfig))
			{
				return NotFound(job.StreamId);
			}
			if (!streamConfig.Authorize(JobAclAction.ViewJob, User))
			{
				return Forbid(JobAclAction.ViewJob, jobId);
			}

			IGraph graph = await _jobService.GetGraphAsync(job);
			if (groupIdx < 0 || groupIdx >= graph.Groups.Count)
			{
				return NotFound(jobId, groupIdx);
			}
			if (nodeIdx < 0 || nodeIdx >= graph.Groups[groupIdx].Nodes.Count)
			{
				return NotFound(jobId, groupIdx, nodeIdx);
			}

			return CreateGetNodeResponse(graph.Groups[groupIdx].Nodes[nodeIdx], graph.Groups).ApplyFilter(filter);
		}

		/// <summary>
		/// Gets the steps currently scheduled to be executed for a job
		/// </summary>
		/// <param name="jobId">Unique id for the job</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of nodes to be executed</returns>
		[HttpGet]
		[Obsolete("Query entire job instead")]
		[Route("/api/v1/jobs/{jobId}/batches")]
		[ProducesResponseType(typeof(List<GetBatchResponse>), 200)]
		public async Task<ActionResult<List<object>>> GetBatchesAsync(JobId jobId, [FromQuery] PropertyFilter? filter = null, CancellationToken cancellationToken = default)
		{
			IJob? job = await _jobService.GetJobAsync(jobId, cancellationToken);
			if (job == null)
			{
				return NotFound(jobId);
			}

			StreamConfig? streamConfig;
			if (!_globalConfig.Value.TryGetStream(job.StreamId, out streamConfig))
			{
				return NotFound(job.StreamId);
			}
			if (!streamConfig.Authorize(JobAclAction.ViewJob, User))
			{
				return Forbid(JobAclAction.ViewJob, job.StreamId);
			}

			bool includeCosts = streamConfig.Authorize(ServerAclAction.ViewCosts, User);

			List<object> responses = new List<object>();
			foreach (IJobStepBatch batch in job.Batches)
			{
				GetBatchResponse response = await CreateBatchResponseAsync(batch, includeCosts, cancellationToken);
				responses.Add(response.ApplyFilter(filter));
			}
			return responses;
		}

		/// <summary>
		/// Updates the state of a jobstep
		/// </summary>
		/// <param name="jobId">Unique id for the job</param>
		/// <param name="batchId">Unique id for the step</param>
		/// <param name="request">Updates to apply to the node</param>
		[HttpPut]
		[Obsolete("Query entire job instead")]
		[Route("/api/v1/jobs/{jobId}/batches/{batchId}")]
		public async Task<ActionResult> UpdateBatchAsync(JobId jobId, JobStepBatchId batchId, [FromBody] UpdateBatchRequest request)
		{
			IJob? job = await _jobService.GetJobAsync(jobId);
			if (job == null)
			{
				return NotFound(jobId);
			}
			if (!_globalConfig.Value.TryGetStream(job.StreamId, out StreamConfig? streamConfig))
			{
				return NotFound(job.StreamId);
			}

			IJobStepBatch? batch = job.Batches.FirstOrDefault(x => x.Id == batchId);
			if (batch == null)
			{
				return NotFound(jobId, batchId);
			}
			if (batch.SessionId == null || !User.HasSessionClaim(batch.SessionId.Value))
			{
				return Forbid("Missing session claim for job {JobId} batch {BatchId}", jobId, batchId);
			}

			IJob? newJob = await _jobService.UpdateBatchAsync(job, batchId, streamConfig, (request.LogId == null) ? null : LogId.Parse(request.LogId), request.State);
			if (newJob == null)
			{
				return NotFound(jobId);
			}
			return Ok();
		}

		/// <summary>
		/// Gets a particular step currently scheduled to be executed for a job
		/// </summary>
		/// <param name="jobId">Unique id for the job</param>
		/// <param name="batchId">Unique id for the step</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of nodes to be executed</returns>
		[HttpGet]
		[Obsolete("Query entire job instead")]
		[Route("/api/v1/jobs/{jobId}/batches/{batchId}")]
		[ProducesResponseType(typeof(GetBatchResponse), 200)]
		public async Task<ActionResult<object>> GetBatchAsync(JobId jobId, JobStepBatchId batchId, [FromQuery] PropertyFilter? filter = null, CancellationToken cancellationToken = default)
		{
			IJob? job = await _jobService.GetJobAsync(jobId, cancellationToken);
			if (job == null)
			{
				return NotFound(jobId);
			}

			StreamConfig? streamConfig;
			if (!_globalConfig.Value.TryGetStream(job.StreamId, out streamConfig))
			{
				return NotFound(job.StreamId);
			}
			if (!streamConfig.Authorize(JobAclAction.ViewJob, User))
			{
				return Forbid(JobAclAction.ViewJob, job.StreamId);
			}

			bool includeCosts = streamConfig.Authorize(ServerAclAction.ViewCosts, User);
			foreach (IJobStepBatch batch in job.Batches)
			{
				if (batch.Id == batchId)
				{
					GetBatchResponse response = await CreateBatchResponseAsync(batch, includeCosts, cancellationToken);
					return response.ApplyFilter(filter);
				}
			}

			return NotFound(jobId, batchId);
		}

		/// <summary>
		/// Gets the steps currently scheduled to be executed for a job
		/// </summary>
		/// <param name="jobId">Unique id for the job</param>
		/// <param name="batchId">Unique id for the batch</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of nodes to be executed</returns>
		[HttpGet]
		[Obsolete("Query entire job instead")]
		[Route("/api/v1/jobs/{jobId}/batches/{batchId}/steps")]
		[ProducesResponseType(typeof(List<GetStepResponse>), 200)]
		public async Task<ActionResult<List<object>>> GetStepsAsync(JobId jobId, JobStepBatchId batchId, [FromQuery] PropertyFilter? filter = null, CancellationToken cancellationToken = default)
		{
			IJob? job = await _jobService.GetJobAsync(jobId, cancellationToken);
			if (job == null)
			{
				return NotFound(jobId);
			}

			StreamConfig? streamConfig;
			if (!_globalConfig.Value.TryGetStream(job.StreamId, out streamConfig))
			{
				return NotFound(job.StreamId);
			}
			if (!streamConfig.Authorize(JobAclAction.ViewJob, User))
			{
				return Forbid(JobAclAction.ViewJob, jobId);
			}

			foreach (IJobStepBatch batch in job.Batches)
			{
				if (batch.Id == batchId)
				{
					List<object> responses = new List<object>();
					foreach (IJobStep step in batch.Steps)
					{
						GetStepResponse response = await CreateStepResponseAsync(step, cancellationToken);
						responses.Add(response.ApplyFilter(filter));
					}
					return responses;
				}
			}

			return NotFound(jobId, batchId);
		}

		/// <summary>
		/// Updates the state of a jobstep
		/// </summary>
		/// <param name="jobId">Unique id for the job</param>
		/// <param name="batchId">Unique id for the batch</param>
		/// <param name="stepId">Unique id for the step</param>
		/// <param name="request">Updates to apply to the node</param>
		[HttpPut]
		[Route("/api/v1/jobs/{jobId}/batches/{batchId}/steps/{stepId}")]
		public async Task<ActionResult<UpdateStepResponse>> UpdateStepAsync(JobId jobId, JobStepBatchId batchId, JobStepId stepId, [FromBody] UpdateStepRequest request)
		{
			IJob? job = await _jobService.GetJobAsync(jobId);
			if (job == null)
			{
				return NotFound(jobId);
			}

			StreamConfig? streamConfig;
			if (!_globalConfig.Value.TryGetStream(job.StreamId, out streamConfig))
			{
				return NotFound(job.StreamId);
			}

			// Check permissions for updating this step. Only the agent executing the step can modify the state of it.
			if (request.State != JobStepState.Unspecified || request.Outcome != JobStepOutcome.Unspecified)
			{
				IJobStepBatch? batch = job.Batches.FirstOrDefault(x => x.Id == batchId);
				if (batch == null)
				{
					return NotFound(jobId, batchId);
				}
				if (!batch.SessionId.HasValue || !User.HasSessionClaim(batch.SessionId.Value))
				{
					return Forbid();
				}
			}

			if (request.Retry != null || request.Priority != null)
			{
				if (!streamConfig.Authorize(JobAclAction.RetryJobStep, User))
				{
					return Forbid(JobAclAction.RetryJobStep, jobId);
				}
			}
			if (request.Properties != null)
			{
				if (!streamConfig.Authorize(JobAclAction.UpdateJob, User))
				{
					return Forbid(JobAclAction.UpdateJob, jobId);
				}
			}

			UserId? retryByUser = (request.Retry.HasValue && request.Retry.Value) ? User.GetUserId() : null;
			UserId? abortByUser = (request.AbortRequested.HasValue && request.AbortRequested.Value) ? User.GetUserId() : null;

			try
			{
				NodeRef? retryNodeRef = null;
				if (request.Retry != null && job.TryGetStep(stepId, out IJobStepBatch? batch, out IJobStep? step))
				{
					retryNodeRef = new NodeRef(batch.GroupIdx, step.NodeIdx);
				}

				IJob? newJob = await _jobService.UpdateStepAsync(job, batchId, stepId, streamConfig, request.State, request.Outcome, null, request.AbortRequested, abortByUser, (request.LogId == null) ? null : LogId.Parse(request.LogId), null, retryByUser, request.Priority, null, request.Properties);
				if (newJob == null)
				{
					return NotFound(jobId);
				}

				UpdateStepResponse response = new UpdateStepResponse();
				if (retryNodeRef != null)
				{
					JobStepRefId? retriedStepId = newJob.FindLatestStepForNode(retryNodeRef);
					if (retriedStepId != null)
					{
						response.BatchId = retriedStepId.Value.BatchId.ToString();
						response.StepId = retriedStepId.Value.StepId.ToString();
					}
				}
				return response;
			}
			catch (RetryNotAllowedException ex)
			{
				return BadRequest(ex.Message);
			}
		}

		/// <summary>
		/// Gets a particular step currently scheduled to be executed for a job
		/// </summary>
		/// <param name="jobId">Unique id for the job</param>
		/// <param name="batchId">Unique id for the batch</param>
		/// <param name="stepId">Unique id for the step</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of nodes to be executed</returns>
		[HttpGet]
		[Obsolete("Query entire job instead")]
		[Route("/api/v1/jobs/{jobId}/batches/{batchId}/steps/{stepId}")]
		[ProducesResponseType(typeof(GetStepResponse), 200)]
		public async Task<ActionResult<object>> GetStepAsync(JobId jobId, JobStepBatchId batchId, JobStepId stepId, [FromQuery] PropertyFilter? filter = null, CancellationToken cancellationToken = default)
		{
			IJob? job = await _jobService.GetJobAsync(jobId, cancellationToken);
			if (job == null)
			{
				return NotFound(jobId);
			}

			StreamConfig? streamConfig;
			if (!_globalConfig.Value.TryGetStream(job.StreamId, out streamConfig))
			{
				return NotFound(job.StreamId);
			}
			if (!streamConfig.Authorize(JobAclAction.ViewJob, User))
			{
				return Forbid(JobAclAction.ViewJob, jobId);
			}

			foreach (IJobStepBatch batch in job.Batches)
			{
				if (batch.Id == batchId)
				{
					foreach (IJobStep step in batch.Steps)
					{
						if (step.Id == stepId)
						{
							GetStepResponse response = await CreateStepResponseAsync(step, cancellationToken);
							return response.ApplyFilter(filter);
						}
					}
					return NotFound(jobId, batchId, stepId);
				}
			}

			return NotFound(jobId, batchId);
		}

		/// <summary>
		/// Updates notifications for a specific job.
		/// </summary>
		/// <param name="jobId">Unique id for the job</param>
		/// <param name="batchId">Unique id for the batch</param>
		/// <param name="stepId">Unique id for the step</param>
		/// <param name="request">The notification request</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about the requested job</returns>
		[HttpPut]
		[Route("/api/v1/jobs/{jobId}/batches/{batchId}/steps/{stepId}/notifications")]
		public async Task<ActionResult> UpdateStepNotificationsAsync(JobId jobId, JobStepBatchId batchId, JobStepId stepId, [FromBody] UpdateNotificationsRequest request, CancellationToken cancellationToken)
		{
			IJob? job = await _jobService.GetJobAsync(jobId, cancellationToken);
			if (job == null)
			{
				return NotFound(jobId);
			}

			StreamConfig? streamConfig;
			if (!_globalConfig.Value.TryGetStream(job.StreamId, out streamConfig))
			{
				return NotFound(job.StreamId);
			}
			if (!streamConfig.Authorize(NotificationAclAction.CreateSubscription, User))
			{
				return Forbid(NotificationAclAction.CreateSubscription, jobId);
			}

			if (!job.TryGetBatch(batchId, out IJobStepBatch? batch))
			{
				return NotFound(jobId, batchId);
			}

			if (!batch.TryGetStep(stepId, out IJobStep? step))
			{
				return NotFound(jobId, batchId, stepId);
			}

			ObjectId? triggerId = step.NotificationTriggerId;
			if (triggerId == null)
			{
				triggerId = ObjectId.GenerateNewId();
				if (await _jobService.UpdateStepAsync(job, batchId, stepId, streamConfig, JobStepState.Unspecified, JobStepOutcome.Unspecified, newNotificationTriggerId: triggerId, cancellationToken: cancellationToken) == null)
				{
					return NotFound(jobId, batchId, stepId);
				}
			}

			await _notificationService.UpdateSubscriptionsAsync(triggerId.Value, User, request.Email, request.Slack, cancellationToken);
			return Ok();
		}

		/// <summary>
		/// Gets information about a specific job.
		/// </summary>
		/// <param name="jobId">Id of the job to find</param>
		/// <param name="batchId">Unique id for the batch</param>
		/// <param name="stepId">Unique id for the step</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about the requested job</returns>
		[HttpGet]
		[Route("/api/v1/jobs/{jobId}/batches/{batchId}/steps/{stepId}/notifications")]
		public async Task<ActionResult<GetNotificationResponse>> GetStepNotificationsAsync(JobId jobId, JobStepBatchId batchId, JobStepId stepId, CancellationToken cancellationToken)
		{
			IJob? job = await _jobService.GetJobAsync(jobId, cancellationToken);
			if (job == null)
			{
				return NotFound(jobId);
			}

			IJobStep? step;
			if (!job.TryGetBatch(batchId, out IJobStepBatch? batch))
			{
				return NotFound(jobId, batchId);
			}
			if (!batch.TryGetStep(stepId, out step))
			{
				return NotFound(jobId, batchId, stepId);
			}

			StreamConfig? streamConfig;
			if (!_globalConfig.Value.TryGetStream(job.StreamId, out streamConfig))
			{
				return NotFound(job.StreamId);
			}
			if (!streamConfig.Authorize(NotificationAclAction.CreateSubscription, User))
			{
				return Forbid(NotificationAclAction.CreateSubscription, jobId);
			}

			INotificationSubscription? subscription;
			if (step.NotificationTriggerId == null)
			{
				subscription = null;
			}
			else
			{
				subscription = await _notificationService.GetSubscriptionsAsync(step.NotificationTriggerId.Value, User, cancellationToken);
			}
			return new GetNotificationResponse(subscription);
		}

		/// <summary>
		/// Gets a particular step currently scheduled to be executed for a job
		/// </summary>
		/// <param name="jobId">Unique id for the job</param>
		/// <param name="batchId">Unique id for the batch</param>
		/// <param name="stepId">Unique id for the step</param>
		/// <param name="name"></param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of nodes to be executed</returns>
		[HttpGet]
		[Route("/api/v1/jobs/{jobId}/batches/{batchId}/steps/{stepId}/artifacts/{*name}")]
		public async Task<ActionResult> GetArtifactAsync(JobId jobId, JobStepBatchId batchId, JobStepId stepId, string name, CancellationToken cancellationToken)
		{
			IJob? job = await _jobService.GetJobAsync(jobId, cancellationToken);
			if (job == null)
			{
				return NotFound(jobId);
			}

			StreamConfig? streamConfig;
			if (!_globalConfig.Value.TryGetStream(job.StreamId, out streamConfig))
			{
				return NotFound(job.StreamId);
			}
			if (!streamConfig.Authorize(JobAclAction.ViewJob, User))
			{
				return Forbid(JobAclAction.ViewJob, jobId);
			}

			if (!job.TryGetBatch(batchId, out IJobStepBatch? batch))
			{
				return NotFound(jobId, batchId);
			}
			if (!batch.TryGetStep(stepId, out _))
			{
				return NotFound(jobId, batchId, stepId);
			}

			IReadOnlyList<IArtifactV1> artifacts = await _artifactCollectionV1.GetArtifactsAsync(jobId, stepId, name, cancellationToken);
			if (artifacts.Count == 0)
			{
				return NotFound();
			}

			IArtifactV1 artifact = artifacts[0];
			return new FileStreamResult(await _artifactCollectionV1.OpenArtifactReadStreamAsync(artifact, cancellationToken), artifact.MimeType);
		}

		/// <summary>
		/// Gets a particular step currently scheduled to be executed for a job
		/// </summary>
		/// <param name="jobId">Unique id for the job</param>
		/// <param name="batchId">Unique id for the batch</param>
		/// <param name="stepId">Unique id for the step</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of nodes to be executed</returns>
		[HttpGet]
		[Route("/api/v1/jobs/{jobId}/batches/{batchId}/steps/{stepId}/trace")]
		public async Task<ActionResult> GetStepTraceAsync(JobId jobId, JobStepBatchId batchId, JobStepId stepId, CancellationToken cancellationToken)
		{
			IJob? job = await _jobService.GetJobAsync(jobId, cancellationToken);
			if (job == null)
			{
				return NotFound(jobId);
			}

			StreamConfig? streamConfig;
			if (!_globalConfig.Value.TryGetStream(job.StreamId, out streamConfig))
			{
				return NotFound(job.StreamId);
			}
			if (!streamConfig.Authorize(JobAclAction.ViewJob, User))
			{
				return Forbid(JobAclAction.ViewJob, jobId);
			}

			if (!job.TryGetBatch(batchId, out IJobStepBatch? batch))
			{
				return NotFound(jobId, batchId);
			}
			if (!batch.TryGetStep(stepId, out _))
			{
				return NotFound(jobId, batchId, stepId);
			}

			IReadOnlyList<IArtifactV1> artifacts = await _artifactCollectionV1.GetArtifactsAsync(jobId, stepId, null, cancellationToken);
			foreach (IArtifactV1 artifact in artifacts)
			{
				if (artifact.Name.Equals("trace.json", StringComparison.OrdinalIgnoreCase))
				{
					return new FileStreamResult(await _artifactCollectionV1.OpenArtifactReadStreamAsync(artifact, cancellationToken), "text/json");
				}
			}
			return NotFound();
		}

		/// <summary>
		/// Updates notifications for a specific label.
		/// </summary>
		/// <param name="jobId">Unique id for the job</param>
		/// <param name="labelIndex">Index for the label</param>
		/// <param name="request">The notification request</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		[HttpPut]
		[Route("/api/v1/jobs/{jobId}/labels/{labelIndex}/notifications")]
		public async Task<ActionResult> UpdateLabelNotificationsAsync(JobId jobId, int labelIndex, [FromBody] UpdateNotificationsRequest request, CancellationToken cancellationToken)
		{
			ObjectId triggerId;
			for (; ; )
			{
				IJob? job = await _jobService.GetJobAsync(jobId, cancellationToken);
				if (job == null)
				{
					return NotFound(jobId);
				}

				StreamConfig? streamConfig;
				if (!_globalConfig.Value.TryGetStream(job.StreamId, out streamConfig))
				{
					return NotFound(job.StreamId);
				}
				if (!streamConfig.Authorize(NotificationAclAction.CreateSubscription, User))
				{
					return Forbid(NotificationAclAction.CreateSubscription, jobId);
				}

				ObjectId newTriggerId;
				if (job.LabelIdxToTriggerId.TryGetValue(labelIndex, out newTriggerId))
				{
					triggerId = newTriggerId;
					break;
				}

				newTriggerId = ObjectId.GenerateNewId();

				IJob? newJob = await _jobService.UpdateJobAsync(job, labelIdxToTriggerId: new KeyValuePair<int, ObjectId>(labelIndex, newTriggerId), cancellationToken: cancellationToken);
				if (newJob != null)
				{
					triggerId = newTriggerId;
					break;
				}
			}

			await _notificationService.UpdateSubscriptionsAsync(triggerId, User, request.Email, request.Slack, cancellationToken);
			return Ok();
		}

		/// <summary>
		/// Gets notification info about a specific label in a job.
		/// </summary>
		/// <param name="jobId">Id of the job to find</param>
		/// <param name="labelIndex">Index for the label</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Notification info for the requested label in the job</returns>
		[HttpGet]
		[Route("/api/v1/jobs/{jobId}/labels/{labelIndex}/notifications")]
		public async Task<ActionResult<GetNotificationResponse>> GetLabelNotificationsAsync(JobId jobId, int labelIndex, CancellationToken cancellationToken)
		{
			IJob? job = await _jobService.GetJobAsync(jobId, cancellationToken);
			if (job == null)
			{
				return NotFound(jobId);
			}

			INotificationSubscription? subscription;
			if (!job.LabelIdxToTriggerId.ContainsKey(labelIndex))
			{
				subscription = null;
			}
			else
			{
				subscription = await _notificationService.GetSubscriptionsAsync(job.LabelIdxToTriggerId[labelIndex], User, cancellationToken);
			}
			return new GetNotificationResponse(subscription);
		}
	}
}
