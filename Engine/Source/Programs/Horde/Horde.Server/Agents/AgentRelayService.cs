// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Collections.Immutable;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Redis;
using Google.Protobuf;
using Google.Protobuf.WellKnownTypes;
using Grpc.Core;
using Horde.Common.Rpc;
using Horde.Server.Server;
using HordeCommon;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using StackExchange.Redis;

namespace Horde.Server.Agents;

/// <summary>
/// Exception thrown by agent relay related code
/// </summary>
public class AgentRelayException : Exception
{
	/// <summary>
	/// Constructor
	/// </summary>
	/// <param name="message"></param>
	public AgentRelayException(string? message) : base(message)
	{
	}
}

/// <summary>
/// Result of a requested port forward for a specific agent
/// </summary>
public class PortMappingResult
{
	/// <summary>
	/// IP addresses to relays that are listening and forwarding traffic for the specified ports
	/// </summary>
	public IReadOnlyList<string> IpAddresses { get; }
	
	/// <summary>
	/// Mapping between relay and local agent ports
	/// </summary>
	public IReadOnlyList<Port> Ports { get; }

	/// <summary>
	/// Constructor
	/// </summary>
	/// <param name="ipAddresses"></param>
	/// <param name="ports"></param>
	public PortMappingResult(IReadOnlyList<string> ipAddresses, IReadOnlyList<Port> ports)
	{
		IpAddresses = ipAddresses;
		Ports = ports;
	}
}

/// <summary>
/// Service handling relaying of agent traffic
/// The Horde agent can be started in a special mode where it will relay traffic to and from other agents.
/// It connects back to this service over gRPC and mirror the current port mappings set by the server.
/// </summary>
public sealed class AgentRelayService : RelayRpc.RelayRpcBase, IHostedService
{
	private const string RedisChannelUpdate = "relay/update";
	private static string KeyPortMappings(string clusterId) => $"relay/port-mappings/{clusterId}";
	private static string KeyUsedPorts(string clusterId) => $"relay/used-ports/{clusterId}";
	private static string KeyAgents(string clusterId) => $"relay/agents/{clusterId}";

	private readonly RedisService _redis;
	private readonly IClock _clock;
	private readonly ILogger<AgentRelayService> _logger;
	private readonly object _lock = new();
	private TimeSpan _longPollTimeout = TimeSpan.FromSeconds(20);
	private TimeSpan _agentExpirationTimeout;
	private TaskCompletionSource<List<PortMapping>> _onPortMappingUpdated = new();

	private IAsyncDisposable? _redisSubscription;
	private int _minPort = 2000;
	private int _maxPort = 65000;
	
	/// <summary>
	/// Constructor
	/// </summary>
	/// <param name="redis"></param>
	/// <param name="clock"></param>
	/// <param name="logger"></param>
	public AgentRelayService(RedisService redis, IClock clock, ILogger<AgentRelayService> logger)
	{
		_redis = redis;
		_clock = clock;
		_logger = logger;
		_agentExpirationTimeout = _longPollTimeout + TimeSpan.FromSeconds(5);
	}
	
	/// <inheritdoc/>
	public async Task StartAsync(CancellationToken cancellationToken)
	{
		_redisSubscription = await SubscribeToUpdateEventAsync(OnPortMappingUpdateAsync);
	}
	
	/// <inheritdoc/>
	public async Task StopAsync(CancellationToken cancellationToken)
	{
		if (_redisSubscription != null)
		{
			await _redisSubscription.DisposeAsync();
			_redisSubscription = null;
		}
	}
	
	/// <summary>
	/// Retrieve a specific port mapping
	/// </summary>
	/// <param name="clusterId">Cluster ID</param>
	/// <param name="leaseId">Lease ID</param>
	/// <returns>The port mapping, or null if not found</returns>
	public async Task<PortMapping?> GetPortMappingAsync(string clusterId, string leaseId)
	{
		IDatabase redis = _redis.GetDatabase();
		RedisValue value = await redis.HashGetAsync(KeyPortMappings(clusterId), leaseId);
		return value.IsNullOrEmpty ? null : PortMapping.Parser.ParseFrom(value);
	}
	
	/// <summary>
	/// Retrieve all current port mappings
	/// </summary>
	/// <param name="clusterId">Cluster ID</param>
	/// <returns>List of all port mappings</returns>
	public async Task<List<PortMapping>> GetPortMappingsAsync(string clusterId)
	{
		IDatabase redis = _redis.GetDatabase();
		HashEntry[] entries = await redis.HashGetAllAsync(KeyPortMappings(clusterId));
		return entries.Select(x => PortMapping.Parser.ParseFrom(x.Value)).ToList();
	}
	
