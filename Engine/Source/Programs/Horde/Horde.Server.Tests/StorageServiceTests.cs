// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Horde.Server.Storage;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests
{
	[TestClass]
	public class StorageServiceTests : TestSetup
	{
		[TestMethod]
		public async Task BlobCollectionTestAsync()
		{
			StorageService storageService = ServiceProvider.GetRequiredService<StorageService>();
			using IStorageClient client = storageService.CreateClient(new NamespaceId("memory"));

			BlobType type1 = new BlobType(Guid.Parse("{11C2D886-4164-3349-D1E9-6F943D2ED10B}"), 0);
			byte[] data1 = new byte[] { 1, 2, 3 };

			BlobType type2 = new BlobType(Guid.Parse("{6CB3A005-4787-26BA-3E79-D286CB7137D1}"), 0);
			byte[] data2 = new byte[] { 4, 5, 6 };

			IBlobHandle handle1a;
			IBlobHandle handle1b;
			IBlobHandle handle2;
			await using (IStorageWriter writer = client.CreateWriter())
			{
				data1.CopyTo(writer.GetOutputBuffer(0, data1.Length));
				handle1a = await writer.WriteBlobAsync(type1, data1.Length, Array.Empty<IBlobHandle>(), new AliasInfo[] { new AliasInfo("foo", 2) });

				data1.CopyTo(writer.GetOutputBuffer(0, data1.Length));
				handle1b = await writer.WriteBlobAsync(type1, data1.Length, Array.Empty<IBlobHandle>(), new AliasInfo[] { new AliasInfo("foo", 1) });

				data2.CopyTo(writer.GetOutputBuffer(0, data2.Length));
				handle2 = await writer.WriteBlobAsync(type2, data2.Length, Array.Empty<IBlobHandle>(), new AliasInfo[] { new AliasInfo("bar") });
			}

			BlobAlias[] aliases;

			aliases = await client.FindAliasesAsync("foo");
			Assert.AreEqual(2, aliases.Length);
			Assert.AreEqual(handle1a.GetLocator(), aliases[0].Target.GetLocator());
			Assert.AreEqual(handle1b.GetLocator(), aliases[1].Target.GetLocator());

			aliases = await client.FindAliasesAsync("bar");
			Assert.AreEqual(1, aliases.Length);
			Assert.AreEqual(handle2.GetLocator(), aliases[0].Target.GetLocator());
		}
	}
}

