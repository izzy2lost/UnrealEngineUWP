// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Horde.Server.Server;
using Microsoft.Extensions.Diagnostics.HealthChecks;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests.Server
{
	[TestClass]
	public class ServerStatusServiceTest : TestSetup
	{
		[TestMethod]
		public void SubsystemArePrepopulated()
		{
			IReadOnlyList<SubsystemStatus> statuses = ServerStatusService.GetSubsystemStatuses();
			Assert.AreEqual(Enum.GetValues<Subsystem>().Length, statuses.Count);
			Assert.AreEqual(Subsystem.GlobalConfig.ToString(), statuses[0].Name);
			Assert.AreEqual(Subsystem.MongoDb.ToString(), statuses[1].Name);
			Assert.AreEqual(Subsystem.Redis.ToString(), statuses[2].Name);
			Assert.AreEqual(Subsystem.Perforce.ToString(), statuses[3].Name);
		}
		
		[TestMethod]
		public void UpdatesAreStoredNewToOld()
		{
			ServerStatusService.Report(Subsystem.GlobalConfig, HealthStatus.Unhealthy, "foo", DateTimeOffset.UtcNow - TimeSpan.FromSeconds(15));
			ServerStatusService.Report(Subsystem.GlobalConfig, HealthStatus.Healthy, "bar", DateTimeOffset.UtcNow);
			ServerStatusService.Report(Subsystem.GlobalConfig, HealthStatus.Unhealthy, "baz", DateTimeOffset.UtcNow - TimeSpan.FromSeconds(5));
			
			SubsystemStatus gcStatus = GetSubsystemStatus(Subsystem.GlobalConfig);
			Assert.AreEqual(3, gcStatus.Updates.Count);
			Assert.AreEqual("bar", gcStatus.Updates[0].Message);
			Assert.AreEqual("baz", gcStatus.Updates[1].Message);
			Assert.AreEqual("foo", gcStatus.Updates[2].Message);
		}
		
		[TestMethod]
		public void OnlyLastNUpdatesAreKept()
		{
			for (int i = 0; i < ServerStatusService.MaxHistoryLength + 10; i++)
			{
				ServerStatusService.Report(Subsystem.GlobalConfig, HealthStatus.Healthy, "foo", DateTimeOffset.UtcNow);	
			}

			Assert.AreEqual(ServerStatusService.MaxHistoryLength, GetSubsystemStatus(Subsystem.GlobalConfig).Updates.Count);
		}
		
		[TestMethod]
		public async Task MongoDbHealthCheckAsync()
		{
			// A MongoDB server is always present during test runs
			await ServerStatusService.UpdateMongoDbHealthAsync(CancellationToken.None);
			SubsystemStatus mongoDb = GetSubsystemStatus(Subsystem.MongoDb);
			Assert.AreEqual(1, mongoDb.Updates.Count);
			Assert.AreEqual(HealthStatus.Healthy, mongoDb.Updates[0].Result);
		}
		
		[TestMethod]
		public async Task RedisHealthCheckAsync()
		{
			// A Redis server is always present during test runs
			await ServerStatusService.UpdateRedisHealthAsync(CancellationToken.None);
			SubsystemStatus redis = GetSubsystemStatus(Subsystem.Redis);
			Assert.AreEqual(1, redis.Updates.Count);
			Assert.AreEqual(HealthStatus.Healthy, redis.Updates[0].Result);
		}

		private SubsystemStatus GetSubsystemStatus(Subsystem subsystem)
		{
			IReadOnlyList<SubsystemStatus> statuses = ServerStatusService.GetSubsystemStatuses();
			return statuses.First(x => x.Name == subsystem.ToString());
		}
	}
}
