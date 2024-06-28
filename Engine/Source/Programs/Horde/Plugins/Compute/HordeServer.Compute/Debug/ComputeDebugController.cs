// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Text;
using System.Threading.Tasks;
using HordeServer.Server;
using HordeServer.Utilities;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;

namespace HordeServer.Debug
{
	class ComputeDebugController : HordeControllerBase
	{
		readonly IOptionsSnapshot<ComputeConfig> _computeConfig;

		public ComputeDebugController(IOptionsSnapshot<ComputeConfig> computeConfig)
		{
			_computeConfig = computeConfig;
		}

		/// <summary>
		/// Get the network ID for a given IP address
		/// </summary>
		[HttpGet]
		[Route("/api/v1/debug/network-id")]
		public ActionResult<object> GetNetworkId([FromQuery] string? ipAddress = null)
		{
			if (!_computeConfig.Value.Authorize(ServerAclAction.Debug, User))
			{
				return Forbid(ServerAclAction.Debug);
			}

			if (ipAddress == null || !IPAddress.TryParse(ipAddress, out IPAddress? ip))
			{
				return BadRequest("Unable to read or convert query parameter 'ipAddress'");
			}

			_computeConfig.Value.TryGetNetworkConfig(ip, out NetworkConfig? networkConfig);
			return networkConfig == null ? StatusCode(StatusCodes.Status500InternalServerError, "Unable to find a network config for the IP") : Ok(networkConfig);
		}
	}
}
