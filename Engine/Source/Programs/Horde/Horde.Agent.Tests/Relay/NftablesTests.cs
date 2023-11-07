// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using Horde.Agent.Relay;
using Horde.Common.Rpc;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Agent.Tests.Relay;

[TestClass]
public class NftablesTests
{
	public static readonly PortMapping LeaseMap1 = new()
	{
		LeaseId = "lease1",
		AgentIp = "192.168.1.1",
		Ports =
		{
			new Port { ListenPort = 1000, AgentPort = 2000, Protocol = PortProtocol.Tcp },
		}
	};
	
	public static readonly PortMapping LeaseMap2 = new()
	{
		LeaseId = "lease2",
		AgentIp = "192.168.1.2",
		Ports =
		{
			new Port { ListenPort = 1002, AgentPort = 2002, Protocol = PortProtocol.Tcp },
			new Port { ListenPort = 2222, AgentPort = 5555, Protocol = PortProtocol.Udp }
		}
	};
	
	[TestMethod]
	public void GenerateNftRules_NoPorts()
	{
		Assert.AreEqual(0, Nftables.GenerateNftRules(new List<PortMapping>()).Count);
	}
	
	[TestMethod]
	public void GenerateNftRules_MultiplePorts()
	{
		List<PortMapping> ports = new () { LeaseMap1, LeaseMap2 };
		List<string> rules = Nftables.GenerateNftRules(ports);
		Assert.AreEqual(3, rules.Count);
		Assert.AreEqual("tcp dport 1000 dnat to 192.168.1.1:2000 comment \"leaseId=lease1\"", rules[0]);
		Assert.AreEqual("tcp dport 1002 dnat to 192.168.1.2:2002 comment \"leaseId=lease2\"", rules[1]);
		Assert.AreEqual("udp dport 2222 dnat to 192.168.1.2:5555 comment \"leaseId=lease2\"", rules[2]);
	}

	[TestMethod]
	public void GenerateNftFile()
	{
		List<PortMapping> ports = new () { LeaseMap1, LeaseMap2 };
		string expected = @"flush ruleset
table ip horde {
  chain prerouting {
    type nat hook prerouting priority -100; policy accept;
    tcp dport 1000 dnat to 192.168.1.1:2000 comment ""leaseId=lease1""
    tcp dport 1002 dnat to 192.168.1.2:2002 comment ""leaseId=lease2""
    udp dport 2222 dnat to 192.168.1.2:5555 comment ""leaseId=lease2""
  }
  chain postrouting {
    type nat hook postrouting priority 100; policy accept;
    masquerade
  }
}
".ReplaceLineEndings("\n");

		Assert.AreEqual(expected, Nftables.GenerateNftFile(ports));
	}
}
