// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics;
using System.Reflection;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde;
using EpicGames.Horde.Server;
using HordeServer.Agents;
using HordeServer.Tools;
using HordeServer.Utilities;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;

namespace HordeServer.Server
{
	/// <summary>
	/// Controller managing account status
	/// </summary>
	[ApiController]
	[Authorize]
	[Route("[controller]")]
	public class ServerController : HordeControllerBase
	{
		readonly IToolCollection _toolCollection;
		readonly IClock _clock;
		readonly IOptionsSnapshot<GlobalConfig> _globalConfig;

		/// <summary>
		/// Constructor
		/// </summary>
		public ServerController(IToolCollection toolCollection, IClock clock, IOptionsSnapshot<GlobalConfig> globalConfig)
		{
			_toolCollection = toolCollection;
			_clock = clock;
			_globalConfig = globalConfig;
		}

		/// <summary>
		/// Get server version
		/// </summary>
		[HttpGet]
		[AllowAnonymous]
		[Route("/api/v1/server/version")]
		public ActionResult GetVersion()
		{
			FileVersionInfo fileVersionInfo = FileVersionInfo.GetVersionInfo(Assembly.GetExecutingAssembly().Location);
			return Ok(fileVersionInfo.ProductVersion);
		}

		/// <summary>
		/// Get server information
		/// </summary>
		[HttpGet]
		[AllowAnonymous]
		[Route("/api/v1/server/info")]
		[ProducesResponseType(typeof(GetServerInfoResponse), 200)]
		public async Task<ActionResult<GetServerInfoResponse>> GetServerInfoAsync()
		{
			GetServerInfoResponse response = new GetServerInfoResponse();
			response.ApiVersion = HordeApiVersion.Latest;

			FileVersionInfo versionInfo = FileVersionInfo.GetVersionInfo(Assembly.GetExecutingAssembly().Location);
			response.ServerVersion = versionInfo.ProductVersion ?? String.Empty;

			ITool? tool = await _toolCollection.GetAsync(AgentExtensions.AgentToolId, HttpContext.RequestAborted);
			if (tool != null)
			{
				IToolDeployment? deployment = tool.GetCurrentDeployment(1.0, _clock.UtcNow);
				if (deployment != null)
				{
					response.AgentVersion = deployment.Version;
				}
			}

			return response;
		}

		/// <summary>
		/// Gets connection information
		/// </summary>
		[HttpGet]
		[AllowAnonymous]
		[Route("/api/v1/server/connection")]
		public ActionResult<GetConnectionResponse> GetConnection()
		{
			GetConnectionResponse response = new GetConnectionResponse();
			response.Ip = HttpContext.Connection.RemoteIpAddress?.ToString();
			response.Port = HttpContext.Connection.RemotePort;
			return response;
		}

		/// <summary>
		/// Gets ports used by the server
		/// </summary>
		[HttpGet]
		[AllowAnonymous]
		[Route("/api/v1/server/ports")]
		public ActionResult<GetPortsResponse> GetPorts()
		{
			ServerSettings serverSettings = _globalConfig.Value.ServerSettings;

			GetPortsResponse response = new GetPortsResponse();
			response.Http = serverSettings.HttpPort;
			response.Https = serverSettings.HttpsPort;
			response.UnencryptedHttp2 = serverSettings.Http2Port;
			return response;
		}

		/// <summary>
		/// Returns settings for automating auth against this server
		/// </summary>
		[HttpGet]
		[AllowAnonymous]
		[Route("/api/v1/server/auth")]
		public ActionResult<GetAuthConfigResponse> GetAuthConfig()
		{
			ServerSettings settings = _globalConfig.Value.ServerSettings;

			GetAuthConfigResponse response = new GetAuthConfigResponse();
			response.Method = settings.AuthMethod;
			response.ProfileName = settings.OidcProfileName;
			if (settings.AuthMethod == AuthMethod.Horde)
			{
				response.ServerUrl = new Uri(_globalConfig.Value.ServerSettings.ServerUrl, "api/v1/oauth2").ToString();
				response.ClientId = "default";
			}
			else
			{
				response.ServerUrl = settings.OidcAuthority;
				response.ClientId = settings.OidcClientId;
			}
			response.LocalRedirectUrls = settings.OidcLocalRedirectUrls;
			return response;
		}
	}
}
