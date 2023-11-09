// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Horde.Server.Agents;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Horde.Common.Rpc;
using Horde.Server.Utilities;
using Microsoft.Extensions.Logging.Abstractions;

namespace Horde.Server.Tests.Agents;

[TestClass]
public class AgentRelayTests : TestSetup
{
	private readonly ServerCallContextStub _adminContext = new (HordeClaims.AdminClaim.ToClaim());
	private readonly AgentRelayService _service;
	private readonly PortMapping _pm1 = new()
	{
		LeaseId = "lease1",
		AgentIp = "192.168.1.1",
		Ports = { new Port { ListenPort = -1, AgentPort = 1111, Protocol = PortProtocol.Tcp } }
	};
	
	private readonly PortMapping _pm2 = new()
	{
		LeaseId = "lease2",
		AgentIp = "192.168.1.2",
		Ports =
		{
			new Port { ListenPort = -1, AgentPort = 2222, Protocol = PortProtocol.Tcp },
			new Port { ListenPort = -1, AgentPort = 3333, Protocol = PortProtocol.Udp },
		}
	};
	
	private readonly PortMapping _pm3 = new()
	{
		LeaseId = "lease3",
		AgentIp = "192.168.1.3",
		Ports =
		{
			new Port { ListenPort = -1, AgentPort = 4444, Protocol = PortProtocol.Tcp },
			new Port { ListenPort = -1, AgentPort = 5555, Protocol = PortProtocol.Udp },
			new Port { ListenPort = -1, AgentPort = 6666, Protocol = PortProtocol.Udp },
		}
	};
	
	public AgentRelayTests()
	{
		_service = new AgentRelayService(GetRedisServiceSingleton(), NullLogger<AgentRelayService>.Instance);
	}

	[TestMethod]
	public async Task LongPoll_Simple_Async()
	{
		await _service.StartAsync(CancellationToken.None);
		
		using CancellationTokenSource cts = new (3000);
		_adminContext.SetCancellationToken(cts.Token);
		TestServerStreamWriter<GetPortMappingsResponse> responseStream = new (_adminContext);
		
		GetPortMappingsRequest request = new () { IpAddresses = { "192.168.1.1" }};
		Task task = _service.GetPortMappings(request, responseStream, _adminContext);
		await _service.AddPortMappingAsync("lease1", "192.168.1.1", _pm1.Ports);
		await task;
		
		responseStream.Complete();
		List<GetPortMappingsResponse> responses = await responseStream.ReadAllAsync().ToListAsync(cts.Token);
		
		Assert.AreEqual(1, responses.Count);
		Assert.AreEqual(1, responses[0].PortMappings.Count);
	}
	
	[TestMethod]
	public async Task LongPoll_TwoClients_Async()
	{
		await _service.StartAsync(CancellationToken.None);
		using CancellationTokenSource cts = new (3000);
		_adminContext.SetCancellationToken(cts.Token);
		
		TestServerStreamWriter<GetPortMappingsResponse> responseStream1 = new (_adminContext);
		TestServerStreamWriter<GetPortMappingsResponse> responseStream2 = new (_adminContext);
		
		GetPortMappingsRequest request = new () { IpAddresses = { "192.168.1.1" }};
		
		Task task1 = _service.GetPortMappings(request, responseStream1, _adminContext);
		Task task2 = _service.GetPortMappings(request, responseStream2, _adminContext);
		await _service.AddPortMappingAsync("lease1", "192.168.1.1", _pm1.Ports);
		await task1;
		await task2;
		
		responseStream1.Complete();
		responseStream2.Complete();
		List<GetPortMappingsResponse> responses1 = await responseStream1.ReadAllAsync().ToListAsync(cts.Token);
		List<GetPortMappingsResponse> responses2 = await responseStream2.ReadAllAsync().ToListAsync(cts.Token);
		
		Assert.AreEqual(1, responses1.Count);
		Assert.AreEqual(1, responses1[0].PortMappings.Count);
		Assert.AreEqual(1, responses2.Count);
		Assert.AreEqual(1, responses2[0].PortMappings.Count);
	}
	
