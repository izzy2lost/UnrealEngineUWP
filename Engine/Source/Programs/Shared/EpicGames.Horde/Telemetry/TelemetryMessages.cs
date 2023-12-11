// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;
using System.Threading.Tasks;

#pragma warning disable CA2227

namespace EpicGames.Horde.Telemetry
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
	/// Metrics matching a particular query
	/// </summary>
	public class GetTelemetryMetricsResponse
	{
		/// <summary>
		/// Metrics matching the search terms
		/// </summary>
		public List<GetTelemetryMetricResponse> Metrics { get; set; } = new List<GetTelemetryMetricResponse>();
	}

	/// <summary>
	/// Information about a particular metric
	/// </summary>
	public class GetTelemetryMetricResponse
	{
		/// <summary>
		/// Start time for the sample
		/// </summary>
		public DateTime Time { get; set; }

		/// <summary>
		/// Name of the group
		/// </summary>
		public string? Group { get; set; }

		/// <summary>
		/// Value for the metric
		/// </summary>
		public double Value { get; set; }
	}
}
