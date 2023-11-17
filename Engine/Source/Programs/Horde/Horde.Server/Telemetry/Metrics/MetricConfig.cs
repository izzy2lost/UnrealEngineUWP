// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Text.Json.Serialization;
using EpicGames.Core;
using Horde.Server.Utilities;
using Json.Path;

namespace Horde.Server.Telemetry.Metrics
{
	/// <summary>
	/// Method for aggregating samples into a metric
	/// </summary>
	public enum AggregationFunction
	{
		/// <summary>
		/// Count the number of matching elements
		/// </summary>
		Count,

		/// <summary>
		/// Take the minimum value of all samples
		/// </summary>
		Min,

		/// <summary>
		/// Take the maximum value of all samples
		/// </summary>
		Max,

		/// <summary>
		/// Sum all the reported values
		/// </summary>
		Sum,

		/// <summary>
		/// Average all the samples
		/// </summary>
		Average,

		/// <summary>
		/// Estimates the value at a certain percentile
		/// </summary>
		Percentile,
	}

	/// <summary>
	/// Configures a metric to aggregate on the server
	/// </summary>
	public class MetricConfig
	{
		/// <summary>
		/// Identifier for this metric
		/// </summary>
		public MetricId Id { get; set; }

		/// <summary>
		/// Property to aggregate
		/// </summary>
		[JsonSchemaString]
		public JsonPath? Property { get; set; }

		/// <summary>
		/// Property to group by
		/// </summary>
		[JsonSchemaString]
		public JsonPath? GroupBy { get; set; }

		/// <summary>
		/// How to aggregate samples for this metric
		/// </summary>
		public AggregationFunction Function { get; set; }

		/// <summary>
		/// For the percentile function, specifies the percentile to measure
		/// </summary>
		public int Percentile { get; set; } = 95;

		/// <summary>
		/// Interval for each metric. Supports times such as "2d", "1h", "1h30m", "20s".
		/// </summary>
		[JsonConverter(typeof(IntervalJsonConverter))]
		public TimeSpan Interval { get; set; } = TimeSpan.FromHours(1.0);
	}
}
