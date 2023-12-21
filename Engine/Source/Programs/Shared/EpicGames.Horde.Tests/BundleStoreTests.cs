// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Clients;
using EpicGames.Horde.Storage.Nodes;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using System.Buffers;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using System.Threading;
using System.Text;
using System.IO;
using EpicGames.Core;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Logging.Abstractions;
using System.Reflection;
using EpicGames.Horde.Storage.Bundles;
using EpicGames.Horde.Storage.Bundles.V1;

namespace EpicGames.Horde.Tests
{
	[TestClass]
	public class BundleStoreTests
	{
		[TestMethod]
		public async Task CreateBundlesManuallyV1Async()
		{
			byte[] a = CreateBundleManually();
			byte[] b = await CreateBundleNormalAsync();
			Assert.IsTrue(a.SequenceEqual(b));
		}

		[BlobType("{F63606D4-4061-5DBB-446F-55A69E22654F}")]
		class TextNode : Node
		{
			public string Text { get; }

			public TextNode(string text) => Text = text;

			public TextNode(IBlobReader reader)
			{
				Text = reader.ReadString();
			}

			public override void Serialize(IBlobWriter writer)
			{
				writer.WriteString(Text);
			}
		}

		public BundleStoreTests()
		{
			Node.RegisterTypesFromAssembly(Assembly.GetExecutingAssembly());
		}

		static async Task<byte[]> CreateBundleNormalAsync()
		{
			using MemoryStorageClient memoryStore = new MemoryStorageClient();
			using BundleStorageClient store = new BundleStorageClient(memoryStore, BundleCache.None, NullLogger.Instance);
			await using IStorageWriter writer = store.CreateWriter(options: new BundleOptions { MaxVersion = BundleVersion.ImportHashes, CompressionFormat = BundleCompressionFormat.None });

			TextNode node = new TextNode("Hello world");
			IBlobHandle handle = await writer.FlushAsync(node, CancellationToken.None);

			IBlobHandle bundleHandle = store.CreateBlobHandle(handle.GetLocator().BaseLocator);
			using BlobData blobData = await bundleHandle.ReadAsync();

			return blobData.Data.ToArray();
		}

		static byte[] CreateBundleManually()
		{
			ArrayMemoryWriter payloadWriter = new ArrayMemoryWriter(200);
			payloadWriter.WriteString("Hello world");
			byte[] payload = payloadWriter.WrittenMemory.ToArray();

			List<BlobType> types = new List<BlobType>();
			types.Add(new BlobType("{F63606D4-4061-5DBB-446F-55A69E22654F}", 1));

			List <BundleExport> exports = new List<BundleExport>();
			exports.Add(new BundleExport(0, 0, 0, payload.Length, Array.Empty<BundleExportRef>()));

			List<BundlePacket> packets = new List<BundlePacket>();
			packets.Add(new BundlePacket(BundleCompressionFormat.None, 0, payload.Length, payload.Length));

			BundleHeader header = new BundleHeader(types.ToArray(), Array.Empty<BlobLocator>(), exports.ToArray(), packets.ToArray());

			ReadOnlySequenceBuilder<byte> builder = new ReadOnlySequenceBuilder<byte>();
			header.AppendTo(builder);
			builder.Append(payload);
			return builder.Construct().ToArray();
		}

		[TestMethod]
		public async Task TestTreeAsync()
		{
			using IMemoryCache cache = new MemoryCache(new MemoryCacheOptions());

			using MemoryStorageClient blobStore = new MemoryStorageClient();
			using BundleStorageClient bundleStore = new BundleStorageClient(blobStore, BundleCache.None, NullLogger.Instance);

			await TestTreeAsync(bundleStore, new BundleOptions { MaxBlobSize = 1024 * 1024 });

			Assert.AreEqual(1, blobStore.Blobs.Count);
			Assert.AreEqual(1, blobStore.Refs.Count);
		}

		[TestMethod]
		public async Task TestTreeSeparateBlobsAsync()
		{
			using IMemoryCache cache = new MemoryCache(new MemoryCacheOptions());

			using MemoryStorageClient blobStore = new MemoryStorageClient();
			using BundleStorageClient bundleStore = new BundleStorageClient(blobStore, BundleCache.None, NullLogger.Instance);

			await TestTreeAsync(bundleStore, new BundleOptions { MaxBlobSize = 1 });

			Assert.AreEqual(5, blobStore.Blobs.Count);
			Assert.AreEqual(1, blobStore.Refs.Count);
		}

