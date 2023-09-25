// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.Metrics;
using System.Linq;
using System.Threading.Tasks;
using EpicGames.Horde.Compute;
using Horde.Server.Compute;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using OpenTelemetry.Trace;

namespace Horde.Server.Tests.Compute
{
	[TestClass]
	public class ComputeServiceTest : TestSetup
	{
		private static void AssertContainsMeasurement(List<Measurement<int>> actualMeasurements, int expectedValue, string expectedResource, string expectedClusterId, string expectedPool)
		{
			Dictionary<string, string> expectedTags = new()
			{
				{ "resource", expectedResource },
				{ "cluster", expectedClusterId },
				{ "pool", expectedPool }
			};
			
			foreach (Measurement<int> measurement in actualMeasurements)
			{
				Dictionary<string, string> actualTags = measurement.Tags.ToArray().ToDictionary(
					kvp => kvp.Key,
					kvp => (string)kvp.Value!);
				
				bool areTagsEqual = actualTags.Count == expectedTags.Count && 
					actualTags.OrderBy(kvp => kvp.Key)
					.SequenceEqual(expectedTags.OrderBy(kvp => kvp.Key));
				
				if (areTagsEqual && expectedValue == measurement.Value)
				{
					return;
				}
			}

			Console.WriteLine("Actual:");
			foreach (Measurement<int> m in actualMeasurements)
			{
				Console.WriteLine($"Measurement(value={m.Value} tags={String.Join(',', m.Tags.ToArray())}");
			}
			Assert.Fail("Unable to find measurement");
		}
		
		[TestMethod]
		public async Task ResourceNeedsMetricAsync()
		{
			await ComputeService.SetResourceNeedsAsync(new ClusterId("cluster1"), "session1", "pool1", new () { {"cpu", 101}, {"ram", 1000} });
			await ComputeService.SetResourceNeedsAsync(new ClusterId("cluster1"), "session1", "pool1", new () { {"cpu", 102}, {"ram", 1001} });
			await ComputeService.SetResourceNeedsAsync(new ClusterId("cluster1"), "session2", "pool1", new () { {"cpu", 210}, {"ram", 1100} });
			await ComputeService.SetResourceNeedsAsync(new ClusterId("cluster1"), "session2", "pool1", new () { {"cpu", 220}, {"ram", 1200} });
			
			await ComputeService.SetResourceNeedsAsync(new ClusterId("cluster1"), "session1", "pool2", new () { {"cpu", 301}, {"ram", 1300} });
			await ComputeService.SetResourceNeedsAsync(new ClusterId("cluster1"), "session2", "pool2", new () { {"cpu", 410}, {"ram", 1400} });
			
			await ComputeService.SetResourceNeedsAsync(new ClusterId("cluster2"), "session1", "pool1", new () { {"cpu", 20}, {"ram", 4000} });
			await ComputeService.SetResourceNeedsAsync(new ClusterId("cluster2"), "session2", "pool1", new () { {"cpu", 30}, {"ram", 5500} });
			
			List<Measurement<int>> measurements = await ComputeService.CalculateResourceNeedsAsync();
			Assert.AreEqual(6, measurements.Count);
			AssertContainsMeasurement(measurements, 322, "cpu", "cluster1", "pool1");
			AssertContainsMeasurement(measurements, 2201, "ram", "cluster1", "pool1");
			AssertContainsMeasurement(measurements, 711, "cpu", "cluster1", "pool2");
			AssertContainsMeasurement(measurements, 2700, "ram", "cluster1", "pool2");
			AssertContainsMeasurement(measurements, 50, "cpu", "cluster2", "pool1");
			AssertContainsMeasurement(measurements, 9500, "ram", "cluster2", "pool1");
		}

		[TestMethod]
		public async Task ResourceNeedsAreReplacedAsync()
		{
			await ComputeService.SetResourceNeedsAsync(new ClusterId("cluster1"), "session1", "pool1", new () { {"cpu", 1}, {"ram", 5} });
			await ComputeService.SetResourceNeedsAsync(new ClusterId("cluster1"), "session1", "pool1", new () { {"cpu", 10}, {"ram", 50} });
			List<ComputeService.SessionResourceNeeds> result = await ComputeService.GetResourceNeedsAsync();
			Assert.AreEqual(1, result.Count);
			Assert.AreEqual(10, result[0].ResourceNeeds["cpu"]);
			Assert.AreEqual(50, result[0].ResourceNeeds["ram"]);
		}
		
