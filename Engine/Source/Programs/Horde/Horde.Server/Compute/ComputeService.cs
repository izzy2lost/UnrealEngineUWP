// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.Metrics;
using System.Linq;
using System.Net;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Api;
using EpicGames.Horde.Compute;
using EpicGames.Horde.Compute.Clients;
using EpicGames.Horde.Compute.Transports;
using Google.Protobuf;
using Google.Protobuf.WellKnownTypes;
using Horde.Server.Agents;
using Horde.Server.Jobs;
using Horde.Server.Logs;
using Horde.Server.Server;
using Horde.Server.Utilities;
using HordeCommon;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using OpenTelemetry.Trace;
using StackExchange.Redis;

namespace Horde.Server.Compute
{
	/// <summary>
	/// Outcome for a compute allocation request
	/// </summary>
	public enum AllocationOutcome
	{
		/// <summary>
		/// A lease was allocated
		/// </summary>
		Accepted,
		
		/// <summary>
		/// A lease was not allocated
		/// </summary>
		Denied
	}
	
	/// <summary>
	/// Assigns compute leases to agents
	/// </summary>
	public sealed class ComputeService : IHostedService, IDisposable
	{
		/// <summary>
		/// Time-to-live for each bucket of request (currently grouped per minute)
		/// </summary>
		private readonly TimeSpan _requestLogTtl = TimeSpan.FromMinutes(5);
		
		/// <summary>
		/// How often the queue metric of allocation requests should be calculated
		/// </summary>
		private readonly TimeSpan _requestLogMetricInterval = TimeSpan.FromMinutes(1);
		
		readonly IAgentCollection _agentCollection;
		readonly ILogFileService _logService;
		readonly AgentService _agentService;
		readonly RedisService _redisService;
		readonly IClock _clock;
		readonly Tracer _tracer;
		readonly Counter<int> _allocationsAcceptedCount;
		readonly Counter<int> _allocationsDeniedCount;
		readonly ITicker _ticker;
		readonly ILogger<ComputeService> _logger;
		
		List<Measurement<int>> _measurements = new ();

		/// <summary>
		/// Constructor
		/// </summary>
		public ComputeService(IAgentCollection agentCollection, ILogFileService logService, AgentService agentService, RedisService redisService, IClock clock, Tracer tracer, Meter meter, ILogger<ComputeService> logger)
		{
			_agentCollection = agentCollection;
			_logService = logService;
			_agentService = agentService;
			_redisService = redisService;
			_clock = clock;
			_tracer = tracer;
			_ticker = clock.AddSharedTicker<ComputeService>(_requestLogMetricInterval, TickSharedAsync, logger);
			_logger = logger;
			
			_allocationsAcceptedCount = meter.CreateCounter<int>("horde.compute.allocations.accepted");
			_allocationsDeniedCount = meter.CreateCounter<int>("horde.compute.allocations.denied");
			meter.CreateObservableGauge("horde.compute.allocations.unserved", () =>
			{
				List<Measurement<int>> temp = new(_measurements);
				_measurements.Clear();
				return temp;
			});
		}
		
		/// <inheritdoc/>
		public Task StartAsync(CancellationToken cancellationToken)
		{
			return _ticker.StartAsync();
		}

		/// <inheritdoc/>
		public Task StopAsync(CancellationToken cancellationToken)
		{
			return _ticker.StopAsync();
		}
		
		/// <inheritdoc/>
		public void Dispose()
		{
			_ticker.Dispose();
		}

		private async ValueTask TickSharedAsync(CancellationToken stoppingToken)
		{
			List<RequestInfo> unservedRequestIds = await GetUnservedRequestsAsync();

			Dictionary<string, int> poolsWithUnservedRequestCounts = GroupByPoolAndCount(unservedRequestIds);
			List<Measurement<int>> newMeasurements = new();
			foreach ((string poolId, int reqCount) in poolsWithUnservedRequestCounts)
			{
				_logger.LogDebug("Unserved request count for {Pool}: {Count}", poolId, reqCount);
				newMeasurements.Add(new Measurement<int>(reqCount, new KeyValuePair<string, object?>("pool", poolId)));
			}

			_measurements = newMeasurements;
		}

		/// <summary>
		/// Allocates a compute resource
		/// </summary>
		public async Task<ComputeResource?> TryAllocateResourceAsync(string? requestId, Requirements requirements, LeaseId? parentLeaseId, CancellationToken cancellationToken)
		{
			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(ComputeService)}.{nameof(TryAllocateResourceAsync)}");
			span.SetAttribute("requestId", requestId);
			span.SetAttribute("parentLeaseId", parentLeaseId?.ToString());
			span.SetAttribute("req.pool", requirements.Pool);
			span.SetAttribute("req.condition", requirements.Condition?.ToString());
			span.SetAttribute("req.exclusive", requirements.Exclusive);

