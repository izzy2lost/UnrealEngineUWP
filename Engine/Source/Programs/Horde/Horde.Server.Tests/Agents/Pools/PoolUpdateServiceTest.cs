// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading.Tasks;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System.Threading;
using Horde.Server.Agents;
using Horde.Server.Agents.Pools;
using Microsoft.Extensions.Logging.Abstractions;

namespace Horde.Server.Tests.Agents.Pools
{
	[TestClass]
	public class PoolUpdateServiceTest : TestSetup
	{
		private readonly PoolUpdateService _pus;
		private IPool _pool = default!;
		private IAgent _enabledAgent = default!;
		private IAgent _disabledAgent = default!;
		private IAgent _disabledAgentBeyondGracePeriod = default!;
		
		public PoolUpdateServiceTest()
		{
			UpdateConfig(x => x.Pools.Clear());
			_pus = new (AgentCollection, PoolCollection, Clock, GlobalConfig, Tracer, new NullLogger<PoolUpdateService>());
		}

		[TestInitialize]
		public async Task SetupAsync()
		{
			_pool = await CreatePoolAsync("testPool", new CreatePoolConfigOptions { EnableAutoscaling = true });
			_enabledAgent = await CreateAgentAsync(_pool, true);
			_disabledAgent = await CreateAgentAsync(_pool, false);
			_disabledAgentBeyondGracePeriod = await CreateAgentAsync(_pool, enabled: false, adjustClockBy: -TimeSpan.FromHours(9));
		}

		private async Task RefreshAgentsAsync()
		{
			_enabledAgent = (await AgentService.GetAgentAsync(_enabledAgent.Id))!;
			_disabledAgent = (await AgentService.GetAgentAsync(_disabledAgent.Id))!;
			_disabledAgentBeyondGracePeriod = (await AgentService.GetAgentAsync(_disabledAgentBeyondGracePeriod.Id))!;
		}

		public override async ValueTask DisposeAsync()
		{
			GC.SuppressFinalize(this);
			await base.DisposeAsync();

			await _pus.DisposeAsync();
		}

		[TestMethod]
		public async Task ShutdownDisabledAgents_WithGlobalGracePeriod_RequestsShutdownAsync()
		{
			// Act
			await _pus.ShutdownDisabledAgentsAsync(CancellationToken.None);
			await RefreshAgentsAsync();
			
			// Assert
			Assert.IsFalse(_enabledAgent.RequestShutdown);
			Assert.IsFalse(_disabledAgent.RequestShutdown);
			Assert.IsTrue(_disabledAgentBeyondGracePeriod.RequestShutdown);
		}
		
		[TestMethod]
		public async Task ShutdownDisabledAgents_WithPerPoolGracePeriod_DoesNotRequestShutdownAsync()
		{
			// Arrange
			// Explicitly set the grace period for the pool to be longer than the default of 8 hours
#pragma warning disable CS0612 // Type or member is obsolete
			await PoolCollection.UpdateConfigAsync(_pool.Id, new UpdatePoolConfigOptions { ShutdownIfDisabledGracePeriod = TimeSpan.FromHours(24) });
#pragma warning restore CS0612 // Type or member is obsolete

			// Act
			await _pus.ShutdownDisabledAgentsAsync(CancellationToken.None);
			await RefreshAgentsAsync();
			
			// Assert
			Assert.IsFalse(_enabledAgent.RequestShutdown);
			Assert.IsFalse(_disabledAgent.RequestShutdown);
			Assert.IsFalse(_disabledAgentBeyondGracePeriod.RequestShutdown);
		}
		
		[TestMethod]
		public async Task ShutdownDisabledAgents_WithAutoScalingOff_DoesNotRequestShutdownAsync()
		{
			// Arrange
#pragma warning disable CS0612 // Type or member is obsolete
			await PoolCollection.UpdateConfigAsync(_pool.Id, new UpdatePoolConfigOptions { EnableAutoscaling = false });
#pragma warning restore CS0612 // Type or member is obsolete

			// Act
			await _pus.ShutdownDisabledAgentsAsync(CancellationToken.None);
			await RefreshAgentsAsync();
			
			// Assert
			Assert.IsFalse(_enabledAgent.RequestShutdown);
			Assert.IsFalse(_disabledAgent.RequestShutdown);
			Assert.IsFalse(_disabledAgentBeyondGracePeriod.RequestShutdown);
		}
	}
}