	[TestMethod]
	public async Task GetAndSetPortMappingsAsync()
	{
		PortMapping newPm1 = await _service.AddPortMappingAsync(_pm1.LeaseId, _pm1.AgentIp, _pm1.Ports);
		List<PortMapping> portMappings = await _service.GetPortMappingsAsync();
		Assert.AreEqual(1, portMappings.Count);
		Assert.AreEqual(newPm1.LeaseId, portMappings[0].LeaseId);
		Assert.AreEqual(newPm1.AgentIp, portMappings[0].AgentIp);
		Assert.AreEqual("lease1", portMappings[0].LeaseId);
		
		await _service.AddPortMappingAsync(_pm2.LeaseId, _pm2.AgentIp, _pm2.Ports);
		portMappings = await _service.GetPortMappingsAsync();
		Assert.AreEqual(2, portMappings.Count);
		portMappings.Sort((a, b) => String.CompareOrdinal(a.LeaseId, b.LeaseId));
		Assert.AreEqual("lease1", portMappings[0].LeaseId);
		Assert.AreEqual("lease2", portMappings[1].LeaseId);
	}
	
	[TestMethod]
	public async Task PortAssignment_Simple_Async()
	{
		PortMapping newPm = await _service.AddPortMappingAsync(_pm2.LeaseId, _pm2.AgentIp, _pm2.Ports);
		Assert.AreEqual(2, newPm.Ports.Count);
		Assert.AreEqual(2000, newPm.Ports[0].ListenPort);
		Assert.AreEqual(2001, newPm.Ports[1].ListenPort);
	}
	
	[TestMethod]
	public async Task PortAssignment_TwoMappings_Async()
	{
		_service.SetMinMaxPorts(1000, 1003);
		PortMapping newPm1 = await _service.AddPortMappingAsync(_pm1.LeaseId, _pm1.AgentIp, _pm1.Ports);
		PortMapping newPm2 = await _service.AddPortMappingAsync(_pm2.LeaseId, _pm2.AgentIp, _pm2.Ports);
		Assert.AreEqual(1000, newPm1.Ports[0].ListenPort);
		Assert.AreEqual(1001, newPm2.Ports[0].ListenPort);
		Assert.AreEqual(1002, newPm2.Ports[1].ListenPort);
	}
	
	[TestMethod]
	public async Task PortAssignment_OutOfPorts_Async()
	{
		_service.SetMinMaxPorts(1000, 1003);
		await _service.AddPortMappingAsync(_pm1.LeaseId, _pm1.AgentIp, _pm1.Ports);
		await _service.AddPortMappingAsync(_pm3.LeaseId, _pm3.AgentIp, _pm3.Ports);
		await Assert.ThrowsExceptionAsync<Exception>(() => _service.AddPortMappingAsync(_pm2.LeaseId, _pm2.AgentIp, _pm2.Ports));
	}
	
	[TestMethod]
	public async Task PortAssignment_AreReleased_Async()
	{
		_service.SetMinMaxPorts(1000, 1003);
		await _service.AddPortMappingAsync(_pm1.LeaseId, _pm1.AgentIp, _pm1.Ports);
		await _service.AddPortMappingAsync(_pm3.LeaseId, _pm3.AgentIp, _pm3.Ports);
		await Assert.ThrowsExceptionAsync<Exception>(() => _service.AddPortMappingAsync(_pm2.LeaseId, _pm2.AgentIp, _pm2.Ports));
		Assert.IsTrue(await _service.RemovePortMappingAsync(_pm3.LeaseId));
		await _service.AddPortMappingAsync(_pm2.LeaseId, _pm2.AgentIp, _pm2.Ports);
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