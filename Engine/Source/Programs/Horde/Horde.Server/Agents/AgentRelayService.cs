// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Collections.Immutable;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Redis;
using Google.Protobuf;
using Grpc.Core;
using Horde.Common.Rpc;
using Horde.Server.Server;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using StackExchange.Redis;

namespace Horde.Server.Agents;

/// <summary>
/// Service handling relaying of agent traffic
/// The Horde agent can be started in a special mode where it will relay traffic to and from other agents.
/// It connects back to this service over gRPC and mirror the current port mappings set by the server.
/// </summary>
public sealed class AgentRelayService : RelayRpc.RelayRpcBase, IHostedService //: IHostedService, IAsyncDisposable
{
	private const string RedisKeyPortMappings = "relay/port-mappings";
	private const string RedisKeyUsedPorts = "relay/used-ports";
	private const string RedisChannelUpdate = "relay/update";

	private readonly RedisService _redis;
	private readonly ILogger<AgentRelayService> _logger;
	private readonly object _lock = new();
	private TaskCompletionSource<List<PortMapping>> _onPortMappingUpdated = new();

	private IAsyncDisposable? _redisSubscription;
	private int _minPort = 2000;
	private int _maxPort = 65000;
	
	/// <summary>
	/// Constructor
	/// </summary>
	/// <param name="redis"></param>
	/// <param name="logger"></param>
	public AgentRelayService(RedisService redis, ILogger<AgentRelayService> logger)
	{
		_redis = redis;
		_logger = logger;
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
	/// <param name="leaseId">Lease ID</param>
	/// <returns>The port mapping, or null if not found</returns>
	public async Task<PortMapping?> GetPortMappingAsync(string leaseId)
	{
		IDatabase redis = _redis.GetDatabase();
		RedisValue value = await redis.HashGetAsync(RedisKeyPortMappings, leaseId);
		return value.IsNullOrEmpty ? null : PortMapping.Parser.ParseFrom(value);
	}
	
	/// <summary>
	/// Retrieve all current port mappings
	/// </summary>
	/// <returns>List of all port mappings</returns>
	public async Task<List<PortMapping>> GetPortMappingsAsync()
	{
		IDatabase redis = _redis.GetDatabase();
		HashEntry[] entries = await redis.HashGetAllAsync(RedisKeyPortMappings);
		return entries.Select(x => PortMapping.Parser.ParseFrom(x.Value)).ToList();
	}
	
	/// <summary>
	/// Add a new port mapping
	/// </summary>
	/// <param name="leaseId">Lease ID this mapping is for</param>
	/// <param name="agentIp">What agent IP to forward all traffic to</param>
	/// <param name="ports">Ports agent is listening</param>
	/// <param name="numRetries">Number of retries before giving up</param>
	/// <returns>A port mapping with listen ports assigned</returns>
	/// <exception cref="Exception"></exception>
	public async Task<PortMapping> AddPortMappingAsync(string leaseId, string agentIp, IList<Port> ports, int numRetries = 10)
	{
		IDatabase redis = _redis.GetDatabase();

		// TODO: Randomize start position to spread out port use
		// TODO: Add pinging to and do not consider stale agent relays
		// TODO: Check failed deserialization from Redis
		
		for (int retryAttempt = 0; retryAttempt < numRetries; retryAttempt++)
		{
			HashSet<int> usedPorts = (await redis.SetMembersAsync(RedisKeyUsedPorts)).Select(x => (int)x).ToHashSet();
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
				.Zip(ports, (newListenPort, port) => new Port { ListenPort = newListenPort, AgentPort = port.AgentPort, Protocol = port.Protocol });
			
			PortMapping newPortMapping = new () { LeaseId = leaseId, AgentIp = agentIp };
			newPortMapping.Ports.AddRange(newPorts);

			ITransaction transaction = redis.CreateTransaction();
			RedisValue[] portRangeRedis = portRange.Select(x => Convert.ToString(x)).Select(x => new RedisValue(x)).ToArray();
			foreach (int port in portRange)
			{
				transaction.AddCondition(Condition.SetNotContains(RedisKeyUsedPorts, port));
			}
			
			_ = transaction.SetAddAsync(RedisKeyUsedPorts, portRangeRedis);
			_ = transaction.HashSetAsync(RedisKeyPortMappings, leaseId, newPortMapping.ToByteArray());
			bool isSuccessful = await transaction.ExecuteAsync();
			if (isSuccessful)
			{
				await PublishUpdateEventAsync();
				return newPortMapping;
			}
		}

		throw new Exception("Unable to find an available port range");
	}
	
	/// <summary>
	/// Remove a port mapping for a lease
	/// </summary>
	/// <param name="leaseId">Lease ID to remove</param>
	/// <returns>True if successful</returns>
	public async Task<bool> RemovePortMappingAsync(string leaseId)
	{
		PortMapping? portMapping = await GetPortMappingAsync(leaseId);
		if (portMapping == null)
		{
			return false;
		}

		RedisValue[] ports = portMapping.Ports.Select(x => new RedisValue(Convert.ToString(x.ListenPort))).ToArray();
		ITransaction transaction = _redis.GetDatabase().CreateTransaction();
		_ = transaction.SetRemoveAsync(RedisKeyUsedPorts, ports);
		_ = transaction.HashDecrementAsync(RedisKeyPortMappings, leaseId);
		return await transaction.ExecuteAsync();
	}

	private Task PublishUpdateEventAsync()
	{
		return _redis.GetDatabase().PublishAsync(RedisChannelUpdate, "updated");
	}
	
	private async Task<IAsyncDisposable> SubscribeToUpdateEventAsync(Action onUpdate)
	{
		return await _redis.GetDatabase().Multiplexer.SubscribeAsync(RedisChannelUpdate, (_) => onUpdate());
	}
	
	private async void OnPortMappingUpdateAsync()
	{
		try
		{
			List<PortMapping> portMappings = await GetPortMappingsAsync();
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
	
	/// <inheritdoc/>
	public override async Task GetPortMappings(GetPortMappingsRequest request, IServerStreamWriter<GetPortMappingsResponse> responseStream, ServerCallContext context)
	{
		try
		{
			List<PortMapping> portMappings = await _onPortMappingUpdated.Task;
			GetPortMappingsResponse response = new ();
			response.PortMappings.AddRange(portMappings);
			await responseStream.WriteAsync(response);
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

