// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Horde.Agents.Pools;
using Horde.Server.Agents.Fleet;
using Horde.Server.Agents.Pools;
using Microsoft.AspNetCore.Mvc;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests
{
	[TestClass]
	public class PoolsControllerTest : TestSetup
	{
		public PoolsControllerTest()
		{
			UpdateConfig(x => x.Pools.Clear());
		}

		[TestMethod]
		public async Task GetPoolsTestAsync()
		{
			IPool pool1 = await CreatePoolAsync("Pool1", new CreatePoolConfigOptions { Properties = new Dictionary<string, string>() { { "foo", "bar" }, { "lorem", "ipsum" } } });
			ActionResult<List<object>> rawResult = await PoolsController.GetPoolsAsync();
			Assert.AreEqual(1, rawResult.Value!.Count);
			GetPoolResponse response = (rawResult.Value![0] as GetPoolResponse)!;
			Assert.AreEqual(pool1.Id.ToString(), response.Id);
			Assert.AreEqual(pool1.Name, response.Name);
#pragma warning disable CS0618
			Assert.AreEqual(pool1.SizeStrategy ?? default, response.SizeStrategy);
#pragma warning restore CS0618
		}

		[TestMethod]
		public async Task CreatePoolsTestAsync()
		{
			CreatePoolRequest request = new CreatePoolRequest
			{
				Name = "Pool1",
				ScaleOutCooldown = 111,
				ScaleInCooldown = 222,
				SizeStrategies = new List<PoolSizeStrategyMessage> { new() { Type = PoolSizeStrategy.JobQueue, Condition = "dayOfWeek == 'monday'", Config = @"{""ScaleOutFactor"": 22.0, ""ScaleInFactor"": 33.0}", ExtraAgentCount = 567 } },
				SizeStrategy = PoolSizeStrategy.JobQueue,
				JobQueueSettings = new JobQueueSettingsMessage(new JobQueueSettings(0.35, 0.85)),
				FleetManagers = new List<FleetManagerMessage> { new() { Type = FleetManagerType.AwsReuse, Condition = "dayOfWeek == 'monday'", Config = "{}" } },
			};

#pragma warning disable CS0618 // Type or member is obsolete
			ActionResult<CreatePoolResponse> result = await PoolsController.CreatePoolAsync(request);
#pragma warning restore CS0618 // Type or member is obsolete
			CreatePoolResponse response = result.Value!;

			IPool pool = (await PoolService.GetPoolAsync(new PoolId(response.Id)))!;
			Assert.AreEqual(request.Name, pool.Name);
			Assert.AreEqual(request.ScaleOutCooldown, (int)pool.ScaleOutCooldown!.Value.TotalSeconds);
			Assert.AreEqual(request.ScaleInCooldown, (int)pool.ScaleInCooldown!.Value.TotalSeconds);
			Assert.AreEqual(request.JobQueueSettings.ScaleOutFactor, pool.JobQueueSettings!.ScaleOutFactor, 0.0001);
			Assert.AreEqual(request.JobQueueSettings.ScaleInFactor, pool.JobQueueSettings!.ScaleInFactor, 0.0001);
			Assert.AreEqual(1, request.SizeStrategies.Count);
			Assert.AreEqual(request.SizeStrategies[0].Type, pool.SizeStrategies![0].Type);
			Assert.AreEqual(request.SizeStrategies[0].Condition!.Text, pool.SizeStrategies[0].Condition!.Text);
			Assert.AreEqual(request.SizeStrategies[0].Config, pool.SizeStrategies[0].Config);
			Assert.AreEqual(request.SizeStrategies[0].ExtraAgentCount, pool.SizeStrategies[0].ExtraAgentCount);
			Assert.AreEqual(1, request.FleetManagers.Count);
			Assert.AreEqual(request.FleetManagers[0].Type, pool.FleetManagers![0].Type);
			Assert.AreEqual(request.FleetManagers[0].Condition!.Text, pool.FleetManagers[0].Condition!.Text);
			Assert.AreEqual(request.FleetManagers[0].Config, pool.FleetManagers[0].Config);
		}

		[TestMethod]
		public async Task UpdatePoolTestAsync()
		{
			IPool pool1 = await CreatePoolAsync("Pool1", new CreatePoolConfigOptions { Properties = new Dictionary<string, string>() { { "foo", "bar" }, { "lorem", "ipsum" } } });

#pragma warning disable CS0618 // Type or member is obsolete
			await PoolsController.UpdatePoolAsync(pool1.Id.ToString(), new UpdatePoolRequest
			{
				Name = "Pool1Modified",
				SizeStrategies = new List<PoolSizeStrategyMessage>
				{
					new () { Type = PoolSizeStrategy.JobQueue, Condition = "true", Config = "{\"someConfig\": 123}", ExtraAgentCount = 123}
				},
				FleetManagers = new List<FleetManagerMessage>
				{
					new () { Type = FleetManagerType.AwsReuse, Condition = "true", Config = "{}"}
				},
				SizeStrategy = PoolSizeStrategy.JobQueue,
				JobQueueSettings = new JobQueueSettingsMessage { ScaleOutFactor = 25.0, ScaleInFactor = 0.3 }
			});
#pragma warning restore CS0618 // Type or member is obsolete

			ActionResult<object> getResult = await PoolsController.GetPoolAsync(pool1.Id.ToString());
			GetPoolResponse response = (getResult.Value! as GetPoolResponse)!;
			Assert.AreEqual("Pool1Modified", response.Name);
			Assert.AreEqual(PoolSizeStrategy.JobQueue, response.SizeStrategy);
			Assert.AreEqual(25.0, response.JobQueueSettings!.ScaleOutFactor);
			Assert.AreEqual(0.3, response.JobQueueSettings!.ScaleInFactor);

			Assert.AreEqual(1, response.SizeStrategies.Count);
			Assert.AreEqual(PoolSizeStrategy.JobQueue, response.SizeStrategies[0].Type);
			Assert.AreEqual("true", response.SizeStrategies[0].Condition!.Text);
			Assert.AreEqual("{\"someConfig\": 123}", response.SizeStrategies[0].Config);
			Assert.AreEqual(123, response.SizeStrategies[0].ExtraAgentCount);

			Assert.AreEqual(1, response.FleetManagers.Count);
			Assert.AreEqual(FleetManagerType.AwsReuse, response.FleetManagers[0].Type);
			Assert.AreEqual("true", response.FleetManagers[0].Condition!.Text);
			Assert.AreEqual("{}", response.FleetManagers[0].Config);
		}
	}
}