		[TestMethod]
		public async Task ResourceNeedsAreRemovedWhenOutdatedAsync()
		{
			await ComputeService.SetResourceNeedsAsync(new ClusterId("cluster1"), "session1", "pool1", new () { {"cpu", 1}, {"ram", 5} });
			Assert.AreEqual(1, (await ComputeService.GetResourceNeedsAsync()).Count);

			await Clock.AdvanceAsync(TimeSpan.FromMinutes(6));
			Assert.AreEqual(0, (await ComputeService.GetResourceNeedsAsync()).Count);
		}

		[TestMethod]
		public async Task DeniedRequestAsync()
		{
			await ComputeService.LogRequestAsync(AllocationOutcome.Denied, "req1", new Requirements { Pool = "pool1" }, null, Tracer.CurrentSpan);
			await Clock.AdvanceAsync(TimeSpan.FromSeconds(5));
			await ComputeService.LogRequestAsync(AllocationOutcome.Denied, "req1", new Requirements { Pool = "pool1" }, null, Tracer.CurrentSpan);
			await Clock.AdvanceAsync(TimeSpan.FromSeconds(5));
			await ComputeService.LogRequestAsync(AllocationOutcome.Denied, "req1", new Requirements { Pool = "pool1" }, null, Tracer.CurrentSpan);

			List<ComputeService.RequestInfo> ids = await ComputeService.GetUnservedRequestsAsync();
			Assert.AreEqual(1, ids.Count);
		}
		
		[TestMethod]
		public async Task AcceptedRequestAsync()
		{
			await ComputeService.LogRequestAsync(AllocationOutcome.Accepted, "req1", new Requirements { Pool = "pool1" }, null, Tracer.CurrentSpan);
			await Clock.AdvanceAsync(TimeSpan.FromSeconds(5));
			await ComputeService.LogRequestAsync(AllocationOutcome.Accepted, "req1", new Requirements { Pool = "pool1" }, null, Tracer.CurrentSpan);
			await Clock.AdvanceAsync(TimeSpan.FromSeconds(5));
			await ComputeService.LogRequestAsync(AllocationOutcome.Accepted, "req2", new Requirements { Pool = "pool1" }, null, Tracer.CurrentSpan);

			List<ComputeService.RequestInfo> ids = await ComputeService.GetUnservedRequestsAsync();
			Assert.AreEqual(0, ids.Count);
		}
		
		[TestMethod]
		public async Task DeniedThenAcceptedRequestAsync()
		{
			await ComputeService.LogRequestAsync(AllocationOutcome.Denied, "req1", new Requirements { Pool = "pool1" }, null, Tracer.CurrentSpan);
			await Clock.AdvanceAsync(TimeSpan.FromSeconds(5));
			await ComputeService.LogRequestAsync(AllocationOutcome.Accepted, "req1", new Requirements { Pool = "pool1" }, null, Tracer.CurrentSpan);
			await Clock.AdvanceAsync(TimeSpan.FromSeconds(5));
			await ComputeService.LogRequestAsync(AllocationOutcome.Denied, "req1", new Requirements { Pool = "pool1" }, null, Tracer.CurrentSpan);

			List<ComputeService.RequestInfo> ids = await ComputeService.GetUnservedRequestsAsync();
			Assert.AreEqual(0, ids.Count);
		}
		
		[TestMethod]
		public async Task OnlyIncludeLastMinuteAsync()
		{
			await ComputeService.LogRequestAsync(AllocationOutcome.Denied, "req1", new Requirements { Pool = "pool1" }, null, Tracer.CurrentSpan);
			await Clock.AdvanceAsync(TimeSpan.FromMinutes(61));
			await ComputeService.LogRequestAsync(AllocationOutcome.Denied, "req2", new Requirements { Pool = "pool1" }, null, Tracer.CurrentSpan);

			List<ComputeService.RequestInfo> ids = await ComputeService.GetUnservedRequestsAsync();
			Assert.AreEqual(1, ids.Count);
			Assert.AreEqual("req2", ids[0].RequestId);
		}
		
