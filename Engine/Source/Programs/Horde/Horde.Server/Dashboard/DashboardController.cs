// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Text;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Authentication;
using Microsoft.AspNetCore.Authentication.Cookies;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;
using Horde.Server.Server;
using System.Collections.Generic;
using System.Linq;
using Horde.Server.Acls;
using EpicGames.Horde.Dashboard;

namespace Horde.Server.Dashboard
{
	/// <summary>	
	/// Dashboard authorization challenge controller	
	/// </summary>	
	[ApiController]
	[Route("[controller]")]
	public class DashboardController : Controller
	{
		/// <summary>
		/// Authentication scheme in use
		/// </summary>
		readonly string _authenticationScheme;

		/// <summary>
		/// Server settings
		/// </summary>
		private readonly ServerSettings _settings;

		private readonly IDashboardPreviewCollection _previewCollection;

		private readonly IOptionsSnapshot<GlobalConfig> _globalConfig;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="previewCollection" />
		/// <param name="serverSettings">Server settings</param>
		/// <param name="globalConfig" />
		public DashboardController(IDashboardPreviewCollection previewCollection, IOptionsMonitor<ServerSettings> serverSettings, IOptionsSnapshot<GlobalConfig> globalConfig)
		{
			_authenticationScheme = AccountController.GetAuthScheme(serverSettings.CurrentValue.AuthMethod);
			_previewCollection = previewCollection;
			_settings = serverSettings.CurrentValue;
			_globalConfig = globalConfig;
		}

		/// <summary>	
		/// Challenge endpoint for the dashboard, using cookie authentication scheme	
		/// </summary>	
		/// <returns>Ok on authorized, otherwise will 401</returns>	
		[HttpGet]
		[Authorize]
		[Route("/api/v1/dashboard/challenge")]
		public StatusCodeResult GetChallenge()
		{
			return Ok();
		}

		/// <summary>
		/// Login to server, redirecting to the specified URL on success
		/// </summary>
		/// <param name="redirect"></param>
		/// <returns></returns>
		[HttpGet]
		[Route("/api/v1/dashboard/login")]
		public IActionResult Login([FromQuery] string? redirect)
		{
			return new ChallengeResult(_authenticationScheme, new AuthenticationProperties { RedirectUri = redirect ?? "/" });
		}

		/// <summary>
		/// Login to server, redirecting to the specified Base64 encoded URL, which fixes some escaping issues on some auth providers, on success
		/// </summary>
		/// <param name="redirect"></param>
		/// <returns></returns>
		[HttpGet]
		[Route("/api/v2/dashboard/login")]
		public IActionResult LoginV2([FromQuery] string? redirect)
		{
			string? redirectUri = null;

			if (redirect != null)
			{
				byte[] data = Convert.FromBase64String(redirect);
				redirectUri = Encoding.UTF8.GetString(data);
			}

			return new ChallengeResult(_authenticationScheme, new AuthenticationProperties { RedirectUri = redirectUri ?? "/index" });
		}

		/// <summary>
		/// Logout of the current account
		/// </summary>
		/// <returns></returns>
		[HttpGet]
		[Route("/api/v1/dashboard/logout")]
		public async Task<StatusCodeResult> LogoutAsync()
		{
			await HttpContext.SignOutAsync(CookieAuthenticationDefaults.AuthenticationScheme);
			try
			{
				await HttpContext.SignOutAsync(_authenticationScheme);
			}
			catch
			{
			}

			return Ok();
		}

