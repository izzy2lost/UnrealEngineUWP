// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Horde.Agents.Pools;
using Horde.Server.Agents.Pools;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests
{
	[TestClass]
    public class PoolServiceTests : TestSetup
    {
        private readonly Dictionary<string, string> _fixtureProps = new Dictionary<string, string>
        {
            {"foo", "bar"},
            {"lorem", "ipsum"}
        };

        public PoolServiceTests()
        {
			UpdateConfig(x => x.Pools.Clear());
        }

        private async Task<IPool> CreatePoolFixtureAsync(string name)
        {
            return await CreatePoolAsync(name, new CreatePoolConfigOptions { Properties = _fixtureProps });
        }

        [TestMethod]
        public async Task GetPoolTestAsync()
        {
            Assert.IsNull(await PoolService.GetPoolAsync(new PoolId("this-does-not-exist")));

            IPool newPool = await CreatePoolFixtureAsync("create-pool");
			IPool? pool = await PoolService.GetPoolAsync(newPool.Id);
			Assert.IsNotNull(pool);
            Assert.AreEqual("create-pool", pool!.Id.ToString());
            Assert.AreEqual("create-pool", pool.Name);
            Assert.AreEqual(_fixtureProps.Count, pool.Properties!.Count);
            Assert.AreEqual(_fixtureProps["foo"], pool.Properties["foo"]);
            Assert.AreEqual(_fixtureProps["lorem"], pool.Properties["lorem"]);
        }
        
        [TestMethod]
        public async Task GetPoolsTestAsync()
        {
            await GetMongoServiceSingleton().Database.DropCollectionAsync("Pools");
            
            List<IPoolConfig> pools = await PoolCollection.GetConfigsAsync();
            Assert.AreEqual(pools.Count, 0);

			IPool pool0 = await CreatePoolFixtureAsync("multiple-pools-0");
			IPool pool1 = await CreatePoolFixtureAsync("multiple-pools-1");
            pools = await PoolService.GetPoolsAsync();
            Assert.AreEqual(pools.Count, 2);
            Assert.AreEqual(pools[0].Name, pool0.Name);
            Assert.AreEqual(pools[1].Name, pool1.Name);
        }
        
        [TestMethod]
        public async Task DeletePoolTestAsync()
        {
#pragma warning disable CS0618 // Type or member is obsolete
			Assert.IsFalse(await PoolCollection.DeleteConfigAsync(new PoolId("this-does-not-exist")));
			IPool pool = await CreatePoolFixtureAsync("pool-to-be-deleted");
            Assert.IsTrue(await PoolCollection.DeleteConfigAsync(pool.Id));
#pragma warning restore CS0618 // Type or member is obsolete
		}

		[TestMethod]
        public async Task UpdatePoolTestAsync()
        {
			string uniqueSuffix = Guid.NewGuid().ToString("N");
            IPool pool = await CreatePoolFixtureAsync($"update-pool-{uniqueSuffix}");
            Dictionary<string, string?> updatedProps = new Dictionary<string, string?>
            {
                {"foo", "bar"},
                {"lorem", null}, // This entry will get removed
                {"cookies", "yumyum"},
            };

#pragma warning disable CS0618 // Type or member is obsolete
			await PoolCollection.UpdateConfigAsync(pool.Id, new UpdatePoolConfigOptions { Name = $"update-pool-new-name-{uniqueSuffix}", Properties = updatedProps });
#pragma warning restore CS0618 // Type or member is obsolete

			IPool? updatedPool = await PoolCollection.GetAsync(pool.Id);
			Assert.IsNotNull(updatedPool);
            Assert.AreEqual(pool.Id, updatedPool!.Id);
            Assert.AreEqual($"update-pool-new-name-{uniqueSuffix}", updatedPool.Name);
            Assert.AreEqual(updatedPool.Properties!.Count, 2);
            Assert.AreEqual(updatedProps["foo"], updatedPool.Properties["foo"]);
            Assert.AreEqual(updatedProps["cookies"], updatedPool.Properties["cookies"]);
        }
        
        [TestMethod]
        public async Task UpdatePoolCollectionTestAsync()
        {
	        IPool pool = await CreatePoolFixtureAsync("update-pool-2");
	        await pool.TryUpdateAsync(new UpdatePoolOptions { LastScaleUpTime = DateTime.UtcNow });
        }
    }
}