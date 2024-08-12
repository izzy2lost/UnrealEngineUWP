// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Linq.Expressions;
using System.Threading.Tasks;
using EpicGames.Redis;
using HordeServer.Server;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using StackExchange.Redis;

namespace HordeServer.Tests.Server
{
	[TestClass]
	public class RedisTests : DatabaseIntegrationTest
	{
		class TestClass
		{
			public int Foo { get; set; }
			public int Bar { get; set; }
			public int Baz { get; set; }
		}

		[TestMethod]
		public async Task HashTestAsync()
		{
			IRedisService redisService = GetRedisServiceSingleton();
			IDatabase database = redisService.GetDatabase();

			RedisHashKey<TestClass> key = new RedisHashKey<TestClass>("test");
			await database.HashSetAsync(key, new TestClass { Foo = 123, Bar = 456, Baz = 789 });

			TestClass value = await database.HashGetAllAsync(key);
			Assert.AreEqual(123, value.Foo);
			Assert.AreEqual(456, value.Bar);
			Assert.AreEqual(789, value.Baz);

			TestClass value2 = await database.HashGetAsync(key, new Expression<Func<TestClass, object>>[] { x => x.Foo, x => x.Bar });
			Assert.AreEqual(123, value2.Foo);
			Assert.AreEqual(456, value2.Bar);
			Assert.AreEqual(0, value2.Baz);
		}
	}
}
