// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Bundles;
using EpicGames.Horde.Storage.Bundles.V2;
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
			public byte[] Padding { get; set; } = Array.Empty<byte>();
			public NodeRef<TestNode>[] Refs { get; }

			public TestNode(int value, params NodeRef<TestNode>[] refs)
			{
				Value = value;
				Refs = refs;
			}

			public TestNode(IBlobReader reader)
			{
				Value = reader.ReadInt32();
				Padding = reader.ReadVariableLengthBytes().ToArray();
				Refs = reader.ReadVariableLengthArray(() => reader.ReadNodeRef<TestNode>());
			}

			public override void Serialize(IBlobWriter writer)
			{
				writer.WriteInt32(Value);
				writer.WriteVariableLengthBytes(Padding);
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

		[TestMethod]
		public async Task PacketFlushAsync()
		{
			await using BundleCache cache = new BundleCache();

			using MemoryStorageClient memoryStore = new MemoryStorageClient();
			using BundleStorageClient store = new BundleStorageClient(memoryStore, cache, NullLogger.Instance);

			await using IStorageWriter writer = store.CreateWriter(options: new BundleOptions { MinCompressionPacketSize = 100, MaxBlobSize = 1024 * 1024 });
			NodeRef<TestNode> nodeRef1 = await writer.WriteNodeAsync(new TestNode(123) { Padding = new byte[1024] });
			await writer.FlushAsync();
			NodeRef<TestNode> nodeRef2 = await writer.WriteNodeAsync(new TestNode(456, nodeRef1) { Padding = new byte[1024] });
			NodeRef<TestNode> nodeRef3 = await writer.WriteNodeAsync(new TestNode(789, nodeRef2));

			// nodeRef1 is in a flushed bundle
			BundleWriter.PendingExportHandle export1 = (BundleWriter.PendingExportHandle)nodeRef1.Handle;
			BundleWriter.PendingPacketHandle packet1 = (BundleWriter.PendingPacketHandle)export1.Outer!;
			BundleWriter.PendingBundleHandle bundle1 = (BundleWriter.PendingBundleHandle)packet1.Outer!;
			Assert.IsNotNull(packet1.FlushedHandle);
			Assert.IsNotNull(bundle1.FlushedHandle);

			TestNode node1 = await export1.ReadNodeAsync<TestNode>();
			Assert.AreEqual(123, node1.Value);

			// nodeRef2 is in a flushed packet, unflushed bundle
			BundleWriter.PendingExportHandle export2 = (BundleWriter.PendingExportHandle)nodeRef2.Handle;

			BundleWriter.PendingPacketHandle packet2 = (BundleWriter.PendingPacketHandle)export2.Outer!;
			Assert.AreNotEqual(packet1, packet2);
			Assert.IsNotNull(packet2.FlushedHandle);

			BundleWriter.PendingBundleHandle bundle2 = (BundleWriter.PendingBundleHandle)packet2.Outer!;
			Assert.AreNotEqual(bundle1, bundle2);
			Assert.IsNull(bundle2.FlushedHandle);

			TestNode node2 = await export2.ReadNodeAsync<TestNode>();
			Assert.AreEqual(456, node2.Value);

			// nodeRef3 is in an unflushed packet, unflushed bundle
			BundleWriter.PendingExportHandle export3 = (BundleWriter.PendingExportHandle)nodeRef3.Handle;

			BundleWriter.PendingPacketHandle packet3 = (BundleWriter.PendingPacketHandle)export3.Outer!;
			Assert.AreNotEqual(packet2, packet3);
			Assert.IsNull(packet3.FlushedHandle);

			BundleWriter.PendingBundleHandle bundle3 = (BundleWriter.PendingBundleHandle)packet3.Outer!;
			Assert.AreEqual(bundle2, bundle3);

			TestNode node3 = await export3.ReadNodeAsync<TestNode>();
			Assert.AreEqual(789, node3.Value);
		}
	}
}
