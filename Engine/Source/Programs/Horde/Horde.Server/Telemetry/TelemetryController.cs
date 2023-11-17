// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.Text.Json.Nodes;
using Horde.Server.Utilities;
using Microsoft.AspNetCore.Mvc;

namespace Horde.Server.Telemetry
{
	/// <summary>
	/// Generic message for a telemetry event
	/// </summary>
	public class PostTelemetryEventStreamRequest
	{
		/// <summary>
		/// List of telemetry events
		/// </summary>
		public List<JsonObject> Events { get; set; } = new List<JsonObject>();
	}

	/// <summary>
	/// Indicates the type of telemetry data being uploaded
	/// </summary>
	public enum TelemetryUploadType
	{
		/// <summary>
		/// A batch of <see cref="PostTelemetryEventStreamRequest"/> objects.
		/// </summary>
		EtEventStream
	}

	/// <summary>
	/// Controller for the /api/v1/telemetry endpoint
	/// </summary>
	[ApiController]
	[Route("[controller]")]
	public class TelemetryController : HordeControllerBase
	{
		readonly TelemetryManager _telemetryManager;

		/// <summary>
		/// Constructor
		/// </summary>
		public TelemetryController(TelemetryManager telemetryManager)
		{
			_telemetryManager = telemetryManager;
		}

		/// <summary>
		/// Posts a new telemetry event. This API is modeled after Epic's external data router, allowing events in the engine to be sent to Horde using the same mechanism.
		/// </summary>
		/// <param name="request">The event data</param>
		/// <param name="appId">Identifier of the application sending the event</param>
		/// <param name="appVersion">Version number of the application</param>
		/// <param name="appEnvironment">Name of the environment that the sending application is running in</param>
		/// <param name="uploadType">Type of data being uploaded</param>
		[HttpPost]
		[Route("/api/v1/telemetry")]
		public ActionResult PostEvent([FromBody] PostTelemetryEventStreamRequest request, [FromQuery][Required] string appId, [FromQuery][Required] string appVersion, [FromQuery][Required] string appEnvironment, [FromQuery][Required] TelemetryUploadType uploadType)
		{
			if (uploadType != TelemetryUploadType.EtEventStream)
			{
				return BadRequest("Invalid upload type");
			}

			TelemetryRecordMeta recordMeta = new TelemetryRecordMeta(AppId: appId, AppVersion: appVersion, AppEnvironment: appEnvironment);
			foreach (JsonObject eventPayload in request.Events)
			{
				_telemetryManager.SendEvent(recordMeta, eventPayload);
			}

			return NoContent();
		}
	}
}