		[BlobType("{F63606D4-4061-5DBB-446F-55A69F22654F}")]
		class SimpleNode : Node
		{
			public ReadOnlySequence<byte> Data { get; }
			public IReadOnlyList<HashedNodeRef<SimpleNode>> Refs { get; }

			public SimpleNode(ReadOnlySequence<byte> data, IReadOnlyList<HashedNodeRef<SimpleNode>> refs)
			{
				Data = data;
				Refs = refs;
			}

			public SimpleNode(IBlobReader reader)
			{
				Data = new ReadOnlySequence<byte>(reader.ReadVariableLengthBytes());
				Refs = reader.ReadVariableLengthArray(() => reader.ReadHashedNodeRef<SimpleNode>());
			}

			public override void Serialize(IBlobWriter writer)
			{
				writer.WriteVariableLengthBytes(Data);
				writer.WriteVariableLengthArray(Refs, x => writer.WriteHashedNodeRef(x));
			}
		}

		static async Task TestTreeAsync(BundleStorageClient store, BundleOptions options)
		{
			// Generate a tree
			{
				await using IStorageWriter writer = store.CreateWriter("test", options);

				SimpleNode node1 = new SimpleNode(new ReadOnlySequence<byte>(new byte[] { 1 }), Array.Empty<HashedNodeRef<SimpleNode>>());
				SimpleNode node2 = new SimpleNode(new ReadOnlySequence<byte>(new byte[] { 2 }), new[] { await writer.WriteHashedNodeAsync(node1) });
				SimpleNode node3 = new SimpleNode(new ReadOnlySequence<byte>(new byte[] { 3 }), new[] { await writer.WriteHashedNodeAsync(node2) });
				SimpleNode node4 = new SimpleNode(new ReadOnlySequence<byte>(new byte[] { 4 }), Array.Empty<HashedNodeRef<SimpleNode>>());

				SimpleNode root = new SimpleNode(new ReadOnlySequence<byte>(new byte[] { 5 }), new[] { await writer.WriteHashedNodeAsync(node4), await writer.WriteHashedNodeAsync(node3) });
				NodeRef<SimpleNode> rootRef = await writer.WriteNodeAsync(root);
				await writer.FlushAsync();

				await store.WriteRefTargetAsync(new RefName("test"), rootRef);
			
				BundleReader reader = new BundleReader(store, BundleCache.None, NullLogger.Instance);
				await CheckTreeAsync(root);
			}

			// Check we can read it back in
			{
				SimpleNode root = await store.ReadRefAsync<SimpleNode>(new RefName("test"));
				await CheckTreeAsync(root);
			}
		}

		static async Task CheckTreeAsync(SimpleNode root)
		{
			SimpleNode node5 = root;
			byte[] data5 = node5.Data.ToArray();
			Assert.IsTrue(data5.SequenceEqual(new byte[] { 5 }));
			IReadOnlyList<HashedNodeRef<SimpleNode>> refs5 = node5.Refs;
			Assert.AreEqual(2, refs5.Count);

			SimpleNode node4 = await refs5[0].ExpandAsync();
			byte[] data4 = node4.Data.ToArray();
			Assert.IsTrue(data4.SequenceEqual(new byte[] { 4 }));
			IReadOnlyList<HashedNodeRef<SimpleNode>> refs4 = node4.Refs;
			Assert.AreEqual(0, refs4.Count);

			SimpleNode node3 = await refs5[1].ExpandAsync();
			byte[] data3 = node3.Data.ToArray();
			Assert.IsTrue(data3.SequenceEqual(new byte[] { 3 }));
			IReadOnlyList<HashedNodeRef<SimpleNode>> refs3 = node3.Refs;
			Assert.AreEqual(1, refs3.Count);

			SimpleNode node2 = await refs3[0].ExpandAsync();
			byte[] data2 = node2.Data.ToArray();
			Assert.IsTrue(data2.SequenceEqual(new byte[] { 2 }));
			IReadOnlyList<HashedNodeRef<SimpleNode>> refs2 = node2.Refs;
			Assert.AreEqual(1, refs2.Count);

			SimpleNode node1 = await refs2[0].ExpandAsync();
			byte[] data1 = node1.Data.ToArray();
			Assert.IsTrue(data1.SequenceEqual(new byte[] { 1 }));
			IReadOnlyList<HashedNodeRef<SimpleNode>> refs1 = node1.Refs;
			Assert.AreEqual(0, refs1.Count);
		}

