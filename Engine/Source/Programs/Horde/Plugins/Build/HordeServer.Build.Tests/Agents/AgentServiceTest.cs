// Copyright Epic Games, Inc. All Rights Reserved.

using System.Security.Claims;
using EpicGames.Core;
using EpicGames.Horde.Agents;
using EpicGames.Horde.Agents.Pools;
using EpicGames.Horde.Jobs;
using HordeCommon.Rpc.Messages;
using HordeServer.Agents;
using HordeServer.Auditing;
using HordeServer.Jobs;
using HordeServer.Utilities;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.DependencyInjection;

namespace HordeServer.Tests.Agents;

/// <summary>
///     Testing the agent service
/// </summary>
[TestClass]
public class AgentServiceTest : BuildTestSetup
{
	[TestMethod]
	public async Task GetJobsAsync()
	{
		await CreateFixtureAsync();

		ActionResult<List<object>> res = await JobsController.FindJobsAsync();

		List<GetJobResponse> responses = res.Value!.ConvertAll(x => (GetJobResponse)x);
		responses.SortBy(x => x.CommitId);

		Assert.AreEqual(2, responses.Count);
		Assert.AreEqual("hello1", responses[0].Name);
		Assert.AreEqual("hello2", responses[1].Name);

		res = await JobsController.FindJobsAsync(includePreflight: false);
		Assert.AreEqual(1, res.Value!.Count);
		Assert.AreEqual("hello2", (res.Value[0] as GetJobResponse)!.Name);
	}

	[TestMethod]
	public async Task CreateSessionTestAsync()
	{
		Fixture fixture = await CreateFixtureAsync();

		IAgent agent = await AgentService.CreateSessionAsync(fixture.Agent1, new RpcAgentCapabilities(), 
			"test");

		Assert.IsTrue(AgentService.AuthorizeSession(agent, GetUser(agent), out string _));
		await Clock.AdvanceAsync(TimeSpan.FromMinutes(20));
		Assert.IsFalse(AgentService.AuthorizeSession(agent, GetUser(agent), out string reason));
		Assert.IsTrue(reason.Contains("expired", StringComparison.Ordinal));
	}

	private static long ToUnixTime(DateTime dateTime)
	{
		return new DateTimeOffset(dateTime).ToUnixTimeSeconds();
	}

	[TestMethod]
	public async Task LastOnlineChangeDuringSessionCreateAsync()
	{
		// No session created yet, status change timestamp is empty
		IAgent agent = await AgentService.CreateAgentAsync("agent1", false, "");
		Assert.AreEqual(AgentStatus.Stopped, agent.Status);
		Assert.IsTrue(agent.LastOnlineTime.HasValue);

		// A session has been created, status change timestamp is current time
		agent = await AgentService.CreateSessionAsync(agent, new RpcAgentCapabilities(), "v1");
		Assert.AreEqual(AgentStatus.Ok, agent.Status);
		Assert.IsNull(agent.LastOnlineTime);

		await Clock.AdvanceAsync(TimeSpan.FromMinutes(1));
	}

	private static int s_agentId = 1;
	private async Task<IAgent> CreateAgentSessionAsync()
	{
		IAgent agent = await AgentService.CreateAgentAsync("agentServiceTest-" + s_agentId++, false, "");
		agent = await AgentService.CreateSessionAsync(agent, new RpcAgentCapabilities(), "v1");
		return agent;
	}

	private static void AssertEqual(DateTime expected, DateTime? actual)
	{
		// When saved as MongoDB documents, some precision is lost. This compares only Unix seconds.
		Assert.AreEqual(ToUnixTime(expected), ToUnixTime(actual!.Value));
	}

	[TestMethod]
	[DataRow(AgentStatus.Ok)]
	[DataRow(AgentStatus.Unhealthy)]
	[DataRow(AgentStatus.Stopping)]
	[DataRow(AgentStatus.Stopped)]
	[DataRow(AgentStatus.Unspecified)]
	public async Task LastOnlineChangeAsync(AgentStatus status)
	{
		IAgent agent = await CreateAgentSessionAsync();
		await Clock.AdvanceAsync(TimeSpan.FromMinutes(1));

		agent = (await AgentService.UpdateSessionAsync(agent, agent.SessionId!.Value, status, null, new List<RpcLease>()))!;
		if (agent.Status == AgentStatus.Stopped)
		{
			AssertEqual(Clock.UtcNow, agent.LastOnlineTime);
		}
		else
		{
			Assert.IsNull(agent.LastOnlineTime);
		}
	}