	/// <summary>
	/// Add a new port mapping
	/// </summary>
	/// <param name="clusterId">Cluster ID this mapping is for</param>
	/// <param name="leaseId">Lease ID this mapping is for</param>
	/// <param name="agentIp">What agent IP to forward all traffic to</param>
	/// <param name="ports">Ports agent is listening</param>
	/// <param name="numRetries">Number of retries before giving up</param>
	/// <returns>A port mapping with listen ports assigned</returns>
	/// <exception cref="Exception"></exception>
	public async Task<PortMapping> AddPortMappingAsync(string clusterId, string leaseId, string agentIp, IList<Port> ports, int numRetries = 10)
	{
		IDatabase redis = _redis.GetDatabase();

		// TODO: Randomize start position to spread out port use
		// TODO: Add pinging to and do not consider stale agent relays
		// TODO: Check failed deserialization from Redis
		
		for (int retryAttempt = 0; retryAttempt < numRetries; retryAttempt++)
		{
			HashSet<int> usedPorts = (await redis.SetMembersAsync(KeyUsedPorts(clusterId))).Select(x => (int)x).ToHashSet();
			int numPorts = ports.Count;
			IReadOnlySet<int> portRange = FindAvailablePortRange(usedPorts, numPorts, _minPort, _maxPort);
			if (portRange.Count == 0)
			{
				throw new Exception("No ports are available");
			}

			// Create a list of ports with the newly acquired ports
			IEnumerable<Port> newPorts = portRange
				.ToList()
				.OrderBy(x => x)
				.Zip(ports, (newRelayPort, port) => new Port { RelayPort = newRelayPort, AgentPort = port.AgentPort, Protocol = port.Protocol });
			
			PortMapping newPortMapping = new () { LeaseId = leaseId, AgentIp = agentIp };
			newPortMapping.Ports.AddRange(newPorts);

			ITransaction transaction = redis.CreateTransaction();
			RedisValue[] portRangeRedis = portRange.Select(x => Convert.ToString(x)).Select(x => new RedisValue(x)).ToArray();
			foreach (int port in portRange)
			{
				transaction.AddCondition(Condition.SetNotContains(KeyUsedPorts(clusterId), port));
			}
			
			_ = transaction.SetAddAsync(KeyUsedPorts(clusterId), portRangeRedis);
			_ = transaction.HashSetAsync(KeyPortMappings(clusterId), leaseId, newPortMapping.ToByteArray());
			bool isSuccessful = await transaction.ExecuteAsync();
			if (isSuccessful)
			{
				await PublishUpdateEventAsync(clusterId);
				return newPortMapping;
			}
		}

		throw new Exception("Unable to find an available port range");
	}
	
	/// <summary>
	/// Remove a port mapping for a lease
	/// </summary>
	/// <param name="clusterId">Cluster ID</param>
	/// <param name="leaseId">Lease ID to remove</param>
	/// <returns>True if successful</returns>
	public async Task<bool> RemovePortMappingAsync(string clusterId, string leaseId)
	{
		PortMapping? portMapping = await GetPortMappingAsync(clusterId, leaseId);
		if (portMapping == null)
		{
			return false;
		}

		RedisValue[] ports = portMapping.Ports.Select(x => new RedisValue(Convert.ToString(x.RelayPort))).ToArray();
		ITransaction transaction = _redis.GetDatabase().CreateTransaction();
		_ = transaction.SetRemoveAsync(KeyUsedPorts(clusterId), ports);
		_ = transaction.HashDecrementAsync(KeyPortMappings(clusterId), leaseId);
		return await transaction.ExecuteAsync();
	}

	/// <summary>
	/// Request a port mapping for a given lease and agent
	/// </summary>
	/// <param name="clusterId"></param>
	/// <param name="leaseId"></param>
	/// <param name="agentIp"></param>
	/// <param name="ports"></param>
	/// <returns>A result describing the port mapping</returns>
	/// <exception cref="AgentRelayException"></exception>
	public async Task<PortMappingResult> RequestPortMappingAsync(string clusterId, string leaseId, string agentIp, IList<Port> ports)
	{
		List<RelayAgentInfo> agents = await GetAvailableRelayAgentsAsync(clusterId);
		if (agents.Count == 0)
		{
			throw new AgentRelayException($"No relay agents available for cluster {clusterId}");
		}

		PortMapping portMapping = await AddPortMappingAsync(clusterId, leaseId, agentIp, ports);

		List<string> ipAddresses = agents.SelectMany(x => x.IpAddresses.ToList()).ToList();
		return new PortMappingResult(ipAddresses, portMapping.Ports);
	}
	
	internal async Task<List<RelayAgentInfo>> GetAvailableRelayAgentsAsync(string clusterId)
	{
		List<RelayAgentInfo> agents = await GetRelayAgentsAsync(clusterId);
		List<RelayAgentInfo> validAgents = new();
		foreach (RelayAgentInfo agent in agents)
		{
			bool hasExpired = agent.LastUpdate.ToDateTime() + _agentExpirationTimeout < _clock.UtcNow;
			if (hasExpired)
			{
				await _redis.GetDatabase().HashDeleteAsync(KeyAgents(clusterId), agent.AgentId);
			}
			else
			{
				validAgents.Add(agent);
			}
		}

		return validAgents;
	}
	
