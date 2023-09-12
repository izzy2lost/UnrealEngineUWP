// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Net;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Api;
using EpicGames.Horde.Compute;
using Horde.Server.Acls;
using Horde.Server.Server;
using Horde.Server.Utilities;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;

namespace Horde.Server.Compute
{
	/// <summary>
	/// Controller for the /api/v2/compute endpoint
	/// </summary>
	[ApiController]
	[Authorize]
	[Route("[controller]")]
	public class ComputeControllerV2 : HordeControllerBase
	{
		readonly ComputeService _computeService;
		readonly IOptionsSnapshot<GlobalConfig> _globalConfig;

		/// <summary>
		/// Constructor
		/// </summary>
		public ComputeControllerV2(ComputeService computeService, IOptionsSnapshot<GlobalConfig> globalConfig)
		{
			_computeService = computeService;
			_globalConfig = globalConfig;
		}

		/// <summary>
		/// Add tasks to be executed remotely
		/// </summary>
		/// <param name="clusterId">Id of the compute cluster</param>
		/// <param name="request">The request parameters</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		[HttpPost]
		[Authorize]
		[Route("/api/v2/compute/{clusterId}")]
		public async Task<ActionResult<AssignComputeResponse>> AssignComputeResourceAsync(ClusterId clusterId, [FromBody] AssignComputeRequest request, CancellationToken cancellationToken)
		{
			ComputeClusterConfig? clusterConfig;
			if (!_globalConfig.Value.TryGetComputeCluster(clusterId, out clusterConfig))
			{
				return NotFound(clusterId);
			}
			if(!clusterConfig.Authorize(ComputeAclAction.AddComputeTasks, User))
			{
				return Forbid(ComputeAclAction.AddComputeTasks, clusterId);
			}

			LeaseId? parentLeaseId = User.GetLeaseClaim();

			Requirements requirements = request.Requirements ?? new Requirements();

			ComputeResource? computeResource = await _computeService.TryAllocateResourceAsync(requirements, parentLeaseId, cancellationToken);
			if (computeResource == null)
			{
				return StatusCode((int)HttpStatusCode.ServiceUnavailable);
			}

			AssignComputeResponse response = new AssignComputeResponse();
			response.Ip = computeResource.Ip.ToString();
			response.Port = computeResource.Port;
			response.Nonce = StringUtils.FormatHexString(computeResource.Task.Nonce.Span);
			response.Key = StringUtils.FormatHexString(computeResource.Task.Key.Span);
			response.AgentId = computeResource.AgentId;
			response.LeaseId = computeResource.LeaseId;
			response.Properties = computeResource.Properties;

			foreach (KeyValuePair<string, int> pair in computeResource.Task.Resources)
			{
				response.AssignedResources.Add(pair.Key, pair.Value);
			}

			return response;
		}
	}
}
