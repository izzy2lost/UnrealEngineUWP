// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Grpc.Core;
using Horde.Server.Agents;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Horde.Common.Rpc;
using Horde.Server.Utilities;
using Microsoft.Extensions.Logging.Abstractions;

namespace Horde.Server.Tests.Agents;

[TestClass]
public class AgentRelayTests : TestSetup
{
	private readonly AgentRelayService _service;
	private readonly GetPortMappingsRequest _request = new () { ClusterId = "cluster1", AgentId = "agent1", IpAddresses = { "192.168.1.1" }};
	
	private readonly PortMapping _pm1 = new()
	{
		LeaseId = "lease1",
		AgentIp = "192.168.1.1",
		Ports = { new Port { RelayPort = -1, AgentPort = 1111, Protocol = PortProtocol.Tcp } }
	};
	
	private readonly PortMapping _pm2 = new()
	{
		LeaseId = "lease2",
		AgentIp = "192.168.1.2",
		Ports =
		{
			new Port { RelayPort = -1, AgentPort = 2222, Protocol = PortProtocol.Tcp },
			new Port { RelayPort = -1, AgentPort = 3333, Protocol = PortProtocol.Udp },
		}
	};
	
	private readonly PortMapping _pm3 = new()
	{
		LeaseId = "lease3",
		AgentIp = "192.168.1.3",
		Ports =
		{
			new Port { RelayPort = -1, AgentPort = 4444, Protocol = PortProtocol.Tcp },
			new Port { RelayPort = -1, AgentPort = 5555, Protocol = PortProtocol.Udp },
			new Port { RelayPort = -1, AgentPort = 6666, Protocol = PortProtocol.Udp },
		}
	};
	
	public AgentRelayTests()
	{
		_service = new AgentRelayService(GetRedisServiceSingleton(), Clock, NullLogger<AgentRelayService>.Instance);
	}

	[TestInitialize]
	public async Task SetupAsync()
	{
		await _service.StartAsync(CancellationToken.None);
	}

	/// <summary>
	/// Helper for testing gRPC calls with streaming responses
	/// </summary>
	/// <param name="call"></param>
	/// <param name="timeout">Max time to wait for test operation to complete</param>
	/// <typeparam name="T">Type streamed back</typeparam>
	/// <returns></returns>
	private static async Task<List<T>> TestStreamingGrpcAsync<T>(Func<TestServerStreamWriter<T>, ServerCallContext, Task> call, TimeSpan timeout) where T : class
	{
		using CancellationTokenSource cts = new (timeout);
		ServerCallContextStub context = new (HordeClaims.AdminClaim.ToClaim());
		context.SetCancellationToken(cts.Token);
		TestServerStreamWriter<T> responseStream = new (context);
		await call(responseStream, context);
		responseStream.Complete();
		return await responseStream.ReadAllAsync().ToListAsync(cts.Token);
	}

	private static GetPortMappingsRequest Request(string cluster = "cluster1", string agent = "agent1", int revision = -1, string ipAddress = "192.168.1.1")
	{
		return new GetPortMappingsRequest { ClusterId = cluster, AgentId = agent, RevisionCount = revision, IpAddresses = { ipAddress }};
	}

	private async Task<GetPortMappingsResponse> GetPortMappingsAsync(GetPortMappingsRequest request, int timeoutMs = 5000)
	{
		List<GetPortMappingsResponse> responses = await TestStreamingGrpcAsync<GetPortMappingsResponse>(async (sw, ctx) =>
		{
			await _service.GetPortMappings(request, sw, ctx);
		}, TimeSpan.FromMilliseconds(timeoutMs));
		
		Assert.AreEqual(1, responses.Count);
		return responses[0];
	}
	
	[TestMethod]
	public async Task LongPoll_Simple_Async()
	{
		Task<GetPortMappingsResponse> task = GetPortMappingsAsync(_request);
		await _service.AddPortMappingAsync("cluster1", "lease1", "192.168.100.10", _pm1.Ports);
		GetPortMappingsResponse res = await task;
		Assert.AreEqual(1, res.RevisionCount);
		Assert.AreEqual(1, res.PortMappings.Count);
		Assert.AreEqual("lease1", res.PortMappings[0].LeaseId);
	}
	
	[TestMethod]
	public async Task LongPoll_TooOldRevision_Async()
	{
		await _service.AddPortMappingAsync("cluster1", _pm1.LeaseId, _pm1.AgentIp, _pm1.Ports);
		await _service.AddPortMappingAsync("cluster1", _pm2.LeaseId, _pm2.AgentIp, _pm2.Ports);
		await _service.AddPortMappingAsync("cluster1", _pm3.LeaseId, _pm3.AgentIp, _pm3.Ports);
		GetPortMappingsResponse res = await GetPortMappingsAsync(Request(revision: 1));
		Assert.AreEqual(3, res.RevisionCount);
		Assert.AreEqual(3, res.PortMappings.Count);
	}
	
