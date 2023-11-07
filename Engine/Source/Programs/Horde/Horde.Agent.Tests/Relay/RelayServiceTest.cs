// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using Grpc.Core;
using Horde.Agent.Relay;
using Horde.Common.Rpc;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Agent.Tests.Relay;

public class TestRelayRpcClient : RelayRpc.RelayRpcClient
{
	public TestAsyncStreamReader<GetPortMappingsResponse> GetPortMappingsResponses { get; } = new ();

	public override AsyncServerStreamingCall<GetPortMappingsResponse> GetPortMappings(
		GetPortMappingsRequest request, Metadata headers = null!,
		DateTime? deadline = null, CancellationToken cancellationToken = default)
	{
		return new AsyncServerStreamingCall<GetPortMappingsResponse>(
			GetPortMappingsResponses, null!, null!, null!, () => { });
	}
}

[TestClass]
public class RelayServiceTest
{
	private readonly TestRelayRpcClient _grpcClient = new ();
	private readonly RelayService _relayService;

	public RelayServiceTest()
	{
		_relayService = new RelayService(new List<string> { "192.168.1.99" }, Nftables.CreateNull(), _grpcClient, NullLogger<RelayService>.Instance);
		_relayService.CooldownOnException = TimeSpan.Zero;
	}
	
	[TestMethod]
	public async Task GetPortMappingsLongPoll_Results_Async()
	{
		using CancellationTokenSource cts = new (3000);
		_grpcClient.GetPortMappingsResponses.AddMessage(new GetPortMappingsResponse { PortMappings = { NftablesTests.LeaseMap1 }});
		_grpcClient.GetPortMappingsResponses.AddMessage(new GetPortMappingsResponse { PortMappings = { NftablesTests.LeaseMap2, NftablesTests.LeaseMap1 }});

		List<PortMapping> pm1 = await _relayService.GetPortMappingsLongPollAsync(TimeSpan.FromSeconds(10), cts.Token);
		Assert.AreEqual(1, pm1.Count);
		Assert.AreEqual("lease1", pm1[0].LeaseId);
		
		List<PortMapping> pm2 = await _relayService.GetPortMappingsLongPollAsync(TimeSpan.FromSeconds(10), cts.Token);
		Assert.AreEqual(2, pm2.Count);
		Assert.AreEqual("lease2", pm2[0].LeaseId);
		Assert.AreEqual("lease1", pm2[1].LeaseId);
	}
	
	[TestMethod]
	public async Task GetPortMappingsLongPoll_TimesOut_Async()
	{
		using CancellationTokenSource cts = new (3000);
		await Assert.ThrowsExceptionAsync<OperationCanceledException>(() => _relayService.GetPortMappingsLongPollAsync(TimeSpan.FromMilliseconds(500), cts.Token));
	}
	
	[TestMethod]
	public async Task ListenForPortMappings_TimesOut_Async()
	{
		using CancellationTokenSource cts = new (3000);

		Task t = _relayService.ListenForPortMappingsAsync(cts.Token);
		await Task.Delay(100, cts.Token);
		_grpcClient.GetPortMappingsResponses.AddMessage(new GetPortMappingsResponse { PortMappings = { NftablesTests.LeaseMap1 }});
		await Task.Delay(100, cts.Token);
		_grpcClient.GetPortMappingsResponses.AddMessage(new GetPortMappingsResponse { PortMappings = { NftablesTests.LeaseMap2 }});

		await t;
	}
}
