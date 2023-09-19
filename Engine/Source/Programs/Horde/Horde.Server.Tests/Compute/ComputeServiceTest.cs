// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
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