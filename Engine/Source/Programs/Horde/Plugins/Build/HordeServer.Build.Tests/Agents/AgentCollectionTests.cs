// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Agents;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Agents.Pools;
using HordeServer.Agents;
using HordeServer.Agents.Sessions;

namespace HordeServer.Tests.Agents;

[TestClass]
public class AgentCollectionTests : BuildTestSetup
{
	private IAgent _agent;
	private readonly AgentLease _lease1;
	private readonly AgentLease _lease2;
	private readonly AgentLease _leaseWithParent3;
	private readonly AgentLease _leaseWithParent4;

	public AgentCollectionTests()
	{
		_agent = CreateAgentAsync(new PoolId("pool1"), ephemeral: true).Result;
		_lease1 = new AgentLease(LeaseId.Parse("aaaaaaaaaaaaaaaaaaaaaaaa"), null, "lease", null, null, null, LeaseState.Pending, null, false, null);
		_lease2 = new AgentLease(LeaseId.Parse("bbbbbbbbbbbbbbbbbbbbbbbb"), null, "lease", null, null, null, LeaseState.Pending, null, false, null);
		_leaseWithParent3 = new AgentLease(LeaseId.Parse("cccccccccccccccccccccccc"), _lease1.Id, "leaseWithParent3", null, null, null, LeaseState.Pending, null, false, null);
		_leaseWithParent4 = new AgentLease(LeaseId.Parse("dddddddddddddddddddddddd"), _lease1.Id, "leaseWithParent4", null, null, null, LeaseState.Pending, null, false, null);
	}

	[TestMethod]
	public async Task AddLeaseAsync()
	{
		await _agent.TryAddLeaseAsync(_lease1);
		await UpdateAgentAsync();

		await _agent.TryAddLeaseAsync(_leaseWithParent3);
		await UpdateAgentAsync();

		await _agent.TryAddLeaseAsync(_leaseWithParent4);
		await UpdateAgentAsync();

		List<LeaseId> leases = await AgentCollection.FindActiveLeaseIdsAsync();

		Assert.AreEqual(3, leases.Count);
		Assert.IsTrue(leases.Contains(_lease1.Id));
		Assert.IsTrue(leases.Contains(_leaseWithParent3.Id));
		Assert.IsTrue(leases.Contains(_leaseWithParent4.Id));
	}

	[TestMethod]
	public async Task StartSessionAsync()
	{
		await _agent.TryAddLeaseAsync(_lease1);
		await UpdateAgentAsync();

		await _agent.TryAddLeaseAsync(_lease2);
		await UpdateAgentAsync();

		await _agent.TryCreateSessionAsync(new CreateSessionOptions(SessionIdUtils.GenerateNewId(), DateTime.UtcNow, AgentStatus.Ok,
			new List<string>(), new Dictionary<string, int>(), new List<PoolId>(), DateTime.UtcNow, null));

		List<LeaseId> leases = await AgentCollection.FindActiveLeaseIdsAsync();

		Assert.AreEqual(0, leases.Count);
	}

	[TestMethod]
	public async Task UpdateSession_WithEmptyLeasesAsync()
	{
		await _agent.TryAddLeaseAsync(_lease1);
		await UpdateAgentAsync();

		await _agent.TryAddLeaseAsync(_lease2);
		await UpdateAgentAsync();

		await _agent.TryUpdateSessionAsync(new UpdateSessionOptions { Leases = new List<AgentLease>() });

		List<LeaseId> leases = await AgentCollection.FindActiveLeaseIdsAsync();

		Assert.AreEqual(0, leases.Count);
	}

	[TestMethod]
	public async Task UpdateSession_WithNewLeasesAsync()
	{
		await _agent.TryAddLeaseAsync(_lease1);
		await UpdateAgentAsync();

		await _agent.TryUpdateSessionAsync(new UpdateSessionOptions { Leases = new List<AgentLease> { _lease1, _lease2 } });

		List<LeaseId> leases = await AgentCollection.FindActiveLeaseIdsAsync();

		Assert.AreEqual(2, leases.Count);
		Assert.IsTrue(leases.Contains(_lease1.Id));
		Assert.IsTrue(leases.Contains(_lease2.Id));
	}

	[TestMethod]
	public async Task UpdateSession_WithOneLeaseRemovedAsync()
	{
		await _agent.TryAddLeaseAsync(_lease1);
		await UpdateAgentAsync();

		await _agent.TryAddLeaseAsync(_lease2);
		await UpdateAgentAsync();

		await _agent.TryUpdateSessionAsync(new UpdateSessionOptions { Leases = new List<AgentLease> { _lease1 } });

		List<LeaseId> leases = await AgentCollection.FindActiveLeaseIdsAsync();

		Assert.AreEqual(1, leases.Count);
		Assert.IsTrue(leases.Contains(_lease1.Id));
	}

	[TestMethod]
	public async Task CancelLeaseAsync()
	{
		await _agent.TryAddLeaseAsync(_lease1);
		await UpdateAgentAsync();

		await _agent.TryAddLeaseAsync(_lease2);
		await UpdateAgentAsync();

		await _agent.TryCancelLeaseAsync(0);

		List<LeaseId> leases = await AgentCollection.FindActiveLeaseIdsAsync();

		Assert.AreEqual(1, leases.Count);
		Assert.IsTrue(leases.Contains(_lease2.Id));
	}

	[TestMethod]
	public async Task TerminateSessionAsync()
	{
		await _agent.TryAddLeaseAsync(_lease1);
		await UpdateAgentAsync();

		await _agent.TryAddLeaseAsync(_lease2);
		await UpdateAgentAsync();

		await _agent.TryTerminateSessionAsync();

		List<LeaseId> leases = await AgentCollection.FindActiveLeaseIdsAsync();

		Assert.AreEqual(0, leases.Count);
	}

	[TestMethod]
	public async Task GetChildLeaseIdsAsync()
	{
		await _agent.TryAddLeaseAsync(_lease1);
		await UpdateAgentAsync();

		await _agent.TryAddLeaseAsync(_leaseWithParent3);
		await UpdateAgentAsync();

		await _agent.TryAddLeaseAsync(_leaseWithParent4);
		await UpdateAgentAsync();

		List<LeaseId> leases = await AgentCollection.GetChildLeaseIdsAsync(_leaseWithParent3.ParentId!.Value);

		Assert.AreEqual(2, leases.Count);
		Assert.IsTrue(leases.Contains(_leaseWithParent3.Id));
		Assert.IsTrue(leases.Contains(_leaseWithParent4.Id));
	}

	private async Task UpdateAgentAsync()
	{
		_agent = (await AgentService.GetAgentAsync(_agent.Id))!;
	}
}