			foreach ((string name, ResourceRequirements resReq) in requirements.Resources)
			{
				span.SetAttribute($"req.res.{name}.min", resReq.Min);
				span.SetAttribute($"req.res.{name}.max", resReq.Max);
			}

			List<IAgent> agents = await _agentCollection.FindAsync();
			foreach (IAgent agent in agents)
			{
				Dictionary<string, int> assignedResources = new Dictionary<string, int>();
				if (agent.MeetsRequirements(requirements, assignedResources))
				{
					LeaseId leaseId = new LeaseId(BinaryIdUtils.CreateNew());
					ILogFile? log = await _logService.CreateLogFileAsync(JobId.Empty, leaseId, agent.SessionId, LogType.Json, useNewStorageBackend: true, cancellationToken: cancellationToken);

					ComputeTask computeTask = CreateComputeTask(assignedResources, log?.Id, parentLeaseId);

					byte[] payload = Any.Pack(computeTask).ToByteArray();
					AgentLease lease = new AgentLease(leaseId, parentLeaseId, "Compute task", null, null, log?.Id, LeaseState.Pending, assignedResources, requirements.Exclusive, payload);

					ComputeResource? resource = TryAssign(agent, computeTask, leaseId);
					if (resource != null)
					{
						IAgent? newAgent = await _agentCollection.TryAddLeaseAsync(agent, lease);
						if (newAgent != null)
						{
							await _agentCollection.PublishUpdateEventAsync(agent.Id);
							await _agentService.CreateLeaseAsync(newAgent, lease);
							span.SetAttribute("allocatedLeaseId", leaseId.ToString());
							span.SetAttribute("allocatedAgentId", newAgent.Id.ToString());

							await LogRequestAsync(AllocationOutcome.Accepted, requestId, requirements, parentLeaseId, span);
							return resource;
						}
					}
				}
			}

			await LogRequestAsync(AllocationOutcome.Denied, requestId, requirements, parentLeaseId, span);
			return null;
		}

		private static string RedisKeyComputeRequests(DateTimeOffset timestamp)
		{
			long unixTime = timestamp.ToUnixTimeSeconds();
			long unixTimeClosestMin = unixTime - unixTime % 60; // Round to closest starting minute (buckets per minute)
			return $"compute/requests/{unixTimeClosestMin}";
		}
		
		/// <summary>
		/// Record to be stored in Redis for representing a compute allocation request
		/// </summary>
		/// <param name="Timestamp"></param>
		/// <param name="Outcome"></param>
		/// <param name="RequestId"></param>
		/// <param name="Pool"></param>
		/// <param name="ParentLeaseId"></param>
		internal record RequestInfo(DateTimeOffset Timestamp, AllocationOutcome Outcome, string RequestId, string Pool, string? ParentLeaseId)
		{
			private const int Version = 1; 
			
			public string Serialize()
			{
				StringBuilder sb = new (100);
				sb.Append(Version).Append('\t');
				sb.Append(Timestamp.ToUnixTimeSeconds()).Append('\t');
				sb.Append(Outcome).Append('\t');
				sb.Append(RequestId).Append('\t');
				sb.Append(Pool).Append('\t');
				sb.Append(ParentLeaseId);
				return sb.ToString();
			}

			public static RequestInfo? Deserialize(RedisValue value) { return Deserialize(value.ToString()); }
			public static RequestInfo? Deserialize(string value)
			{
				string[] parts = value.Split("\t");
				if (parts.Length != 6)
				{
					return null;
				}

				if (!Int32.TryParse(parts[0], out int version) || version != Version)
				{
					return null;
				}

				if (!Int64.TryParse(parts[1], out long unixTimeSec))
				{
					return null;
				}

				if (!System.Enum.TryParse(parts[2], out AllocationOutcome outcome))
				{
					return null;
				}

				return new RequestInfo(DateTimeOffset.FromUnixTimeSeconds(unixTimeSec), outcome, parts[3], parts[4], String.IsNullOrEmpty(parts[5]) ? null : parts[5]);
			}
		}
		
