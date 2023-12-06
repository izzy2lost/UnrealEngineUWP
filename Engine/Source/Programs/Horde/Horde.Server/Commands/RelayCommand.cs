// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Grpc.Net.Client;
using Horde.Common.Rpc;
using Horde.Server.Agents.Relay;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Abstractions;

namespace Horde.Server.Commands;

/// <summary>
/// Run agent in relay mode
/// </summary>
[Command("relay", "Run agent in relay mode")]
class RelayCommand : Command
{
	/// <summary>
	/// Log verbosity level (use normal Serilog levels such as debug, warning or info)
	/// </summary>
	[CommandLine("-LogLevel")]
	public string LogLevelStr { get; set; } = "information";
	
	/// <summary>
	/// Cluster ID this relay agent belongs to
	/// </summary>
	[CommandLine("-ClusterId=", Required = true)]
	public string ClusterId { get; set; } = null!;
	
	/// <summary>
	/// Arbitrary but unique agent ID
	/// </summary>
	[CommandLine("-AgentId=", Required = true)]
	public string AgentId { get; set; } = null!;

	/// <summary>
	/// gRPC URL to Horde server
	/// </summary>
	[CommandLine("-ServerUrl=", Required = true)]
	public string ServerUrl { get; set; } = null!;
	
	/// <summary>
	/// IP addresses this relay agent can be addressed. Multiple IPs are separated with comma.
	/// </summary>
	[CommandLine("-ListenIps=", Required = true)]
	public string ListenIpsStr { get; set; } = null!;
	
	/// <summary>
	/// Run 'nft' executable with sudo
	/// </summary>
	[CommandLine("-RunWithSudo=")]
	public bool RunWithSudo { get; set; } = true;

	/// <summary>
	/// Constructor
	/// </summary>
	public RelayCommand()
	{
	}
	
	/// <summary>
	/// Runs the service indefinitely
	/// </summary>
	/// <returns>Exit code</returns>
	public override async Task<int> ExecuteAsync(ILogger logger)
	{
		string[] listenIps = ListenIpsStr.Split(",");
		
		logger.LogInformation("     Agent ID: {AgentId}", AgentId);
		logger.LogInformation("   Cluster ID: {ClusterId}", ClusterId);
		logger.LogInformation("Run with sudo: {RunWithSudo}", RunWithSudo);
		logger.LogInformation("   Listen IPs: {ListenIps}", String.Join(' ', listenIps));
		logger.LogInformation("   Server URL: {ServerUrl}", ServerUrl);
		
		Nftables nftables = new (NullLogger<Nftables>.Instance) { RunWithSudo = RunWithSudo };
		await nftables.InitializeAsync();
		
		AppContext.SetSwitch("System.Net.Http.SocketsHttpHandler.Http2UnencryptedSupport", true);

		using CancellationTokenSource cts = new();
		Console.CancelKeyPress += new ((sender, args) =>
		{
			logger.LogInformation("Stopping service due to user request...");
			args.Cancel = true;
			cts.Cancel();
		});
		
		using GrpcChannel channel = GrpcChannel.ForAddress(ServerUrl);
		RelayRpc.RelayRpcClient relayGrpcClient = new (channel);
		AgentRelayClient relayClient = new(ClusterId, AgentId, listenIps.ToList(), nftables, relayGrpcClient, logger);

		logger.LogInformation("Listening for port mappings...");
		await relayClient.ListenForPortMappingsAsync(cts.Token);
		return 0;
	}
}
