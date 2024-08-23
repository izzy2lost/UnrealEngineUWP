// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde.Agents;
using HordeCommon;
using HordeCommon.Rpc;
using HordeCommon.Rpc.Messages;
using HordeServer.Agents.Sessions;
using Microsoft.Extensions.DependencyInjection;

namespace HordeServer.Tests.Agents
{
	[TestClass]
	public class AgentSchedulerTests : ComputeTestSetup
	{
		[TestMethod]
		public async Task CreateSessionAsync()
		{
			IClock clock = ServiceProvider.GetRequiredService<IClock>();

			RpcAgentCapabilities caps = new RpcAgentCapabilities();
			caps.Properties.Add("foo=bar");

			RpcSession? session = await AgentScheduler.TryCreateSessionAsync(new AgentId("foo"), SessionIdUtils.GenerateNewId(), caps);
			Assert.IsNotNull(session);

			Assert.AreEqual(session.AgentId, new AgentId("foo"));
			Assert.AreEqual(0, session.Leases.Count);
			Assert.IsTrue(session.ExpiryTime > clock.UtcNow);

			RpcSession? session2 = await AgentScheduler.TryGetSessionAsync(session.SessionId);
			Assert.AreEqual(session, session2);

			RpcAgentCapabilities? caps2 = await AgentScheduler.TryGetSessionCapabilitiesAsync(session);
			Assert.AreEqual(caps2, caps);
		}

		[TestMethod]
		public async Task UpdateCapabilitesAsync()
		{
			// Set and get the initial capabilities
			RpcAgentCapabilities caps = new RpcAgentCapabilities();
			caps.Properties.Add("foo=bar");

			RpcSession? session = await AgentScheduler.TryCreateSessionAsync(new AgentId("foo"), SessionIdUtils.GenerateNewId(), caps);
			Assert.IsNotNull(session);

			RpcAgentCapabilities? caps2 = await AgentScheduler.TryGetSessionCapabilitiesAsync(session);
			Assert.AreEqual(caps, caps2);

			// Update them
			RpcAgentCapabilities caps3 = new RpcAgentCapabilities(caps);
			caps3.Properties.Add("bar=baz");

			RpcSession? session2 = await AgentScheduler.TryUpdateSessionAsync(session, newCapabilities: caps3);
			Assert.IsNotNull(session2);

			RpcAgentCapabilities? caps4 = await AgentScheduler.TryGetSessionCapabilitiesAsync(session);
			Assert.IsNull(caps4);

			RpcAgentCapabilities? caps5 = await AgentScheduler.TryGetSessionCapabilitiesAsync(session2);
			Assert.AreEqual(caps3, caps5);
		}

		[TestMethod]
		public async Task TerminateSessionAsync()
		{
			FakeClock clock = ServiceProvider.GetRequiredService<FakeClock>();

			RpcSession? session = await AgentScheduler.TryCreateSessionAsync(new AgentId("foo"), SessionIdUtils.GenerateNewId(), new RpcAgentCapabilities());
			Assert.IsNotNull(session);

			await clock.AdvanceAsync(TimeSpan.FromSeconds(30.0));

			RpcSession? session2 = await AgentScheduler.TryUpdateSessionAsync(session, new RpcSession(session) { Status = RpcAgentStatus.Stopped });
			Assert.IsNotNull(session2);
			Assert.AreEqual(clock.UtcNow, session2.ExpiryTime);

			RpcSession? session3 = await AgentScheduler.TryGetSessionAsync(session.SessionId);
			Assert.IsNull(session3);
		}
	}
}