	[TestMethod]
	public async Task LongPoll_TooNewRevision_Async()
	{
		await _service.AddPortMappingAsync("cluster1", _pm1.LeaseId, _pm1.AgentIp, _pm1.Ports);
		await _service.AddPortMappingAsync("cluster1", _pm2.LeaseId, _pm2.AgentIp, _pm2.Ports);
		await _service.AddPortMappingAsync("cluster1", _pm3.LeaseId, _pm3.AgentIp, _pm3.Ports);
		GetPortMappingsResponse res = await GetPortMappingsAsync(Request(revision: 999999));
		Assert.AreEqual(3, res.RevisionCount);
		Assert.AreEqual(3, res.PortMappings.Count);
	}
	
	[TestMethod]
	public async Task LongPoll_TwoClients_Async()
	{
		Task<GetPortMappingsResponse> task1 = GetPortMappingsAsync(Request());
		Task<GetPortMappingsResponse> task2 = GetPortMappingsAsync(Request());
		await _service.AddPortMappingAsync("cluster1", "lease1", "192.168.100.10", _pm1.Ports);

		GetPortMappingsResponse response1 = await task1;
		GetPortMappingsResponse response2 = await task2;
		Assert.AreEqual(1, response1.PortMappings.Count);
		Assert.AreEqual(1, response2.PortMappings.Count);
		Assert.AreEqual(1, response1.RevisionCount);
		Assert.AreEqual(1, response2.RevisionCount);
	}
	
	[TestMethod]
	public async Task AgentHeartbeat_IsAvailable_Async()
	{
		_service.SetTimeouts(100, 5000);
		await GetPortMappingsAsync(Request(revision: -2));

		List<RelayAgentInfo> agents = await _service.GetAvailableRelayAgentsAsync("cluster1");
		Assert.AreEqual(1, agents.Count);
		Assert.AreEqual("192.168.1.1" , agents[0].IpAddresses[0]);
		
		Assert.AreEqual(0, (await _service.GetAvailableRelayAgentsAsync("otherCluster")).Count);
	}
	
	[TestMethod]
	public async Task AgentHeartbeat_StaleAgentsAreNotReturned_Async()
	{
		_service.SetTimeouts(100, 5000);
		await GetPortMappingsAsync(Request(revision: -2));
		Assert.AreEqual(1, (await _service.GetAvailableRelayAgentsAsync(_request.ClusterId)).Count);
		await Clock.AdvanceAsync(TimeSpan.FromMinutes(5));
		Assert.AreEqual(0, (await _service.GetAvailableRelayAgentsAsync(_request.ClusterId)).Count);
	}
	
	[TestMethod]
	public async Task GetAndSetPortMappingsAsync()
	{
		(int changeId, List<PortMapping> portMappings) =  await _service.GetPortMappingsAsync("cluster1");
		Assert.AreEqual(0, changeId);
		Assert.AreEqual(0, portMappings.Count);
		
		PortMapping newPm1 = await _service.AddPortMappingAsync("cluster1", _pm1.LeaseId, _pm1.AgentIp, _pm1.Ports);
		(changeId, portMappings) = await _service.GetPortMappingsAsync("cluster1");
		Assert.AreEqual(1, changeId);
		Assert.AreEqual(1, portMappings.Count);
		Assert.AreEqual(newPm1.LeaseId, portMappings[0].LeaseId);
		Assert.AreEqual(newPm1.AgentIp, portMappings[0].AgentIp);
		Assert.AreEqual("lease1", portMappings[0].LeaseId);
		
		await _service.AddPortMappingAsync("cluster1", _pm2.LeaseId, _pm2.AgentIp, _pm2.Ports);
		(changeId, portMappings) = await _service.GetPortMappingsAsync("cluster1");
		Assert.AreEqual(2, changeId);
		Assert.AreEqual(2, portMappings.Count);
		portMappings.Sort((a, b) => String.CompareOrdinal(a.LeaseId, b.LeaseId));
		Assert.AreEqual("lease1", portMappings[0].LeaseId);
		Assert.AreEqual("lease2", portMappings[1].LeaseId);
	}
	