	[TestMethod]
	public async Task AuditLogAwsInstanceTypeChangesAsync()
	{
		Fixture fixture = await CreateFixtureAsync();
		IAuditLogChannel<AgentId> agentLogger = AgentCollection.GetLogger(fixture.Agent1.Id);

		async Task<bool> AuditLogContains(string text)
		{
			await agentLogger.FlushAsync();
			return await agentLogger.FindAsync().AnyAsync(x => x.Data.Contains(text, StringComparison.Ordinal));
		}

		List<string> props = new() { $"{KnownPropertyNames.AwsInstanceType}=m5.large" };
		IAgent agent = await AgentService.CreateSessionAsync(fixture.Agent1, new RpcAgentCapabilities(props), "test");
		Assert.IsFalse(await AuditLogContains("AWS EC2 instance type changed"));

		props = new() { $"{KnownPropertyNames.AwsInstanceType}=c6.xlarge" };
		agent = await AgentService.CreateSessionAsync(agent, new RpcAgentCapabilities(props), "test");
		Assert.IsTrue(await AuditLogContains("AWS EC2 instance type changed"));
	}

	[TestMethod]
	public async Task GetAgentRateTestAsync()
	{
		IAgent agent1 = await AgentService.CreateAgentAsync("agent1", false, "");
		IAgent agent2 = await AgentService.CreateAgentAsync("agent2", false, "");
		await AgentService.CreateSessionAsync(agent1, new RpcAgentCapabilities(new List<string>() { "aws-instance-type=c5.24xlarge", "osfamily=windows" }), "test");
		await AgentService.CreateSessionAsync(agent2, new RpcAgentCapabilities(new List<string>() { "aws-instance-type=c4.4xLARge", "osfamily=WinDowS" }), "test");

		List<AgentRateConfig> agentRateConfigs = new()
		{
			new AgentRateConfig() { Condition = "aws-instance-type == 'c5.24xlarge' && osfamily == 'windows'", Rate = 200, },
			new AgentRateConfig() { Condition = "aws-instance-type == 'c4.4xlarge' && osfamily == 'windows'", Rate = 300 }
		};
		await AgentService.UpdateRateTableAsync(agentRateConfigs);

		double? rate1 = await AgentService.GetRateAsync(agent1.Id);
		Assert.AreEqual(200, rate1!.Value, 0.1);

		double? rate2 = await AgentService.GetRateAsync(agent2.Id);
		Assert.AreEqual(300, rate2!.Value, 0.1);
	}

	[TestMethod]
	public async Task EphemeralTestAsync()
	{
		IAgent agent = await CreateAgentAsync(new PoolId("pool1"), ephemeral: true);
		Assert.IsTrue(agent.Ephemeral);

		agent = (await AgentService.GetAgentAsync(agent.Id))!;
		Assert.IsTrue(agent.Ephemeral);
		Assert.AreEqual(AgentStatus.Ok, agent.Status);

		// Let background task run for purging outdated sessions, which will terminate session for our agent
		await Clock.AdvanceAsync(TimeSpan.FromHours(1));
		await ServiceProvider.GetRequiredService<AgentCollection>().TickSharedAsync(CancellationToken.None);

		// Ephemeral agent is marked as deleted once its session is terminated
		Assert.IsTrue((await AgentService.GetAgentAsync(agent.Id))!.Deleted);

		await Clock.AdvanceAsync(TimeSpan.FromHours(25));
		await ServiceProvider.GetRequiredService<AgentCollection>().TickSharedAsync(CancellationToken.None);

		// Once more time has passed, the ephemeral agent marked as deleted is removed from database
		Assert.IsNull(await AgentService.GetAgentAsync(agent.Id));
	}

	private static ClaimsPrincipal GetUser(IAgent agent)
	{
		return new ClaimsPrincipal(new ClaimsIdentity(
			new List<Claim> { new(HordeClaimTypes.AgentSessionId, agent.SessionId.ToString()!) }, "TestAuthType"));
	}
}