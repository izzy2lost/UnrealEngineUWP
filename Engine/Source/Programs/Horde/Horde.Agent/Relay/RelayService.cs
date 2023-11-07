// Copyright Epic Games, Inc. All Rights Reserved.

using Grpc.Core;
using Horde.Common.Rpc;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Relay;

/// <summary>
/// Relay service mirroring port mappings sent from server
/// These mappings will port forward specific ports to agents sitting behind a firewall.
/// An agent using this relay mode usually has multiple IPs assigned to allow bridging.
/// </summary>
public class RelayService
{
	/// <summary>
	/// Cooldown after an exception occurs. Primarily set to speed up tests.
	/// </summary>
	public TimeSpan CooldownOnException { get; set; } = TimeSpan.FromSeconds(5);

	private readonly List<string> _ipAddresses;
	private readonly Nftables _nftables;
	private readonly RelayRpc.RelayRpcClient _relayRpcClient;
	private readonly ILogger<RelayService> _logger;

	/// <summary>
	/// Constructor
	/// </summary>
	/// <param name="ipAddresses">IP addresses this relay agent is listening on</param>
	/// <param name="nftables"></param>
	/// <param name="relayRpcClient"></param>
	/// <param name="logger"></param>
	public RelayService(List<string> ipAddresses, Nftables nftables, RelayRpc.RelayRpcClient relayRpcClient, ILogger<RelayService> logger)
	{
		_ipAddresses = ipAddresses;
		_nftables = nftables;
		_relayRpcClient = relayRpcClient;
		_logger = logger;
	}

	/// <summary>
	/// Get a list of port mappings from the server using a streaming response.
	/// Using streaming, the relay service can act immediately once new mappings are sent.
	/// This avoids excessive and repetitive polling, and in turn load on the server.
	/// </summary>
	/// <param name="timeout">Max timeout before aborting the long poll</param>
	/// <param name="cancellationToken"></param>
	/// <returns>List of new port mappings</returns>
	public async Task<List<PortMapping>> GetPortMappingsLongPollAsync(TimeSpan timeout, CancellationToken cancellationToken)
	{
		_logger.LogDebug("Long polling for port mappings...");
		using CancellationTokenSource cts = new (timeout);
		using CancellationTokenSource linkedCt = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken, cts.Token);

		GetPortMappingsRequest request = new();
		request.IpAddresses.AddRange(_ipAddresses);
		using AsyncServerStreamingCall<GetPortMappingsResponse> cursor = _relayRpcClient.GetPortMappings(request, null, null, linkedCt.Token);
		await foreach (GetPortMappingsResponse response in cursor.ResponseStream.ReadAllAsync(linkedCt.Token))
		{
			return response.PortMappings.ToList();
		}

		// Should never make it here. It will be either get a return value back or get cancelled
		return new List<PortMapping>();
	}
	
	/// <summary>
	/// Continuously listen for port mappings from server and apply them
	/// </summary>
	/// <param name="cancellationToken"></param>
	public async Task ListenForPortMappingsAsync(CancellationToken cancellationToken)
	{
		// Avoid setting a too large value as load balancers can interfere.
		// 55 sec is below a typical 60 second timeout. 
		TimeSpan timeout = TimeSpan.FromSeconds(55);
		
		while (!cancellationToken.IsCancellationRequested)
		{
			try
			{
				List<PortMapping> portMappings = await GetPortMappingsLongPollAsync(timeout, cancellationToken);
				_logger.LogDebug("Received {NumMappings} port mappings...", portMappings.Count);
				await _nftables.ApplyPortForwardingAsync(portMappings);
			}
			catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
			{
				break;
			}
			catch (Exception e)
			{
				_logger.LogError(e, "Failed getting and applying port mappings");
				await Task.Delay(CooldownOnException, CancellationToken.None);
			}
		}
	}
}