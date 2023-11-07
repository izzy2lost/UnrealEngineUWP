// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace EpicGames.Horde.Tests
{
	[TestClass]
	public class BlobDataTests
	{
		class DummyHandle : IBlobHandle
		{
			readonly BlobLocator _locator;

			public IBlobHandle? Outer => null;

			public DummyHandle(string locator) => _locator = new BlobLocator(new Utf8String(locator));
			public ValueTask<BlobData> ReadAsync(CancellationToken cancellationToken = default) => throw new NotImplementedException();

			public bool TryAppendIdentifier(Utf8StringBuilder builder)
			{
				builder.Append(_locator.Path);
				return true;
			}

			public override bool Equals(object? obj) => obj is DummyHandle other && _locator == other._locator;
			public override int GetHashCode() => _locator.GetHashCode();
		}

		[TestMethod]
		public void HeaderSerialization()
		{
			List<IBlobHandle> refs = new List<IBlobHandle>();
			refs.Add(new DummyHandle("hello"));
			refs.Add(new DummyHandle("world"));

			using BlobData blobData = new BlobData(new BlobType(Guid.NewGuid(), 4), new byte[] { 1, 2, 3 }, refs);

			byte[] data = EncodedBlobData.Create(blobData);
			EncodedBlobData packet = new EncodedBlobData(data);
			Assert.AreEqual(49, data.Length);

			Assert.AreEqual(blobData.Type.Guid, packet.Type.Guid);
			Assert.AreEqual(blobData.Type.Version, packet.Type.Version);
			Assert.AreEqual(blobData.Refs.Count, packet.Refs.Count);

			for (int idx = 0; idx < packet.Refs.Count; idx++)
			{
				string locator = blobData.Refs[idx].GetLocator().ToString();
				string encoded = packet.Refs[idx].ToString();
				Assert.AreEqual(locator, encoded);
			}
		}
	}
}
