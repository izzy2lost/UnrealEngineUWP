// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.RegularExpressions;
using EpicGames.Horde.Projects;
using Horde.Server.Configuration;
using Horde.Server.Server;
using Horde.Server.Streams;
using Horde.Server.Utilities;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.AspNetCore.StaticFiles;
using Microsoft.Extensions.Options;

namespace Horde.Server.Parameters
{
	/// <summary>
	/// Controller for the /api/v1/parameters endpoint. Provides configuration data to other tools.
	/// </summary>
	[ApiController]
	[AllowAnonymous]
	public class ParametersController : HordeControllerBase
	{
		private readonly IOptionsSnapshot<GlobalConfig> _globalConfig;

		/// <summary>
		/// Constructor
		/// </summary>
		public ParametersController(IOptionsSnapshot<GlobalConfig> globalConfig)
		{
			_globalConfig = globalConfig;
		}

		/// <summary>
		/// Query all the parameters
		/// </summary>
		/// <param name="filter">Filter for the properties to return</param>
		/// <returns>Parameters matching the requested filter</returns>
		[HttpGet]
		[Route("/api/v1/parameters")]
		[ProducesResponseType(typeof(object), 200)]
		public ActionResult<object> GetParameters([FromQuery] PropertyFilter? filter = null)
		{
			GlobalConfig globalConfig = _globalConfig.Value;
			return PropertyFilter.Apply(globalConfig.Parameters, filter);
		}
	}
}
