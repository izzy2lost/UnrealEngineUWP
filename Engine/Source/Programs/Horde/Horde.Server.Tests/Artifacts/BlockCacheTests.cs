// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Linq;
using System.Security.Cryptography;
using EpicGames.Core;
using Horde.Server.Artifacts;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests.Artifacts
{
	[TestClass]
	public class BlockCacheTests
	{
		[TestMethod]
		public void BasicTest()
		{
			using BlockCache blockCache = BlockCache.CreateInMemory(1, 4096, 4096);
			blockCache.Add("hello", new byte[] { 1, 2, 3 });

			IBlockCacheValue? value = blockCache.Get("hello");
			Assert.IsNotNull(value);

			Assert.IsTrue(value.Data.ToArray().SequenceEqual(new byte[] { 1, 2, 3 }));
		}

		[TestMethod]
		public void LargeBlockTest()
		{
			byte[] data = RandomNumberGenerator.GetBytes(100000);

			using BlockCache blockCache = BlockCache.CreateInMemory(1);
			blockCache.Add("hello", data);

			IBlockCacheValue? value = blockCache.Get("hello");
			Assert.IsNotNull(value);

			Assert.IsTrue(value.Data.ToArray().SequenceEqual(data));
		}

		[TestMethod]
		public void MultiBlockTest()
		{
			string[] keys = new string[1000];
			byte[][] values = new byte[keys.Length][];

			using BlockCache blockCache = BlockCache.CreateInMemory(1);

			Random random = new Random(0);
			for (int idx = 0; idx < keys.Length; idx++)
			{
				keys[idx] = $"key{idx}";

				int length = random.Next(4000) + 1;

				byte[] buffer = new byte[length];
				random.NextBytes(buffer);

				values[idx] = buffer;
				blockCache.Add(keys[idx], values[idx]);
			}

			for (int idx = 0; idx < 2000; idx++)
			{
				int keyIdx = random.Next(keys.Length);

				IBlockCacheValue? value = blockCache.Get(keys[keyIdx]);
				Assert.IsNotNull(value);

				Assert.IsTrue(value.Data.ToArray().SequenceEqual(values[keyIdx]));
			}
		}
	}
}