		[TestMethod]
		public async Task ComplexAsync()
		{
			await ComputeService.LogRequestAsync(AllocationOutcome.Denied, "req1", new Requirements { Pool = "pool1" }, null, Tracer.CurrentSpan);
			await Clock.AdvanceAsync(TimeSpan.FromSeconds(5));
			await ComputeService.LogRequestAsync(AllocationOutcome.Denied, "req2", new Requirements { Pool = "pool1" }, null, Tracer.CurrentSpan);
			await Clock.AdvanceAsync(TimeSpan.FromSeconds(5));
			await ComputeService.LogRequestAsync(AllocationOutcome.Denied, "req1", new Requirements { Pool = "pool1" }, null, Tracer.CurrentSpan);
			await Clock.AdvanceAsync(TimeSpan.FromSeconds(5));
			await ComputeService.LogRequestAsync(AllocationOutcome.Denied, "req2", new Requirements { Pool = "pool1" }, null, Tracer.CurrentSpan);
			await Clock.AdvanceAsync(TimeSpan.FromSeconds(5));
			await ComputeService.LogRequestAsync(AllocationOutcome.Denied, "req3", new Requirements { Pool = "pool1" }, null, Tracer.CurrentSpan);
			await Clock.AdvanceAsync(TimeSpan.FromSeconds(5));
			await ComputeService.LogRequestAsync(AllocationOutcome.Accepted, "req2", new Requirements { Pool = "pool1" }, null, Tracer.CurrentSpan);

			List<ComputeService.RequestInfo> ids = await ComputeService.GetUnservedRequestsAsync();
			CollectionAssert.AreEquivalent(new List<string> { "req1", "req3" }, ids.Select(x => x.RequestId).ToList());
		}

		[TestMethod]
		public void SerializeRequestInfo()
		{
			{
				ComputeService.RequestInfo req = new(DateTimeOffset.UnixEpoch, AllocationOutcome.Accepted, "reqId1", "pool1", "parent1");
				string serialized = req.Serialize();
				ComputeService.RequestInfo deserialized = ComputeService.RequestInfo.Deserialize(serialized)!;
				Assert.AreEqual(DateTimeOffset.UnixEpoch, deserialized!.Timestamp);
				Assert.AreEqual(AllocationOutcome.Accepted, deserialized.Outcome);
				Assert.AreEqual("reqId1", deserialized.RequestId);
				Assert.AreEqual("pool1", deserialized.Pool);
				Assert.AreEqual("parent1", deserialized.ParentLeaseId);
			}

			{
				ComputeService.RequestInfo req = new(DateTimeOffset.UnixEpoch, AllocationOutcome.Accepted, "reqId1", "pool1", null);
				string serialized = req.Serialize();
				ComputeService.RequestInfo deserialized = ComputeService.RequestInfo.Deserialize(serialized)!;
				Assert.IsNull(deserialized.ParentLeaseId);
			}
		}

		[TestMethod]
		public void GroupByPoolAndCount()
		{
			List<ComputeService.RequestInfo> ris = new()
			{
				new ComputeService.RequestInfo(DateTimeOffset.UnixEpoch, AllocationOutcome.Denied, "reqId1", "poolA", null),
				new ComputeService.RequestInfo(DateTimeOffset.UnixEpoch, AllocationOutcome.Denied, "reqId2", "poolA", null),
				new ComputeService.RequestInfo(DateTimeOffset.UnixEpoch, AllocationOutcome.Denied, "reqId3", "poolA", null),
				new ComputeService.RequestInfo(DateTimeOffset.UnixEpoch, AllocationOutcome.Denied, "reqId4", "poolB", null),
				new ComputeService.RequestInfo(DateTimeOffset.UnixEpoch, AllocationOutcome.Denied, "reqId4", "poolB", null),
			};
			Dictionary<string, int> result = ComputeService.GroupByPoolAndCount(ris);
			Assert.AreEqual(3, result["poolA"]);
			Assert.AreEqual(2, result["poolB"]);
		}
	}
}