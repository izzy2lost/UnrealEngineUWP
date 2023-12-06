// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json;
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
		/// Filter expression to evaluate to determine which events to include. This query is evaluated against an array.
		/// </summary>
		[JsonSchemaString]
		public JsonPath? Filter { get; set; }

		/// <summary>
		/// Property to aggregate
		/// </summary>
		[JsonSchemaString]
		public JsonPath? Property { get; set; }

		/// <summary>
		/// Property to group by
		/// </summary>
		[JsonSchemaString]
		[JsonConverter(typeof(MetricGroupJsonConverter))]
		public List<JsonPath> GroupBy { get; set; } = new List<JsonPath>();

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

	/// <summary>
	/// Converter for a list of json path expressions, separated by commas
	/// </summary>
	class MetricGroupJsonConverter : JsonConverter<List<JsonPath>>
	{
		/// <inheritdoc/>
		public override List<JsonPath>? Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options)
		{
			string? str = reader.GetString();
			if (String.IsNullOrWhiteSpace(str))
			{
				return null;
			}

			List<string> fields = str.Split(',').Select(x => x.Trim()).Where(x => x.Length > 0).ToList();
			if (fields.Count == 0)
			{
				return null;
			}

			return fields.ConvertAll(x => JsonPath.Parse(x));
		}

		/// <inheritdoc/>
		public override void Write(Utf8JsonWriter writer, List<JsonPath> value, JsonSerializerOptions options)
		{
			writer.WriteStringValue(string.Join(",", value.Select(x => x.ToString())));
		}
	}
}
