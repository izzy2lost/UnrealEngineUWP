// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde.Agents;
using EpicGames.Horde.Agents.Sessions;
using HordeCommon;
using HordeCommon.Rpc;
using HordeCommon.Rpc.Messages;
using HordeServer.Agents;
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

		[TestMethod]
		public async Task AddSessionsToNewQueueTestAsync()
		{
			AgentScheduler scheduler = (AgentScheduler)AgentScheduler;
			await scheduler.StartAsync(default);

			// First session
			for (int idx = 0; idx < 3; idx++)
			{
				RpcAgentCapabilities caps1 = new RpcAgentCapabilities();
				caps1.Properties.Add("Platform=Windows");

				RpcSession? session1 = await AgentScheduler.TryCreateSessionAsync(new AgentId("foo"), SessionIdUtils.GenerateNewId(), caps1);
				Assert.IsNotNull(session1);
			}

			// Second session
			for (int idx = 0; idx < 5; idx++)
			{
				RpcAgentCapabilities caps2 = new RpcAgentCapabilities();
				caps2.Properties.Add("Platform=MacOS");

				RpcSession? session2 = await AgentScheduler.TryCreateSessionAsync(new AgentId("bar"), SessionIdUtils.GenerateNewId(), caps2);
				Assert.IsNotNull(session2);
			}

			// Create the first queue
			RpcAgentRequirements reqs1 = new RpcAgentRequirements();
			reqs1.Properties.Add("Platform=Windows");

			IoHash queueHash1 = await scheduler.CreateFilterAsync(reqs1);

			// Create the second queue
			RpcAgentRequirements reqs2 = new RpcAgentRequirements();
			reqs2.Properties.Add("Platform=MacOS");

			IoHash queueHash2 = await scheduler.CreateFilterAsync(reqs2);

			// Update the cached queues
			await scheduler.ForceUpdateFiltersAsync(CancellationToken.None);

			SessionId[] sessionIds1 = await scheduler.GetAllFilteredSessionsAsync(queueHash1);
			Assert.AreEqual(3, sessionIds1.Length);

			SessionId[] sessionIds2 = await scheduler.GetAllFilteredSessionsAsync(queueHash2);
			Assert.AreEqual(5, sessionIds2.Length);
		}

		[TestMethod]
		public async Task AddSessionsToExistingQueueTestAsync()
		{
			FakeClock clock = ServiceProvider.GetRequiredService<FakeClock>();
			AgentScheduler scheduler = (AgentScheduler)AgentScheduler;
			await scheduler.StartAsync(default);

			// Create the first queue
			RpcAgentRequirements reqs1 = new RpcAgentRequirements();
			reqs1.Properties.Add("Platform=Windows");

			IoHash queueHash1 = await scheduler.CreateFilterAsync(reqs1);

			// Create the second queue
			RpcAgentRequirements reqs2 = new RpcAgentRequirements();
			reqs2.Properties.Add("Platform=MacOS");

			IoHash queueHash2 = await scheduler.CreateFilterAsync(reqs2);

			// Update the cached queues
			await clock.AdvanceAsync(TimeSpan.FromMinutes(1.0));

			// First session
			for (int idx = 0; idx < 3; idx++)
			{
				RpcAgentCapabilities caps1 = new RpcAgentCapabilities();
				caps1.Properties.Add("Platform=Windows");

				RpcSession? session1 = await AgentScheduler.TryCreateSessionAsync(new AgentId("foo"), SessionIdUtils.GenerateNewId(), caps1);
				Assert.IsNotNull(session1);
			}

			// Second session
			for (int idx = 0; idx < 5; idx++)
			{
				RpcAgentCapabilities caps2 = new RpcAgentCapabilities();
				caps2.Properties.Add("Platform=MacOS");

				RpcSession? session2 = await AgentScheduler.TryCreateSessionAsync(new AgentId("bar"), SessionIdUtils.GenerateNewId(), caps2);
				Assert.IsNotNull(session2);
			}

			//
			SessionId[] sessionIds1 = await scheduler.GetAllFilteredSessionsAsync(queueHash1);
			Assert.AreEqual(3, sessionIds1.Length);

			SessionId[] sessionIds2 = await scheduler.GetAllFilteredSessionsAsync(queueHash2);
			Assert.AreEqual(5, sessionIds2.Length);
		}

		[TestMethod]
		public async Task ExpireQueuesTestAsync()
		{
			FakeClock clock = ServiceProvider.GetRequiredService<FakeClock>();
			AgentScheduler scheduler = (AgentScheduler)AgentScheduler;
			await scheduler.StartAsync(default);

			// Create the first queue
			RpcAgentRequirements reqs1 = new RpcAgentRequirements();
			reqs1.Properties.Add("Platform=Windows");

			IoHash reqsHash1 = await scheduler.CreateFilterAsync(reqs1);
			Assert.AreEqual(1, (await scheduler.GetFiltersAsync()).Length);

			// Check that the queue exists
			await clock.AdvanceAsync(HordeServer.Agents.AgentScheduler.ExpireFiltersTime / 2);
			await scheduler.TouchFilterAsync(reqsHash1);
			Assert.AreEqual(1, (await scheduler.GetFiltersAsync()).Length);

			await clock.AdvanceAsync(HordeServer.Agents.AgentScheduler.ExpireFiltersTime / 2);
			await scheduler.TouchFilterAsync(reqsHash1);
			Assert.AreEqual(1, (await scheduler.GetFiltersAsync()).Length);

			await clock.AdvanceAsync(HordeServer.Agents.AgentScheduler.ExpireFiltersTime / 2);
			Assert.AreEqual(1, (await scheduler.GetFiltersAsync()).Length);

			await clock.AdvanceAsync(HordeServer.Agents.AgentScheduler.ExpireFiltersTime);
			Assert.AreEqual(0, (await scheduler.GetFiltersAsync()).Length);
		}
	}
}
