// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde.Storage.Nodes;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using System.Linq;
using System.Threading.Tasks;
using System.IO;
using System.Threading;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Clients;
using Microsoft.Extensions.Caching.Memory;

namespace EpicGames.Horde.Tests
{
	[TestClass]
	public class ChunkingTests
	{
		[TestMethod]
		public void BuzHashTests()
		{
			byte[] data = new byte[4096];
			new Random(0).NextBytes(data);

			const int WindowSize = 128;

			uint rollingHash = 0;
			for (int maxIdx = 0; maxIdx < data.Length + WindowSize; maxIdx++)
			{
				int minIdx = maxIdx - WindowSize;

				if (maxIdx < data.Length)
				{
					rollingHash = BuzHash.Add(rollingHash, data[maxIdx]);
				}

				int length = Math.Min(maxIdx + 1, data.Length) - Math.Max(minIdx, 0);
				uint cleanHash = BuzHash.Add(0, data.AsSpan(Math.Max(minIdx, 0), length));
				Assert.AreEqual(rollingHash, cleanHash);

				if (minIdx >= 0)
				{
					rollingHash = BuzHash.Sub(rollingHash, data[minIdx], length);
				}
			}
		}

		[TestMethod]
		public async Task EmptyNodeTestAsync()
		{
			using KeyValueStorageClient store = KeyValueStorageClient.CreateInMemory();

			const string RefName = "hello";
			await using (IStorageWriter writer = store.CreateWriter(RefName))
			{
				ChunkingOptions options = new ChunkingOptions();
				options.LeafOptions = new LeafChunkedDataNodeOptions(64, 64, 64);
				options.InteriorOptions = new InteriorChunkedDataNodeOptions(4, 4, 4);

				using MemoryStream emptyStream = new MemoryStream();
				LeafChunkedData leafChunkedData = await LeafChunkedDataNode.CreateFromStreamAsync(writer, emptyStream, new LeafChunkedDataNodeOptions(64, 64, 64), CancellationToken.None);
				ChunkedData chunkedData = await InteriorChunkedDataNode.CreateTreeAsync(leafChunkedData, new InteriorChunkedDataNodeOptions(4, 4, 4), writer, BlobSerializerOptions.Default, CancellationToken.None); 

				DirectoryNode directory = new DirectoryNode();
				directory.AddFile("test.foo", FileEntryFlags.None, 0, chunkedData);

				IBlobHandle handle = await writer.WriteBlobAsync(directory);
				await store.WriteRefTargetAsync(RefName, handle);
			}
		}

		[TestMethod]
		public async Task FixedSizeChunkingTestsAsync()
		{
			ChunkingOptions options = new ChunkingOptions();
			options.LeafOptions = new LeafChunkedDataNodeOptions(64, 64, 64);
			options.InteriorOptions = new InteriorChunkedDataNodeOptions(4, 4, 4);

			await TestChunkingAsync(options);
		}

		[TestMethod]
		public async Task VariableSizeChunkingTestsAsync()
		{
			ChunkingOptions options = new ChunkingOptions();
			options.LeafOptions = new LeafChunkedDataNodeOptions(32, 64, 96);
			options.InteriorOptions = new InteriorChunkedDataNodeOptions(1, 4, 12);

			await TestChunkingAsync(options);
		}

		static async Task TestChunkingAsync(ChunkingOptions options)
		{
			using MemoryCache cache = new MemoryCache(new MemoryCacheOptions());

			using KeyValueStorageClient store = KeyValueStorageClient.CreateInMemory();

			await using IStorageWriter writer = store.CreateWriter();

			byte[] data = new byte[4096];
			new Random(0).NextBytes(data);

			for (int idx = 0; idx < data.Length; idx++)
			{
				data[idx] = (byte)idx;
			}

			ChunkedDataNodeRef handle;

			const int NumIterations = 100;
			{
				using ChunkedDataWriter fileWriter = new ChunkedDataWriter(writer, options, BlobSerializerOptions.Default);

				for (int idx = 0; idx < NumIterations; idx++)
				{
					await fileWriter.AppendAsync(data, CancellationToken.None);
				}

				handle = (await fileWriter.FlushAsync(CancellationToken.None)).Root;
			}

			ChunkedDataNode root = await handle.ReadBlobAsync();

			byte[] result;
			using (MemoryStream stream = new MemoryStream())
			{
				await root.CopyToStreamAsync(stream);
				result = stream.ToArray();
			}

			Assert.AreEqual(NumIterations * data.Length, result.Length);

			for (int idx = 0; idx < NumIterations; idx++)
			{
				ReadOnlyMemory<byte> spanData = result.AsMemory(idx * data.Length, data.Length);
				Assert.IsTrue(spanData.Span.SequenceEqual(data));
			}

			await CheckSizesAsync(root, options, true);
		}

		static async Task CheckSizesAsync(ChunkedDataNode node, ChunkingOptions options, bool rightmost)
		{
			if (node is LeafChunkedDataNode leafNode)
			{
				Assert.IsTrue(rightmost || leafNode.Data.Length >= options.LeafOptions.MinSize);
				Assert.IsTrue(leafNode.Data.Length <= options.LeafOptions.MaxSize);
			}
			else
			{
				InteriorChunkedDataNode interiorNode = (InteriorChunkedDataNode)node;

				Assert.IsTrue(rightmost || interiorNode.Children.Count >= options.InteriorOptions.MinChildCount);
				Assert.IsTrue(interiorNode.Children.Count <= options.InteriorOptions.MaxChildCount);

				int childCount = interiorNode.Children.Count;
				for (int idx = 0; idx < childCount; idx++)
				{
					ChunkedDataNode childNode = await interiorNode.Children[idx].ReadBlobAsync();
					await CheckSizesAsync(childNode, options, idx == childCount - 1);
				}
			}
		}
	}
}
