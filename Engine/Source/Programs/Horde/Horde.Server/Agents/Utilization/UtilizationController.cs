// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using EpicGames.Horde.Agents.Telemetry;
using Microsoft.AspNetCore.Mvc;

namespace Horde.Server.Agents.Telemetry
{
	/// <summary>
	/// Controller for the /api/v1/reports endpoint, used for the reports pages
	/// </summary>
	[ApiController]
	[Route("[controller]")]
	public sealed class ReportsController : ControllerBase
	{
		/// <summary>
		/// the Telemetry collection singleton
		/// </summary>
		readonly IUtilizationDataCollection _utilizationDataCollection;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="utilizationDataCollection">The telemetry collection</param>
		public ReportsController(IUtilizationDataCollection utilizationDataCollection)
		{
			_utilizationDataCollection = utilizationDataCollection;
		}

		/// <summary>
		/// Gets a collection of utilization data including an endpoint and a range
		/// </summary>
		[HttpGet]
		[Route("/api/v1/reports/utilization/{endDate}")]
		public async Task<ActionResult<List<UtilizationTelemetryResponse>>> GetStreamUtilizationDataAsync(DateTime endDate, [FromQuery(Name = "Range")] int range, [FromQuery(Name = "TzOffset")] int? tzOffset)
		{
			// Logic here is a bit messy. The client is always passing in a date at midnight
			// If user passes in 12/1/2020 into the date with range of 1, the range should be 12/1/2020:00:00:00 to 12/1/2020:23:59:59
			// Range 2 should be 11/30/2020:00:00:00 to 12/1/2020:23:59:59
			int offset = tzOffset ?? 0;
			DateTimeOffset endDateOffset = new DateTimeOffset(endDate, TimeSpan.FromHours(offset)).Add(new TimeSpan(23, 59, 59));
			DateTimeOffset startDateOffset = endDate.Subtract(new TimeSpan(range - 1, 0, 0, 0));

			List<IUtilizationData> telemetry = await _utilizationDataCollection.GetUtilizationDataAsync(startDateOffset.UtcDateTime, endDateOffset.UtcDateTime);

			return telemetry.ConvertAll(CreateTelemetryResponse);
		}

		static UtilizationTelemetryResponse CreateTelemetryResponse(IUtilizationData telemetry)
		{
			UtilizationTelemetryResponse response = new UtilizationTelemetryResponse();
			response.StartTime = telemetry.StartTime;
			response.FinishTime = telemetry.FinishTime;
			response.AdminTime = telemetry.AdminTime;
			response.HibernatingTime = telemetry.HibernatingTime;
			response.NumAgents = telemetry.NumAgents;

			response.Pools.AddRange(telemetry.Pools.Select(CreatePoolTelemetryResponse));
			return response;
		}

		static UtilizationTelemetryPool CreatePoolTelemetryResponse(IPoolUtilizationData pool)
		{
			UtilizationTelemetryPool response = new UtilizationTelemetryPool();
			response.PoolId = pool.PoolId.ToString();
			response.NumAgents = pool.NumAgents;
			response.AdminTime = pool.AdminTime;
			response.HibernatingTime = pool.HibernatingTime;
			response.OtherTime = pool.OtherTime;
			response.Streams.AddRange(pool.Streams.Select(CreateStreamTelemetryResponse));
			return response;
		}

		static UtilizationTelemetryStream CreateStreamTelemetryResponse(IStreamUtilizationData stream)
		{
			UtilizationTelemetryStream response = new UtilizationTelemetryStream();
			response.StreamId = stream.StreamId.ToString();
			response.Time = stream.Time;
			return response;
		}
	}
}
