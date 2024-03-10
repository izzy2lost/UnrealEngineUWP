// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage.Bundles;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace EpicGames.Horde.Tests
{
	[TestClass]
	public class BundleCacheTests
	{
		[TestMethod]
		public async Task AllocatorAsync()
		{
			await using BundleCache cache = new BundleCache(new BundleCacheOptions { MaxSize = 4096 });

			Assert.AreEqual(0, cache.CurrentSize);
			using (IMemoryOwner<byte> owner = cache.Allocator.Alloc(1024, null))
			{
				Assert.AreEqual(1024, cache.CurrentSize);
			}
			Assert.AreEqual(0, cache.CurrentSize);
			using (IMemoryOwner<byte> owner = cache.Allocator.Alloc(1024, null))
			{
				Assert.AreEqual(1024, cache.CurrentSize);
			}
			Assert.AreEqual(0, cache.CurrentSize);
		}

		class TestData : IDisposable
		{
			readonly IMemoryOwner<byte> _owner;

			public TestData(IMemoryOwner<byte> owner) => _owner = owner;
			public void Dispose() => _owner.Dispose();
		}

		[TestMethod]
		public async Task FindOrAddAsync()
		{
			await using BundleCache cache = new BundleCache(new BundleCacheOptions { MaxSize = 1024 }, ManagedHeapAllocator.Shared);

			Assert.AreEqual(0, cache.CurrentSize);

			IRefCountedHandle<TestData> result = await cache.FindOrAddAsync("test", (key, ctx) => Task.FromResult(new TestData(cache.Allocator.Alloc(1000, null))));
			Assert.AreEqual(1000, cache.CurrentSize);
			Assert.AreEqual(2, result.RefCount); // cache and result

			IRefCountedHandle<TestData>? value = cache.Find<string, TestData>("test");
			Assert.IsNotNull(value);
			Assert.AreEqual(1000, cache.CurrentSize);
			Assert.IsTrue(ReferenceEquals(result.Target, value.Target));
			Assert.AreEqual(3, result.RefCount); // cache, result, value

			value.Dispose();

			Assert.AreEqual(1000, cache.CurrentSize);
			Assert.AreEqual(2, result.RefCount); // cache and result

			result.Dispose();

			Assert.AreEqual(1000, cache.CurrentSize);

			// Check other things don't return a value
			Assert.IsNull(cache.Find<string, TestData>("other"));

			// Allocate above the 1024 byte budget and check it's released
			IMemoryOwner<byte> owner = cache.Allocator.Alloc(25, null);
			Assert.AreEqual(25, cache.CurrentSize);
			Assert.IsNull(cache.Find<string, TestData>("test"));

			// Allow allocating more than the limit if we don't have any cache values to free
			IMemoryOwner<byte> owner2 = cache.Allocator.Alloc(2000, null);
			Assert.AreEqual(2025, cache.CurrentSize);

			owner2.Dispose();
			owner.Dispose();
			Assert.AreEqual(0, cache.CurrentSize);
		}

		[TestMethod]
		public async Task TrimAsync()
		{
			await using BundleCache cache = new BundleCache(new BundleCacheOptions { MaxSize = 1024 }, ManagedHeapAllocator.Shared);

			Assert.AreEqual(0, cache.CurrentSize);

			IRefCountedHandle<TestData> result = await cache.FindOrAddAsync("test", (key, ctx) => Task.FromResult(new TestData(cache.Allocator.Alloc(1000, null))));
			Assert.AreEqual(1000, cache.CurrentSize);
			result.Dispose();

			IMemoryOwner<byte> owner = cache.Allocator.Alloc(10, null);

			Assert.AreEqual(1010, cache.CurrentSize);

			cache.Trim();

			Assert.AreEqual(10, cache.CurrentSize);

			owner.Dispose();
		}

		[TestMethod]
		public async Task TryAddSuccessAsync()
		{
			await using BundleCache cache = new BundleCache(new BundleCacheOptions { MaxSize = 1024 }, ManagedHeapAllocator.Shared);

			Assert.AreEqual(0, cache.CurrentSize);

#pragma warning disable CA2000
			Assert.IsTrue(cache.TryAdd("test", new TestData(cache.Allocator.Alloc(20, null))));
#pragma warning restore CA2000
			Assert.AreEqual(20, cache.CurrentSize);

			IRefCountedHandle<TestData> result = await cache.FindOrAddAsync("test", (key, ctx) => Task.FromResult(new TestData(cache.Allocator.Alloc(20, null))));
			Assert.AreEqual(20, cache.CurrentSize);

			cache.Trim();
			Assert.AreEqual(20, cache.CurrentSize);
			result.Dispose();
			Assert.AreEqual(20, cache.CurrentSize);
			cache.Trim();
			Assert.AreEqual(0, cache.CurrentSize);
		}

		[TestMethod]
		public async Task TryAddFailureAsync()
		{
			await using BundleCache cache = new BundleCache(new BundleCacheOptions { MaxSize = 1024 }, ManagedHeapAllocator.Shared);

			Assert.AreEqual(0, cache.CurrentSize);

			IRefCountedHandle<TestData> result = await cache.FindOrAddAsync("test", (key, ctx) => Task.FromResult(new TestData(cache.Allocator.Alloc(20, null))));
			Assert.AreEqual(20, cache.CurrentSize);

			TestData testData = new TestData(cache.Allocator.Alloc(20, null));
			Assert.IsFalse(cache.TryAdd("test", testData));
			Assert.AreEqual(40, cache.CurrentSize);
			testData.Dispose();

			result.Dispose();
			Assert.AreEqual(20, cache.CurrentSize);
			cache.Trim();
			Assert.AreEqual(0, cache.CurrentSize);
		}
	}
}