	[TestMethod]
	public async Task PortMapping_Remove_Async()
	{
		await _service.AddPortMappingAsync("cluster1", _pm1.LeaseId, _pm1.AgentIp, _pm1.Ports);
		await _service.AddPortMappingAsync("cluster1", _pm3.LeaseId, _pm3.AgentIp, _pm3.Ports);
		(int changeId, List<PortMapping> portMappings) = await _service.GetPortMappingsAsync("cluster1");
		Assert.AreEqual(2, changeId);
		Assert.AreEqual(2, portMappings.Count);
		Assert.IsTrue(await _service.RemovePortMappingAsync("cluster1", _pm3.LeaseId));
		(changeId, portMappings) = await _service.GetPortMappingsAsync("cluster1");
		Assert.AreEqual(3, changeId);
		Assert.AreEqual(1, portMappings.Count);
	}
	
	[TestMethod]
	public async Task PortAssignment_Simple_Async()
	{
		PortMapping newPm = await _service.AddPortMappingAsync("cluster1", _pm2.LeaseId, _pm2.AgentIp, _pm2.Ports);
		Assert.AreEqual(2, newPm.Ports.Count);
		Assert.AreEqual(10000, newPm.Ports[0].RelayPort);
		Assert.AreEqual(10001, newPm.Ports[1].RelayPort);
	}
	
	[TestMethod]
	public async Task PortAssignment_TwoMappings_Async()
	{
		_service.SetMinMaxPorts(1000, 1003);
		PortMapping newPm1 = await _service.AddPortMappingAsync("cluster1", _pm1.LeaseId, _pm1.AgentIp, _pm1.Ports);
		PortMapping newPm2 = await _service.AddPortMappingAsync("cluster1", _pm2.LeaseId, _pm2.AgentIp, _pm2.Ports);
		Assert.AreEqual(1000, newPm1.Ports[0].RelayPort);
		Assert.AreEqual(1001, newPm2.Ports[0].RelayPort);
		Assert.AreEqual(1002, newPm2.Ports[1].RelayPort);
	}
	
	[TestMethod]
	public async Task PortAssignment_OutOfPorts_Async()
	{
		_service.SetMinMaxPorts(1000, 1003);
		await _service.AddPortMappingAsync("cluster1", _pm1.LeaseId, _pm1.AgentIp, _pm1.Ports);
		await _service.AddPortMappingAsync("cluster1", _pm3.LeaseId, _pm3.AgentIp, _pm3.Ports);
		await Assert.ThrowsExceptionAsync<Exception>(() => _service.AddPortMappingAsync("cluster1", _pm2.LeaseId, _pm2.AgentIp, _pm2.Ports));
	}
	
	[TestMethod]
	public async Task PortAssignment_AreReleased_Async()
	{
		_service.SetMinMaxPorts(1000, 1003);
		await _service.AddPortMappingAsync("cluster1", _pm1.LeaseId, _pm1.AgentIp, _pm1.Ports);
		await _service.AddPortMappingAsync("cluster1", _pm3.LeaseId, _pm3.AgentIp, _pm3.Ports);
		await Assert.ThrowsExceptionAsync<Exception>(() => _service.AddPortMappingAsync("cluster1", _pm2.LeaseId, _pm2.AgentIp, _pm2.Ports));
		Assert.IsTrue(await _service.RemovePortMappingAsync("cluster1", _pm3.LeaseId));
		await _service.AddPortMappingAsync("cluster1", _pm2.LeaseId, _pm2.AgentIp, _pm2.Ports);
	}
	
	[TestMethod]
	public void FindAvailablePortRange()
	{
		List<int> Actual(int numPorts, params int[] usedPorts)
		{
			return AgentRelayService.FindAvailablePortRange(usedPorts.ToHashSet(), numPorts, 0, 5).ToList();
		}
		
		List<int> Expected(params int[] ports)
		{
			return ports.ToHashSet().ToList();
		}
		
		CollectionAssert.AreEquivalent(Expected(), Actual(0));
		CollectionAssert.AreEquivalent(Expected(0, 1, 2), Actual(3));
		CollectionAssert.AreEquivalent(Expected(0, 1, 2, 3, 4), Actual(5));
		CollectionAssert.AreEquivalent(Expected(0, 1, 2, 3, 4, 5), Actual(6));
		CollectionAssert.AreEquivalent(Expected(), Actual(7));

		CollectionAssert.AreEquivalent(Expected(2, 3), Actual(2, 1));
		CollectionAssert.AreEquivalent(Expected(0, 1), Actual(2, 2));
		CollectionAssert.AreEquivalent(Expected(3, 4), Actual(2, 0, 1, 2));
		CollectionAssert.AreEquivalent(Expected(4, 5), Actual(2, 0, 1, 2, 3));
		CollectionAssert.AreEquivalent(Expected(), Actual(2, 0, 1, 2, 3, 4));
		CollectionAssert.AreEquivalent(Expected(), Actual(2, 0, 1, 2, 3, 4, 5));
		CollectionAssert.AreEquivalent(Expected(1, 2), Actual(2, 0, 3, 4, 5));
	}
}