// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using Horde.Server.Server;
using Horde.Server.Storage;

namespace Horde.Server.Tests
{
	[TestClass]
	public class BlobStoreTests : TestSetup
	{
		static readonly BlobType s_blobType = new BlobType(Guid.Parse("{AFDF76A7-5333-4DEE-B837-B5F5CA511245}"), 1);

		IStorageClient CreateStorageClient()
		{
			GlobalConfig globalConfig = new GlobalConfig();
			globalConfig.Storage.Backends.Add(new BackendConfig { Id = new BackendId("default-backend"), Type = StorageBackendType.Memory });
			globalConfig.Storage.Namespaces.Add(new NamespaceConfig { Id = new NamespaceId("default"), Backend = new BackendId("default-backend"), GcDelayHrs = 0.0 });
			SetConfig(globalConfig);

			return StorageService.CreateClient(new NamespaceId("default"));
		}

		static byte[] CreateTestData(int length, int seed)
		{
			byte[] data = new byte[length];
			new Random(seed).NextBytes(data);
			return data;
		}

		class Blob
		{
			public ReadOnlyMemory<byte> Data { get; set; }
			public List<BlobLocator> References { get; set; }

			public Blob() : this(ReadOnlyMemory<byte>.Empty, new List<BlobLocator>())
			{
			}

			public Blob(ReadOnlyMemory<byte> data, IEnumerable<BlobLocator> references)
			{
				Data = data;
				References = new List<BlobLocator>(references);
			}
		}

		static async ValueTask<Blob> ReadBlobAsync(IStorageClient store, BlobLocator locator)
		{
			using (BlobData blobData = await store.ReadBlobAsync(locator))
			{
				byte[] data = blobData.Data.ToArray();
				List<BlobLocator> locators = blobData.Refs.ConvertAll(x => x.GetLocator());
				return new Blob(data, locators);
			}
		}

		static async ValueTask<BlobLocator> WriteBlobAsync(IStorageClient store, Blob blob)
		{
			await using IStorageWriter writer = store.CreateWriter();
			blob.Data.CopyTo(writer.GetOutputBuffer(0, blob.Data.Length));

			IBlobHandle handle = await writer.WriteBlobAsync(s_blobType, blob.Data.Length, blob.References.ConvertAll(x => store.CreateBlobHandle(x)));
			await handle.FlushAsync();

			return handle.GetLocator();
		}

		[TestMethod]
		public async Task LeafTestAsync()
		{
			using IStorageClient store = CreateStorageClient();

			byte[] input = CreateTestData(256, 0);

			Blob inputBlob = new Blob(input, Array.Empty<BlobLocator>());
			BlobLocator locator = await WriteBlobAsync(store, inputBlob);
			Blob outputBlob = await ReadBlobAsync(store, locator);

			Assert.IsTrue(outputBlob.Data.Span.SequenceEqual(input));
			Assert.AreEqual(0, outputBlob.References.Count);
		}

		[TestMethod]
		public async Task ReferenceTestAsync()
		{
			using IStorageClient store = CreateStorageClient();

			byte[] input1 = CreateTestData(256, 1);
			BlobLocator locator1 = await WriteBlobAsync(store, new Blob(input1, Array.Empty<BlobLocator>()));
			Blob blob1 = await ReadBlobAsync(store, locator1);
			Assert.IsTrue(blob1.Data.Span.SequenceEqual(input1));
			Assert.IsTrue(blob1.References.SequenceEqual(Array.Empty<BlobLocator>()));

			byte[] input2 = CreateTestData(256, 2);
			BlobLocator locator2 = await WriteBlobAsync(store, new Blob(input2, new BlobLocator[] { locator1 }));
			Blob blob2 = await ReadBlobAsync(store, locator2);
			Assert.IsTrue(blob2.Data.Span.SequenceEqual(input2));
			Assert.IsTrue(blob2.References.SequenceEqual(new BlobLocator[] { locator1 }));

			byte[] input3 = CreateTestData(256, 3);
			BlobLocator locator3 = await WriteBlobAsync(store, new Blob(input3, new BlobLocator[] { locator1, locator2, locator1 }));
			Blob blob3 = await ReadBlobAsync(store, locator3);
			Assert.IsTrue(blob3.Data.Span.SequenceEqual(input3));
			Assert.IsTrue(blob3.References.SequenceEqual(new BlobLocator[] { locator1, locator2, locator1 }));

			for(int idx = 0; idx < 2; idx++)
			{
				RefName refName = new RefName("hello");
				await store.WriteRefTargetAsync(refName, store.CreateBlobHandle(locator3));
				IBlobHandle refTarget = await store.ReadRefTargetAsync(refName);
				Assert.AreEqual(locator3, refTarget.GetLocator());
			}
		}

		[TestMethod]
		public async Task RefExpiryTestAsync()
		{
			using IStorageClient store = CreateStorageClient();

			Blob blob1 = new Blob(new byte[] { 1, 2, 3 }, Array.Empty<BlobLocator>());
			BlobLocator target = await WriteBlobAsync(store, blob1);

			await store.WriteRefTargetAsync("test-ref-1", store.CreateBlobHandle(target));
			await store.WriteRefTargetAsync("test-ref-2", store.CreateBlobHandle(target), new RefOptions { Lifetime = TimeSpan.FromMinutes(30.0), Extend = true });
			await store.WriteRefTargetAsync("test-ref-3", store.CreateBlobHandle(target), new RefOptions { Lifetime = TimeSpan.FromMinutes(30.0), Extend = false });

			Assert.AreEqual(target, await TryReadRefTargetAsync(store, "test-ref-1"));
			Assert.AreEqual(target, await TryReadRefTargetAsync(store, "test-ref-2"));
			Assert.AreEqual(target, await TryReadRefTargetAsync(store, "test-ref-3"));

			await Clock.AdvanceAsync(TimeSpan.FromMinutes(25.0));

			Assert.AreEqual(target, await TryReadRefTargetAsync(store, "test-ref-1"));
			Assert.AreEqual(target, await TryReadRefTargetAsync(store, "test-ref-2"));
			Assert.AreEqual(target, await TryReadRefTargetAsync(store, "test-ref-3"));

			await Clock.AdvanceAsync(TimeSpan.FromMinutes(25.0));

			Assert.AreEqual(target, await TryReadRefTargetAsync(store, "test-ref-1"));
			Assert.AreEqual(target, await TryReadRefTargetAsync(store, "test-ref-2"));
			Assert.AreEqual(default, await TryReadRefTargetAsync(store, "test-ref-3"));

			await Clock.AdvanceAsync(TimeSpan.FromMinutes(35.0));

			Assert.AreEqual(target, await TryReadRefTargetAsync(store, "test-ref-1"));
			Assert.AreEqual(default, await TryReadRefTargetAsync(store, "test-ref-2"));
			Assert.AreEqual(default, await TryReadRefTargetAsync(store, "test-ref-3"));
		}

		static async Task<BlobLocator> TryReadRefTargetAsync(IStorageClient store, RefName name)
		{
			IBlobHandle? handle = await store.TryReadRefTargetAsync(name);
			if (handle == null)
			{
				return default;
			}
			return handle.GetLocator();
		}
	}
}

