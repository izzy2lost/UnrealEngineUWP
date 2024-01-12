// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Replicators;
using EpicGames.Horde.Streams;
using Horde.Server.Acls;
using Horde.Server.Projects;
using Horde.Server.Server;
using Horde.Server.Streams;
using Horde.Server.Utilities;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;

namespace Horde.Server.Replicators
{
	/// <summary>
	/// Controller for the /api/v1/projects endpoint
	/// </summary>
	[ApiController]
	[Authorize]
	public class ReplicatorsController : HordeControllerBase
	{
		readonly IReplicatorCollection _replicatorCollection;
		readonly IOptionsSnapshot<GlobalConfig> _globalConfig;

		/// <summary>
		/// Constructor
		/// </summary>
		public ReplicatorsController(IReplicatorCollection replicatorCollection, IOptionsSnapshot<GlobalConfig> globalConfig)
		{
			_replicatorCollection = replicatorCollection;
			_globalConfig = globalConfig;
		}

		/// <summary>
		/// Query all the replicators
		/// </summary>
		/// <param name="streamId">Stream to query</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <returns>Information about all the projects</returns>
		[HttpGet]
		[Route("/api/v1/replicators")]
		[ProducesResponseType(typeof(List<GetReplicatorResponse>), 200)]
		public async Task<ActionResult<object>> GetReplicatorsAsync([FromQuery] StreamId? streamId = null, [FromQuery] PropertyFilter? filter = null)
		{
			List<object> responses = new List<object>();

			GlobalConfig globalConfig = _globalConfig.Value;
			foreach (ProjectConfig projectConfig in globalConfig.Projects)
			{
				foreach (StreamConfig streamConfig in projectConfig.Streams)
				{
					if (streamId == null || streamConfig.Id == streamId.Value)
					{
						if (streamConfig.Authorize(ReplicatorAclAction.ViewReplicator, User))
						{
							foreach (ReplicatorConfig replicatorConfig in streamConfig.Replicators)
							{
								ReplicatorId replicatorId = new ReplicatorId(streamConfig.Id, replicatorConfig.Id);
								IReplicator? replicator = await _replicatorCollection.GetAsync(replicatorId);
								GetReplicatorResponse response = CreateGetReplicatorResponse(replicatorId, replicator);
								responses.Add(response.ApplyFilter(filter));
							}
						}
					}
				}
			}

			return responses;
		}

		/// <summary>
		/// Query a replicator state
		/// </summary>
		/// <param name="replicatorId">Repliactor to query</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <returns>Information about all the projects</returns>
		[HttpGet]
		[Route("/api/v1/replicator/{replicatorId}")]
		[ProducesResponseType(typeof(GetReplicatorResponse), 200)]
		public async Task<ActionResult<object>> GetReplicatorsAsync(ReplicatorId replicatorId, [FromQuery] PropertyFilter? filter = null)
		{
			GlobalConfig globalConfig = _globalConfig.Value;
			if (!globalConfig.TryGetStream(replicatorId.StreamId, out StreamConfig? streamConfig))
			{
				return NotFound();
			}
			if (streamConfig.Authorize(ReplicatorAclAction.ViewReplicator, User))
			{
				return Forbid(ReplicatorAclAction.ViewReplicator);
			}

			if (!streamConfig.TryGetReplicator(replicatorId.StreamReplicatorId, out _))
			{
				return NotFound();
			}

			IReplicator? replicator = await _replicatorCollection.GetAsync(replicatorId);
			GetReplicatorResponse response = CreateGetReplicatorResponse(replicatorId, replicator);
			return response.ApplyFilter(filter);
		}

		/// <summary>
		/// Query a replicator state
		/// </summary>
		/// <param name="replicatorId">Repliactor to query</param>
		/// <param name="request">Update request</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		[HttpPut]
		[Route("/api/v1/replicator/{replicatorId}")]
		public async Task<ActionResult<object>> UpdateReplicatorAsync(ReplicatorId replicatorId, [FromBody] UpdateReplicatorRequest request, CancellationToken cancellationToken)
		{
			GlobalConfig globalConfig = _globalConfig.Value;
			if (!globalConfig.TryGetStream(replicatorId.StreamId, out StreamConfig? streamConfig))
			{
				return NotFound();
			}
			if (streamConfig.Authorize(ReplicatorAclAction.UpdateReplicator, User))
			{
				return Forbid(ReplicatorAclAction.UpdateReplicator);
			}

			if (!streamConfig.TryGetReplicator(replicatorId.StreamReplicatorId, out _))
			{
				return NotFound();
			}

			for (; ; )
			{
				IReplicator? replicator = await _replicatorCollection.GetOrAddAsync(replicatorId, cancellationToken: cancellationToken);

				UpdateReplicatorOptions updateOptions = new UpdateReplicatorOptions(NewPaused: request.Paused, NewLastChange: (request.Reset ?? false) ? 0 : null, request.Change);
				if (await replicator.TryUpdateAsync(updateOptions, cancellationToken) != null)
				{
					break;
				}
			}

			return Ok();
		}

		static GetReplicatorResponse CreateGetReplicatorResponse(ReplicatorId replicatorId, IReplicator? replicator)
		{
			GetReplicatorResponse response = new GetReplicatorResponse();
			response.Id = replicatorId;

			if (replicator != null)
			{
				response.CurrentChange = replicator.CurrentChange;
				response.CurrentChangeStartTime = replicator.CurrentChangeStartTime;
				response.LastChange = replicator.LastChange;
				response.LastChangeFinishTime = replicator.LastChangeFinishTime;
				response.Error = replicator.Error;
			}

			return response;
		}
	}
}
