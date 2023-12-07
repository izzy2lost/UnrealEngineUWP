// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
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
		/// Property to group by. Specified as a comma-separated list of JSON path expressions.
		/// </summary>
		public string GroupBy
		{
			get => _groupBy;
			set 
			{
				_groupBy = value;

				GroupByPaths.Clear();
				if (!String.IsNullOrWhiteSpace(_groupBy))
				{
					List<string> fields = _groupBy.Split(',').Select(x => x.Trim()).Where(x => x.Length > 0).ToList();
					if (fields.Count > 0)
					{
						GroupByPaths.AddRange(fields.Select(x => JsonPath.Parse(x)));
					}
				}
			}
		}

		[JsonIgnore]
		string _groupBy = String.Empty;

		/// <summary>
		/// Accessor for the <see cref="GroupBy"/> field, parsed as a list of json paths
		/// </summary>
		[JsonIgnore]
		public List<JsonPath> GroupByPaths { get; } = new List<JsonPath>();

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
