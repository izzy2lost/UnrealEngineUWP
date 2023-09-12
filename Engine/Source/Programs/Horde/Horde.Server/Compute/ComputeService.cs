// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.Metrics;
using System.Linq;
using System.Net;
using System.Security.Cryptography;
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
using Horde.Server.Utilities;
using HordeCommon;
using HordeCommon.Rpc.Tasks;
using OpenTelemetry.Trace;

namespace Horde.Server.Compute
{
	/// <summary>
	/// Assigns compute leases to agents
	/// </summary>
	public class ComputeService
	{
		readonly IAgentCollection _agentCollection;
		readonly ILogFileService _logService;
		readonly AgentService _agentService;
		readonly Tracer _tracer;
		readonly Counter<int> _allocationsAcceptedCount;
		readonly Counter<int> _allocationsDeniedCount;

		/// <summary>
		/// Constructor
		/// </summary>
		public ComputeService(IAgentCollection agentCollection, ILogFileService logService, AgentService agentService, Tracer tracer, Meter meter)
		{
			_agentCollection = agentCollection;
			_logService = logService;
			_agentService = agentService;
			_tracer = tracer;
			_allocationsAcceptedCount = meter.CreateCounter<int>("horde.compute.allocations.accepted");
			_allocationsDeniedCount = meter.CreateCounter<int>("horde.compute.allocations.denied");
		}

		/// <summary>
		/// Allocates a compute resource
		/// </summary>
		public async Task<ComputeResource?> TryAllocateResourceAsync(Requirements requirements, LeaseId? parentLeaseId, CancellationToken cancellationToken)
		{
			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(ComputeService)}.{nameof(TryAllocateResourceAsync)}");
			span.SetAttribute("parentLeaseId", parentLeaseId?.ToString());
			span.SetAttribute("req.pool", requirements.Pool);
			span.SetAttribute("req.condition", requirements.Condition?.ToString());
			span.SetAttribute("req.exclusive", requirements.Exclusive);

			foreach ((string name, ResourceRequirements resReq) in requirements.Resources)
			{
				span.SetAttribute($"req.res.{name}.min", resReq.Min);
				span.SetAttribute($"req.res.{name}.max", resReq.Max);
			}
			
			KeyValuePair<string, object?> poolTag = new ("pool", requirements.Pool);

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
							_allocationsAcceptedCount.Add(1, poolTag);
							return resource;
						}
					}
				}
			}
			_allocationsDeniedCount.Add(1, poolTag);
			return null;
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