		[TestMethod]
		public async Task SimpleNodeAsync()
		{
			using MemoryStorageClient store = new MemoryStorageClient();

			RefName refName = new RefName("test");

			NodeRef<SimpleNode> inputRef;
			await using (IStorageWriter writer = store.CreateWriter(refName))
			{
				inputRef = await writer.WriteNodeAsync(new SimpleNode(new ReadOnlySequence<byte>(new byte[] { (byte)123 }), Array.Empty<HashedNodeRef<SimpleNode>>()));
			}
			await store.WriteRefAsync(refName, inputRef.Handle);

			SimpleNode node = await store.ReadRefAsync<SimpleNode>(refName);

			Assert.AreEqual(123, node.Data.FirstSpan[0]);
		}

		[TestMethod]
		public async Task DirectoryNodesAsync()
		{
			using MemoryStorageClient store = new MemoryStorageClient();

			// Generate a tree
			{
				await using (IStorageWriter writer = store.CreateWriter(new RefName("test")))
				{
					DirectoryNode world = new DirectoryNode();
					HashedNodeRef<DirectoryNode> worldRef = await writer.WriteHashedNodeAsync(world);

					DirectoryNode hello = new DirectoryNode();
					hello.AddDirectory(new DirectoryEntry("world", 0, worldRef));
					HashedNodeRef<DirectoryNode> helloRef = await writer.WriteHashedNodeAsync(hello);

					DirectoryNode root = new DirectoryNode(DirectoryFlags.None);
					root.AddDirectory(new DirectoryEntry("hello", 0, helloRef));
					HashedNodeRef<DirectoryNode> rootRef = await writer.WriteHashedNodeAsync(root);

					await writer.FlushAsync();

					await store.WriteRefTargetAsync(new RefName("test"), rootRef.Handle);
				}
			}

			// Check we can read it back in
			{
				DirectoryNode root = await store.ReadRefAsync<DirectoryNode>(new RefName("test"));
				await CheckDirectoryTreeAsync(root);
			}
		}

		static async Task CheckDirectoryTreeAsync(DirectoryNode root)
		{
			Assert.AreEqual(1, root.Directories.Count);
			Assert.AreEqual("hello", root.Directories.First().Name);

			DirectoryNode hello = await root.Directories.First().Target.ExpandAsync(CancellationToken.None);
			Assert.AreEqual(1, hello.Directories.Count);
			Assert.AreEqual("world", hello.Directories.First().Name);

			DirectoryNode world = await hello.Directories.First().Target.ExpandAsync(CancellationToken.None);
			Assert.AreEqual(0, world.Directories.Count);
		}

		[TestMethod]
		public async Task FileNodesAsync()
		{
			using IMemoryCache cache = new MemoryCache(new MemoryCacheOptions());
			using MemoryStorageClient store = new MemoryStorageClient();
			BundleReader reader = new BundleReader(store, BundleCache.None, NullLogger.Instance);

			// Generate a tree
			{
				await using IStorageWriter writer = store.CreateWriter();

				using ChunkedDataWriter fileWriter = new ChunkedDataWriter(writer, new ChunkingOptions());
				ChunkedData fileHandle = await fileWriter.CreateAsync(Encoding.UTF8.GetBytes("world"), CancellationToken.None);

				List<FileUpdate> fileUpdates = new List<FileUpdate>();
				fileUpdates.Add(new FileUpdate("hello/world", FileEntryFlags.None, fileWriter.Length, fileHandle));

				DirectoryNode root = new DirectoryNode();
				await root.UpdateAsync(fileUpdates, writer);

				HashedNodeRef<DirectoryNode> rootRef = await writer.WriteHashedNodeAsync(root);
				await store.WriteRefTargetAsync(new RefName("test"), rootRef);

				await CheckFileTreeAsync(root);
			}

			// Check we can read it back in
			{
				DirectoryNode root = await store.ReadRefAsync<DirectoryNode>(new RefName("test"));
				await CheckFileTreeAsync(root);
			}
		}

