// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using EpicGames.Horde.Agents.Pools;
using EpicGames.Horde.Common;
using Horde.Server.Agents.Fleet;

namespace Horde.Server.Agents.Pools
{
	/// <summary>
	/// Configuration for a pool of machines
	/// </summary>
	public interface IPoolConfig
	{
		/// <summary>
		/// Unique id for this pool
		/// </summary>
		public PoolId Id { get; }

		/// <summary>
		/// Name of the pool
		/// </summary>
		public string Name { get; }

		/// <summary>
		/// Condition for agents to automatically be included in this pool
		/// </summary>
		public Condition? Condition { get; }

		/// <summary>
		/// Arbitrary properties related to this pool
		/// </summary>
		public IReadOnlyDictionary<string, string> Properties { get; }

		/// <summary>
		/// Whether to enable autoscaling for this pool
		/// </summary>
		public bool EnableAutoscaling { get; }

		/// <summary>
		/// The minimum number of agents to keep in the pool
		/// </summary>
		public int? MinAgents { get; }

		/// <summary>
		/// The minimum number of idle agents to hold in reserve
		/// </summary>
		public int? NumReserveAgents { get; }

		/// <summary>
		/// Interval between conforms. If zero, the pool will not conform on a schedule.
		/// </summary>
		public TimeSpan? ConformInterval { get; }
		
		/// <summary>
		/// Time to wait before shutting down an agent that has been disabled
		/// </summary>
		public TimeSpan? ShutdownIfDisabledGracePeriod { get; }

		/// <inheritdoc/>
		[Obsolete("Use SizeStrategies instead")]
		public PoolSizeStrategy? SizeStrategy { get; set; }

		/// <summary>
		/// List of pool sizing strategies for this pool. The first strategy with a matching condition will be picked.
		/// </summary>
		public IReadOnlyList<PoolSizeStrategyInfo>? SizeStrategies { get; }

		/// <summary>
		/// List of fleet managers for this pool. The first strategy with a matching condition will be picked.
		/// If empty or no conditions match, a default fleet manager will be used.
		/// </summary>
		public IReadOnlyList<FleetManagerInfo>? FleetManagers { get; }

		/// <summary>
		/// Settings for lease utilization pool sizing strategy (if used)
		/// </summary>
		public LeaseUtilizationSettings? LeaseUtilizationSettings { get; }

		/// <summary>
		/// Settings for job queue pool sizing strategy (if used)
		/// </summary>
		public JobQueueSettings? JobQueueSettings { get; }

		/// <summary>
		/// Settings for job queue pool sizing strategy (if used)
		/// </summary>
		public ComputeQueueAwsMetricSettings? ComputeQueueAwsMetricSettings { get; }
	}

	/// <summary>
	/// Mutable configuration for a pool
	/// </summary>
	[DebuggerDisplay("{Id}")]
	public class PoolConfig : IPoolConfig
	{
		/// <inheritdoc/>
		public PoolId Id { get; set; }

		/// <summary>
		/// Base pool config to copy settings from
		/// </summary>
		public PoolId? Base { get; set; }
		
		/// <inheritdoc/>
		public string Name { get; set; } = String.Empty;

		/// <inheritdoc/>
		public Condition? Condition { get; set; }

		/// <inheritdoc cref="IPoolConfig.Properties"/>
		public Dictionary<string, string> Properties { get; set; } = new Dictionary<string, string>();

		/// <inheritdoc/>
		IReadOnlyDictionary<string, string> IPoolConfig.Properties => Properties;

		/// <inheritdoc cref="IPoolConfig.EnableAutoscaling"/>
		public bool? EnableAutoscaling { get; set; }

		/// <inheritdoc/>
		bool IPoolConfig.EnableAutoscaling => EnableAutoscaling ?? true;

		/// <inheritdoc/>
		public int? MinAgents { get; set; }

		/// <inheritdoc/>
		public int? NumReserveAgents { get; set; }

		/// <inheritdoc/>
		public TimeSpan? ConformInterval { get; set; }

		/// <inheritdoc/>
		public DateTime? LastScaleUpTime { get; set; }

		/// <inheritdoc/>
		public DateTime? LastScaleDownTime { get; set; }

		/// <inheritdoc/>
		public TimeSpan? ScaleOutCooldown { get; set; }

		/// <inheritdoc/>
		public TimeSpan? ScaleInCooldown { get; set; }
		
		/// <inheritdoc/>
		public TimeSpan? ShutdownIfDisabledGracePeriod { get; set;  }

		/// <inheritdoc/>
		[Obsolete("Use SizeStrategies instead")]
		public PoolSizeStrategy? SizeStrategy{ get; set; }

		/// <inheritdoc/>
		public List<PoolSizeStrategyInfo>? SizeStrategies { get; set; }

		/// <inheritdoc cref="IPoolConfig.SizeStrategies"/>
		IReadOnlyList<PoolSizeStrategyInfo>? IPoolConfig.SizeStrategies => SizeStrategies;

		/// <inheritdoc/>
		public List<FleetManagerInfo>? FleetManagers { get; set; }

		/// <inheritdoc cref="IPoolConfig.FleetManagers"/>
		IReadOnlyList<FleetManagerInfo>? IPoolConfig.FleetManagers => FleetManagers;

		/// <inheritdoc/>
		public LeaseUtilizationSettings? LeaseUtilizationSettings { get; set; }

		/// <inheritdoc/>
		public JobQueueSettings? JobQueueSettings { get; set; }

		/// <inheritdoc/>
		public ComputeQueueAwsMetricSettings? ComputeQueueAwsMetricSettings { get; set; }
	}
}