		internal async Task LogRequestAsync(AllocationOutcome outcome, string? requestId, Requirements requirements, LeaseId? parentLeaseId, TelemetrySpan currentSpan)
		{
			int? numActiveLeases = await GetNumActiveLeasesAsync(parentLeaseId);
			currentSpan.SetAttribute("numActiveLeases", numActiveLeases);
			
			KeyValuePair<string, object?> poolTag = new ("pool", requirements.Pool);
			KeyValuePair<string, object?> activeLeasesTag = new ("activeLeases", numActiveLeases?.ToString() ?? "null");

			if (requestId != null && requirements.Pool != null)
			{
				DateTimeOffset timestamp = new (_clock.UtcNow);
				string key = RedisKeyComputeRequests(timestamp);
				RequestInfo requestInfo = new (timestamp, outcome, requestId, requirements.Pool, parentLeaseId?.ToString());
				await _redisService.GetDatabase().ListRightPushAsync(key, requestInfo.Serialize());
				await _redisService.GetDatabase().KeyExpireAsync(key, _requestLogTtl);
			}

			switch (outcome)
			{
				case AllocationOutcome.Accepted: _allocationsAcceptedCount.Add(1, poolTag, activeLeasesTag); break;
				case AllocationOutcome.Denied: _allocationsDeniedCount.Add(1, poolTag, activeLeasesTag); break;
				default: throw new Exception("Invalid outcome " + outcome);
			}
		}

		internal async Task<List<RequestInfo>> GetUnservedRequestsAsync()
		{
			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(ComputeService)}.{nameof(GetUnservedRequestsAsync)}");
			IDatabase redis = _redisService.GetDatabase();
			DateTime utcNow = _clock.UtcNow;
			DateTimeOffset startTime = new (utcNow - TimeSpan.FromMinutes(1));
			DateTimeOffset endTime = new (utcNow);
			
			RedisValue[] lastMinValues = await redis.ListRangeAsync(RedisKeyComputeRequests(startTime));
			RedisValue[] currentMinValues = await redis.ListRangeAsync(RedisKeyComputeRequests(endTime));
			List<RedisValue> values = new (lastMinValues.Concat(currentMinValues));
			List<RequestInfo> requestInfos = values
				.Select(RequestInfo.Deserialize)
				.OfType<RequestInfo>()
				.Where(x => x.Timestamp >= startTime && x.Timestamp < endTime)
				.OrderBy(x => x.Timestamp)
				.ToList();

			Dictionary<string, RequestInfo> idToInfo = new();
			foreach (RequestInfo ri in requestInfos)
			{
				if (!idToInfo.TryGetValue(ri.RequestId, out RequestInfo? lastSeen) || lastSeen.Outcome == AllocationOutcome.Denied)
				{
					idToInfo[ri.RequestId] = ri;
				}
			}
			
			List<RequestInfo> results = idToInfo
				.Where(pair => pair.Value.Outcome == AllocationOutcome.Denied)
				.Select(pair => pair.Value)
				.ToList();

			span.SetAttribute("numCurrentMin", currentMinValues.Length);
			span.SetAttribute("numLastMin", lastMinValues.Length);
			span.SetAttribute("numResults", results.Count);
			return results;
		}

		internal static Dictionary<string, int> GroupByPoolAndCount(List<RequestInfo> requests)
		{
			return requests
				.GroupBy(ri => ri.Pool)
				.ToDictionary(group => group.Key, group => group.Count());
		}

		/// <summary>
		/// Get the number of currently active leases belonging to the given parent lease.
		/// Allows compute allocation requests metric to be broken down by lease.
		/// </summary>
		/// <param name="parentLeaseId"></param>
		/// <returns>Number of active leases</returns>
		private async Task<int?> GetNumActiveLeasesAsync(LeaseId? parentLeaseId)
		{
			if (parentLeaseId == null)
			{
				return null;
			}

			List<LeaseId> childLeaseIds = await _agentCollection.GetChildLeaseIds(parentLeaseId.Value);
			return childLeaseIds.Count;
		}

		static ComputeResource? TryAssign(IAgent agent, ComputeTask computeTask, LeaseId leaseId)
		{
			string? ipStr = agent.GetPropertyValues("ComputeIp").FirstOrDefault();
			if (ipStr == null || !IPAddress.TryParse(ipStr, out IPAddress? ip))
			{
				return null;
			}

			string? portStr = agent.GetPropertyValues("ComputePort").FirstOrDefault();
			if (portStr == null || !Int32.TryParse(portStr, out int port))
			{
				return null;
			}

			return new ComputeResource(ip, port, computeTask, agent.Properties, agent.Id, leaseId);
		}

		static ComputeTask CreateComputeTask(Dictionary<string, int> assignedResources, LogId? logId, LeaseId? parentLeaseId)
		{
			ComputeTask computeTask = new ComputeTask();
			computeTask.Nonce = UnsafeByteOperations.UnsafeWrap(RandomNumberGenerator.GetBytes(ServerComputeClient.NonceLength));
			computeTask.Key = UnsafeByteOperations.UnsafeWrap(AesTransport.CreateKey());
			computeTask.Resources.Add(assignedResources);
			computeTask.LogId = logId?.ToString();
			computeTask.ParentLeaseId = parentLeaseId?.ToString() ?? String.Empty;
			return computeTask;
		}
	}
}
