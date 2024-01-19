// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text.Json;
using System.Text.Json.Serialization;
using EpicGames.Horde.Common;
using EpicGames.Horde.Compute;
using Horde.Server.Agents.Fleet;

namespace Horde.Server.Agents.Pools
{
	/// <summary>
	/// Metadata for configuring and picking a pool sizing strategy
	/// </summary>
	public class PoolSizeStrategyInfo
	{
		/// <summary>
		/// Strategy implementation to use
		/// </summary>
		public PoolSizeStrategy Type { get; set; }
		
		/// <summary>
		/// Condition if this strategy should be enabled (right now, using date/time as a distinguishing factor)
		/// </summary>
		public Condition? Condition { get; set; }

		/// <summary>
		/// Configuration for the strategy, serialized as JSON
		/// </summary>
		public string Config { get; set; } = "";
		
		/// <summary>
		/// Integer to add after pool size has been calculated. Can also be negative.
		/// </summary>
		public int ExtraAgentCount { get; set; } = 0;

		/// <summary>
		/// Empty constructor for JSON serialization
		/// </summary>
		public PoolSizeStrategyInfo()
		{
		}
		
		/// <summary>
		/// Constructor
		/// </summary>
		public PoolSizeStrategyInfo(PoolSizeStrategy type, Condition? condition, string? config, int extraAgentCount = 0)
		{
			config = String.IsNullOrWhiteSpace(config) ? "{}" : config;

			// Try deserializing to ensure the config is valid JSON
			// Config can be null due to JSON serializer calling the constructor
			JsonSerializer.Deserialize<dynamic>(config);
			
			Type = type;
			Condition = condition;
			Config = config;
			ExtraAgentCount = extraAgentCount;
		}
	}
	
	/// <summary>
	/// Metadata for configuring and picking a fleet manager
	/// </summary>
	public class FleetManagerInfo
	{
		/// <summary>
		/// Fleet manager type implementation to use
		/// </summary>
		public FleetManagerType Type { get; set; }
		
		/// <summary>
		/// Condition if this strategy should be enabled (right now, using date/time as a distinguishing factor)
		/// </summary>
		public Condition? Condition { get; set; }

		/// <summary>
		/// Configuration for the strategy, serialized as JSON
		/// </summary>
		public string Config { get; set; } = "{}";

		/// <summary>
		/// Empty constructor for BSON/JSON serialization
		/// </summary>
		public FleetManagerInfo()
		{
		}
		
		/// <summary>
		/// Constructor
		/// </summary>
		[JsonConstructor]
		public FleetManagerInfo(FleetManagerType type, Condition? condition, string? config)
		{
			config = String.IsNullOrWhiteSpace(config) ? "{}" : config;
			JsonSerializer.Deserialize<dynamic>(config);
			
			Type = type;
			Condition = condition;
			Config = config;
		}
	}

	/// <summary>
	/// Pool of machines
	/// </summary>
	public interface IPool : IPoolConfig
	{
		/// <summary>
		/// List of workspaces currently assigned to this pool
		/// </summary>
		public IReadOnlyList<AgentWorkspace> Workspaces { get; }

		/// <summary>
		/// Last time the pool was (auto) scaled up
		/// </summary>
		public DateTime? LastScaleUpTime { get; }

		/// <summary>
		/// Last time the pool was (auto) scaled down
		/// </summary>
		public DateTime? LastScaleDownTime { get; }

		/// <summary>
		/// Last known agent count
		/// </summary>
		public int? LastAgentCount { get; }

		/// <summary>
		/// Last known desired agent count
		/// </summary>
		public int? LastDesiredAgentCount { get; }

		/// <summary>
		/// Last result from scaling the pool
		/// </summary>
		public ScaleResult? LastScaleResult { get; }

		/// <summary>
		/// Revision string for the config file describing this pool, or null if added manually.
		/// </summary>
		public string? Revision { get; }

		/// <summary>
		/// Update index for this document
		/// </summary>
		public int UpdateIndex { get; }
	}

	/// <summary>
	/// Extension methods for IPool
	/// </summary>
	public static class PoolExtensions
	{
		record struct RgbColor(byte R, byte G, byte B)
		{
			public static RgbColor Lerp(RgbColor lhs, RgbColor rhs, float t)
				=> new RgbColor((byte)(lhs.R + (rhs.R - lhs.R) * t), (byte)(lhs.G + (rhs.G - lhs.G) * t), (byte)(lhs.B + (rhs.B - lhs.B) * t));

			public string ToHexString() => $"#{R:x2}{G:x2}{B:x2}";
		}

		static readonly RgbColor[] s_colorTable =
		{
			new RgbColor(0x00, 0xbc, 0xf2),
			new RgbColor(0x5a, 0xc9, 0x5a),
			new RgbColor(0xff, 0x66, 0x00),
			new RgbColor(0xdf, 0x8b, 0xe5),
			new RgbColor(0x00, 0xbc, 0xf2),
		};

		/// <summary>
		/// Gets the color for a pool
		/// </summary>
		public static string GetColorValue(this IPool pool)
		{
			// Get the desired color from the properties object on the pool config
			float slider = 0.0f;
			if (pool.Properties.TryGetValue("Color", out string? colorText) && uint.TryParse(colorText, out uint colorInt))
			{
				slider = colorInt / 600.0f;
			}

			// Convert to an index/lerp value
			float value = slider * (s_colorTable.GetLength(0) - 1);
			int idx = Math.Clamp((int)value, 0, s_colorTable.Length - 2);
			float t = Math.Clamp(value - idx, 0.0f, 1.0f);

			// Create the final rgb value
			return RgbColor.Lerp(s_colorTable[idx], s_colorTable[idx + 1], t).ToHexString();
		}

		/// <summary>
		/// Evaluates a condition against a pool
		/// </summary>
		/// <param name="pool">The pool to evaluate</param>
		/// <param name="condition">The condition to evaluate</param>
		/// <returns>True if the pool satisfies the condition</returns>
		public static bool SatisfiesCondition(this IPool pool, Condition condition)
		{
			return condition.Evaluate(propKey =>
			{
				if (pool.Properties.TryGetValue(propKey, out string? propValue))
				{
					return new[] { propValue };
				}

				return Array.Empty<string>();
			});
		}

		/// <summary>
		/// Determine whether a pool offers agents that are compatible with the given requirements.
		/// Since a pool does not have beforehand knowledge of what type of agents will be created, their expected
		/// properties must be specified on the IPool. 
		/// </summary>
		/// <param name="pool">The pool to verify</param>
		/// <param name="requirements">Requirements</param>
		/// <returns>True if the pool satisfies the requirements</returns>
		public static bool MeetsRequirements(this IPool pool, Requirements requirements)
		{
			if (requirements.Condition != null && !pool.SatisfiesCondition(requirements.Condition))
			{
				return false;
			}

			return true;
		}
	}
}