		static async Task CheckFileTreeAsync(DirectoryNode root)
		{
			Assert.AreEqual(1, root.Directories.Count);
			Assert.AreEqual("hello", root.Directories.First().Name);

			DirectoryNode hello = await root.Directories.First().Target.ExpandAsync();
			Assert.AreEqual(0, hello.Directories.Count);
			Assert.AreEqual(1, hello.Files.Count);
			Assert.AreEqual("world", hello.Files.First().Name);

			ChunkedDataNode world = await hello.Files.First().Target.ExpandAsync();

			byte[] worldData = await GetFileDataAsync(world);
			Assert.IsTrue(worldData.SequenceEqual(Encoding.UTF8.GetBytes("world")));
		}

		[TestMethod]
		public async Task StreamTestAsync()
		{
			using MemoryStorageClient memoryStore = new MemoryStorageClient();
			using BundleStorageClient store = new BundleStorageClient(memoryStore, BundleCache.None, NullLogger.Instance);

			const int Length = 4096;

			byte[] chunk = new byte[Length];
			new Random(0).NextBytes(chunk);

			// Generate a tree
			HashedNodeRef<ChunkedDataNode> nodeRef;
			{
				await using IStorageWriter writer = store.CreateWriter(options: new BundleOptions { MaxBlobSize = 1024 });

				ChunkingOptions options = new ChunkingOptions();
				options.LeafOptions = new LeafChunkedDataNodeOptions(128, 256, 64 * 1024);

				using ChunkedDataWriter fileWriter = new ChunkedDataWriter(writer, options);
				for (int idx = 0; idx < chunk.Length / 16; idx++)
				{
					await fileWriter.AppendAsync(chunk.AsMemory(idx * 16, 16), CancellationToken.None);
				}

				nodeRef = (await fileWriter.FlushAsync(CancellationToken.None)).Root;
			}

			// Check we can read it back in
			{
				ChunkedDataNode newRoot = await nodeRef.ExpandAsync();

				using MemoryStream stream = new MemoryStream();
				await newRoot.CopyToStreamAsync(stream, CancellationToken.None);

				byte[] output = stream.ToArray();
				Assert.IsTrue(chunk.SequenceEqual(output));
			}
		}

		[TestMethod]
		public async Task LargeFileTestAsync()
		{
			using IMemoryCache cache = new MemoryCache(new MemoryCacheOptions());
			using MemoryStorageClient memoryStore = new MemoryStorageClient();
			using BundleStorageClient store = new BundleStorageClient(memoryStore, BundleCache.None, NullLogger.Instance);

			const int Length = 1024;
			const int Copies = 4096;

			byte[] chunk = new byte[Length];
			new Random(0).NextBytes(chunk);

			byte[] data = new byte[chunk.Length * Copies];
			for (int idx = 0; idx < Copies; idx++)
			{
				chunk.CopyTo(data.AsSpan(idx * chunk.Length));
			}

			// Generate a tree
			DirectoryNode root;
			{
				await using DedupeStorageWriter writer = new DedupeStorageWriter(store.CreateWriter(options: new BundleOptions { MaxBlobSize = 1024 }));

				ChunkingOptions options = new ChunkingOptions();
				options.LeafOptions = new LeafChunkedDataNodeOptions(128, 256, 64 * 1024);

				using ChunkedDataWriter fileWriter = new ChunkedDataWriter(writer, options);
				ChunkedData chunkedData = await fileWriter.CreateAsync(data, CancellationToken.None);

				root = new DirectoryNode(DirectoryFlags.None);
				root.AddFile("test", FileEntryFlags.None, fileWriter.Length, chunkedData);

				HashedNodeRef<DirectoryNode> rootRef = await writer.WriteHashedNodeAsync(root);
				await store.WriteRefTargetAsync(new RefName("test"), rootRef);

				await CheckLargeFileTreeAsync(root, data);
			}

			// Check we can read it back in
			{
				DirectoryNode newRoot = await store.ReadRefAsync<DirectoryNode>(new RefName("test"));
				await CompareTreesAsync(root, newRoot);
				await CheckLargeFileTreeAsync(root, data);

				FileEntry file = root.GetFileEntry("test");

				Dictionary<BlobLocator, long> locatorToSize = new Dictionary<BlobLocator, long>();
				await GetUniqueBlobsAsync(file.Target.Handle, locatorToSize);

				long uniqueSize = locatorToSize.Sum(x => x.Value);
				Assert.IsTrue(uniqueSize < data.Length / 3); // random fraction meaning "lots of dedupe happened"
			}
		}