	private async Task<List<RelayAgentInfo>> GetRelayAgentsAsync(string clusterId)
	{
		HashEntry[] entries = await _redis.GetDatabase().HashGetAllAsync(KeyAgents(clusterId));
		return entries.Select(x => RelayAgentInfo.Parser.ParseFrom(x.Value)).ToList();
	}

	internal async Task UpdateAgentHeartbeatAsync(string clusterId, string agentRelayId, IEnumerable<string> ipAddresses)
	{
		RelayAgentInfo info = new() { AgentId = agentRelayId, LastUpdate = Timestamp.FromDateTime(_clock.UtcNow) };
		info.IpAddresses.AddRange(ipAddresses);
		await _redis.GetDatabase().HashSetAsync(KeyAgents(clusterId), agentRelayId, info.ToByteArray());
	}

	private Task PublishUpdateEventAsync(string clusterId)
	{
		return _redis.GetDatabase().PublishAsync(RedisChannel.Literal(RedisChannelUpdate), clusterId);
	}
	
	private async Task<IAsyncDisposable> SubscribeToUpdateEventAsync(Action<string> onUpdate)
	{
		return await _redis.GetDatabase().Multiplexer.SubscribeAsync(RedisChannelUpdate, onUpdate);
	}
	
	private async void OnPortMappingUpdateAsync(string clusterId)
	{
		try
		{
			List<PortMapping> portMappings = await GetPortMappingsAsync(clusterId);
			lock (_lock)
			{
				_onPortMappingUpdated.SetResult(portMappings);
				_onPortMappingUpdated = new TaskCompletionSource<List<PortMapping>>();
			}
		}
		catch (Exception e)
		{
			_logger.LogError(e, "Failed updating port mappings");
		}
	}
	
	internal static IReadOnlySet<int> FindAvailablePortRange(IReadOnlySet<int> usedPorts, int numPorts, int minPort, int maxPort)
	{
		for (int start = minPort; start <= maxPort - numPorts + 1; start++)
		{
			HashSet<int> potentialPorts = Enumerable.Range(start, numPorts).ToHashSet();
			if (!potentialPorts.Overlaps(usedPorts))
			{
				return potentialPorts;
			}
		}

		return ImmutableSortedSet<int>.Empty;
	}

	internal void SetMinMaxPorts(int minPort, int maxPort)
	{
		_minPort = minPort;
		_maxPort = maxPort;
	}
	
	internal void SetTimeouts(int longPollMs, int expirationMs)
	{
		_longPollTimeout = TimeSpan.FromMilliseconds(longPollMs);
		_agentExpirationTimeout = TimeSpan.FromMilliseconds(expirationMs);
	}
	
	/// <inheritdoc/>
	public override async Task GetPortMappings(GetPortMappingsRequest request, IServerStreamWriter<GetPortMappingsResponse> responseStream, ServerCallContext context)
	{
		if (String.IsNullOrWhiteSpace(request.ClusterId))
		{
			throw new RpcException(new Status(StatusCode.InvalidArgument, "Invalid cluster ID"));
		}
		
		if (String.IsNullOrWhiteSpace(request.AgentId))
		{
			throw new RpcException(new Status(StatusCode.InvalidArgument, "Invalid agent ID"));
		}
		
		if (request.IpAddresses.Count == 0)
		{
			throw new RpcException(new Status(StatusCode.InvalidArgument, "No IP addresses specified"));
		}
		
		try
		{
			await UpdateAgentHeartbeatAsync(request.ClusterId, request.AgentId, request.IpAddresses);
			
			// Send current state then long-poll for updates
			List<PortMapping> portMappings = await GetPortMappingsAsync(request.ClusterId);
			GetPortMappingsResponse response = new();
			response.PortMappings.AddRange(portMappings);
			await responseStream.WriteAsync(response);
			
			Task<List<PortMapping>> mappingUpdatedTask = _onPortMappingUpdated.Task;
			Task result = await Task.WhenAny(mappingUpdatedTask, Task.Delay(_longPollTimeout, context.CancellationToken));
			if (result == mappingUpdatedTask)
			{
				GetPortMappingsResponse additionalResponse = new();
				additionalResponse.PortMappings.AddRange(await mappingUpdatedTask);
				await responseStream.WriteAsync(additionalResponse);
			}
		}
		catch (OperationCanceledException) when (context.CancellationToken.IsCancellationRequested)
		{
			// Ignore cancellations
		}
		catch (InvalidOperationException ioe) when (ioe.Message.Contains("request is complete", StringComparison.Ordinal))
		{
			// Ignore write error due to request already being completed
		}
	}
}