		/// <summary>
		/// Query all the projects
		/// </summary>
		/// <returns>Config information needed by the dashboard</returns>
		[HttpGet]
		[Authorize]
		[Route("/api/v1/dashboard/config")]
		public ActionResult<GetDashboardConfigResponse> GetDashbordConfig()
		{
			GetDashboardConfigResponse dashboardConfigResponse = new GetDashboardConfigResponse();

			if (_settings.JiraUrl != null)
			{
				dashboardConfigResponse.ExternalIssueServiceName = "Jira";
				dashboardConfigResponse.ExternalIssueServiceUrl = _settings.JiraUrl.ToString().TrimEnd('/');
			}

			if (_settings.P4SwarmUrl != null)
			{
				dashboardConfigResponse.PerforceSwarmUrl = _settings.P4SwarmUrl.ToString().TrimEnd('/');
			}

			dashboardConfigResponse.HelpEmailAddress = _settings.HelpEmailAddress;
			dashboardConfigResponse.HelpSlackChannel = _settings.HelpSlackChannel;

			dashboardConfigResponse.DeviceProblemCooldownMinutes = _settings.DeviceProblemCooldownMinutes;

			foreach (DashboardAgentCategoryConfig category in _globalConfig.Value.Dashboard.AgentCategories)
			{
				dashboardConfigResponse.AgentCategories.Add(new GetDashboardAgentCategoryResponse { Name = category.Name, Condition = category.Condition });
			}

			foreach (TelemetryViewConfig telemetry in _globalConfig.Value.Dashboard.Telemetry)
			{
				GetTelemetryViewResponse rview = new GetTelemetryViewResponse();
				rview.Id = telemetry.Id.ToString();
				rview.Name = telemetry.Name;

				foreach (TelemetryVariableConfig variable in telemetry.Variables)
				{
					rview.Variables.Add(new GetTelemetryVariableResponse { Name = variable.Name, Group = variable.Group });
				}

				foreach (TelemetryCategoryConfig category in telemetry.Categories)
				{
					GetTelemetryCategoryResponse rcategory = new GetTelemetryCategoryResponse { Name = category.Name };

					foreach (TelemetryChartConfig chart in category.Charts)
					{
						GetTelemetryChartResponse rchart = new GetTelemetryChartResponse { Name = chart.Name, Display = chart.Display.ToString(), Graph = chart.Graph.ToString(), Max = chart.Max, Metrics = new List<GetTelemetryChartMetricResponse>() };

						foreach (TelemetryChartMetricConfig metric  in chart.Metrics)
						{
							rchart.Metrics.Add(new GetTelemetryChartMetricResponse { MetricId = metric.Id.ToString(), Threshold = metric.Threshold, Alias = metric.Alias });
						}

						rcategory.Charts.Add(rchart);
					}

					rview.Categories.Add(rcategory);
				}

				dashboardConfigResponse.TelemetryViews.Add(rview);
			}

			return dashboardConfigResponse;
		}

		/// <summary>
		/// Create a new dashboard preview item
		/// </summary>
		/// <returns>Config information needed by the dashboard</returns>
		[HttpPost]
		[Authorize]
		[Route("/api/v1/dashboard/preview")]
		public async Task<ActionResult<GetDashboardPreviewResponse>> CreateDashbordPreviewAsync([FromBody] CreateDashboardPreviewRequest request)
		{
			if (!_globalConfig.Value.Authorize(AdminAclAction.AdminWrite, User))
			{
				return Forbid();
			}

			IDashboardPreview preview = await _previewCollection.AddPreviewAsync(request.Summary);

			if (!String.IsNullOrEmpty(request.ExampleLink) || !String.IsNullOrEmpty(request.DiscussionLink) || !String.IsNullOrEmpty(request.TrackingLink))
			{
				IDashboardPreview? updated = await _previewCollection.UpdatePreviewAsync(preview.Id, null, null, null, request.ExampleLink, request.DiscussionLink, request.TrackingLink);
				if (updated == null) 
				{
					return NotFound(preview.Id);
				}

				return CreatePreviewResponse(updated);
			}

			return CreatePreviewResponse(preview);
		}

		/// <summary>
		/// Update a dashboard preview item
		/// </summary>
		/// <returns>Config information needed by the dashboard</returns>
		[HttpPut]
		[Authorize]
		[Route("/api/v1/dashboard/preview")]
		public async Task<ActionResult<GetDashboardPreviewResponse>> UpdateDashbordPreviewAsync([FromBody] UpdateDashboardPreviewRequest request)
		{
			if (!_globalConfig.Value.Authorize(AdminAclAction.AdminWrite, User))
			{
				return Forbid();
			}

			IDashboardPreview? preview = await _previewCollection.UpdatePreviewAsync(request.Id, request.Summary, request.DeployedCL, request.Open, request.ExampleLink, request.DiscussionLink, request.TrackingLink);
			
			if (preview == null)
			{
				return NotFound(request.Id);
			}			

			return CreatePreviewResponse(preview);
		}

		/// <summary>
		/// Query dashboard preview items
		/// </summary>
		/// <returns>Config information needed by the dashboard</returns>
		[HttpGet]
		[Authorize]
		[Route("/api/v1/dashboard/previews")]
		public async Task<ActionResult<List<GetDashboardPreviewResponse>>> GetDashbordPreviewsAsync([FromQuery] bool open = true)
		{			
			List <IDashboardPreview> previews = await _previewCollection.FindPreviewsAsync(open);			
			return previews.Select(CreatePreviewResponse).ToList();
		}

		static GetDashboardPreviewResponse CreatePreviewResponse(IDashboardPreview preview)
		{
			GetDashboardPreviewResponse response = new GetDashboardPreviewResponse();
			response.Id = preview.Id;
			response.CreatedAt = preview.CreatedAt;
			response.Summary = preview.Summary;
			response.DeployedCL = preview.DeployedCL;
			response.Open = preview.Open;
			response.ExampleLink = preview.ExampleLink;
			response.DiscussionLink = preview.DiscussionLink;
			response.TrackingLink = preview.TrackingLink;
			return response;
		}
	}
}