		static async Task GetUniqueBlobsAsync(IBlobHandle handle, Dictionary<BlobLocator, long> locatorToSize)
		{
			using BlobData data = await handle.ReadAsync();
			locatorToSize[handle.GetLocator()] = data.Data.Length;

			foreach (IBlobHandle reference in data.Refs)
			{
				await GetUniqueBlobsAsync(reference, locatorToSize);
			}
		}

		static async Task CompareTreesAsync(DirectoryNode oldNode, DirectoryNode newNode)
		{
			Assert.AreEqual(oldNode.Length, newNode.Length);
			Assert.AreEqual(oldNode.Files.Count, newNode.Files.Count);
			Assert.AreEqual(oldNode.Directories.Count, newNode.Directories.Count);

			foreach ((FileEntry oldFileEntry, FileEntry newFileEntry) in oldNode.Files.Zip(newNode.Files))
			{
				ChunkedDataNode oldFile = await oldFileEntry.Target.ExpandAsync();
				ChunkedDataNode newFile = await newFileEntry.Target.ExpandAsync();
				await CompareTreesAsync(oldFile, newFile);
			}
		}

		static async Task CompareTreesAsync(ChunkedDataNode oldNode, ChunkedDataNode newNode)
		{
			if (oldNode is InteriorChunkedDataNode oldInteriorNode)
			{
				InteriorChunkedDataNode newInteriorNode = (InteriorChunkedDataNode)newNode;
				Assert.AreEqual(oldInteriorNode.Children.Count, newInteriorNode.Children.Count);

				int index = 0;
				foreach ((HashedNodeRef<ChunkedDataNode> oldFileRef, HashedNodeRef<ChunkedDataNode> newFileRef) in oldInteriorNode.Children.Zip(newInteriorNode.Children))
				{
					ChunkedDataNode oldFile = await oldFileRef.ExpandAsync();
					ChunkedDataNode newFile = await newFileRef.ExpandAsync();
					await CompareTreesAsync(oldFile, newFile);
					index++;
				}
			}
			else if (oldNode is LeafChunkedDataNode oldLeafNode)
			{
				LeafChunkedDataNode newLeafNode = (LeafChunkedDataNode)newNode;
				Assert.IsTrue(oldLeafNode.Data.Span.SequenceEqual(newLeafNode.Data.Span));
			}
			else
			{
				throw new NotImplementedException();
			}
		}

		static async Task CheckLargeFileTreeAsync(DirectoryNode root, byte[] data)
		{
			Assert.AreEqual(0, root.Directories.Count);
			Assert.AreEqual(1, root.Files.Count);

			ChunkedDataNode world = await root.Files.First().Target.ExpandAsync(CancellationToken.None);

			int length = await CheckFileDataAsync(world, data);
			Assert.AreEqual(data.Length, length);
		}

		static async Task<int> CheckFileDataAsync(ChunkedDataNode fileNode, ReadOnlyMemory<byte> data)
		{
			int offset = 0;
			if (fileNode is LeafChunkedDataNode leafNode)
			{
				Assert.IsTrue(leafNode.Data.Span.SequenceEqual(data.Span.Slice(offset, leafNode.Data.Length)));
				offset += leafNode.Data.Length;
			}
			else if (fileNode is InteriorChunkedDataNode interiorFileNode)
			{
				foreach (HashedNodeRef<ChunkedDataNode> childRef in interiorFileNode.Children)
				{
					ChunkedDataNode child = await childRef.ExpandAsync();
					offset += await CheckFileDataAsync(child, data.Slice(offset));
				}
			}
			else
			{
				throw new NotImplementedException();
			}
			return offset;
		}

		static async Task<byte[]> GetFileDataAsync(ChunkedDataNode fileNode)
		{
			using (MemoryStream stream = new MemoryStream())
			{
				await fileNode.CopyToStreamAsync(stream, CancellationToken.None);
				return stream.ToArray();
			}
		}
	}
}
