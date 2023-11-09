// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Clients;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace EpicGames.Horde.Tests
{
	[TestClass]
	public class StorageClientTests
	{
		[NodeType("{99601905-A089-4F6E-87F1-D603EEAF1B71}", 1)]
		class TestNode : Node
		{
			public int Value { get; }
			public NodeRef<TestNode>[] Refs { get; }

			public TestNode(int value, params NodeRef<TestNode>[] refs)
			{
				Value = value;
				Refs = refs;
			}

			public TestNode(IBlobReader reader)
			{
				Value = reader.ReadInt32();
				Refs = reader.ReadVariableLengthArray(() => reader.ReadNodeRef<TestNode>());
			}

			public override void Serialize(IBlobWriter writer)
			{
				writer.WriteInt32(Value);
				writer.WriteVariableLengthArray(Refs, x => writer.WriteNodeRef(x));
			}
		}

		static StorageClientTests()
		{
			Node.RegisterType<TestNode>();
		}

		[TestMethod]
		public async Task TestBasicAsync()
		{
			using MemoryStorageClient store = new MemoryStorageClient();
			await TestBasicAsync(store);
		}

		[TestMethod]
		public async Task TestBasicBundleV2Async()
		{
			using MemoryStorageClient store = new MemoryStorageClient();
			await using BundleCache cache = new BundleCache();
			using BundleStorageClient storeV2 = new BundleStorageClient(store, cache, NullLogger.Instance);
			await TestBasicAsync(storeV2);
		}

		[TestMethod]
		public async Task PendingRefsAsync()
		{
			await using BundleCache cache = new BundleCache();

			using MemoryStorageClient memoryStore = new MemoryStorageClient();
			using BundleStorageClient store = new BundleStorageClient(memoryStore, cache, NullLogger.Instance);

			NodeRef<TestNode> nodeRef2;
			await using (IStorageWriter writer = store.CreateWriter())
			{
				NodeRef<TestNode> nodeRef1 = await writer.WriteNodeAsync(new TestNode(123));
				nodeRef2 = await writer.WriteNodeAsync(new TestNode(456, nodeRef1));
			}
			await store.WriteRefAsync("hello", nodeRef2.Handle);

			TestNode output2 = await store.ReadRefAsync<TestNode>("hello");
			Assert.AreEqual(456, output2.Value);
			Assert.AreEqual(1, output2.Refs.Length);

			TestNode output1 = await output2.Refs[0].ExpandAsync();
			Assert.AreEqual(0, output1.Refs.Length);
			Assert.AreEqual(123, output1.Value);
		}

		static async Task TestBasicAsync(IStorageClient store)
		{
			NodeRef nodeRef;
			await using (IStorageWriter writer = store.CreateWriter())
			{
				nodeRef = await writer.WriteHashedNodeAsync(new TestNode(123));
			}
			await store.WriteRefAsync("hello", nodeRef.Handle);

			TestNode output = await store.ReadRefAsync<TestNode>("hello");
			Assert.AreEqual(123, output.Value);
		}
	}
}
