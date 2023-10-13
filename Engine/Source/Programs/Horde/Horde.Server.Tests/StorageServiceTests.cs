// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Bundles;
using EpicGames.Horde.Storage.Clients;
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
			using IServerStorageClient client = storageService.CreateClient(new NamespaceId("memory"));

			List<BlobType> types = new List<BlobType>();
			types.Add(new BlobType(Guid.Parse("{11C2D886-3349-4164-946F-E9D10BD12E3D}"), 0));
			types.Add(new BlobType(Guid.Parse("{6CB3A005-26BA-4787-86D2-793ED13771CB}"), 0));

			byte[] data1 = new byte[] { 1, 2, 3 };
			IoHash hash1 = IoHash.Compute(data1);

			byte[] data2 = new byte[] { 4, 5, 6 };
			IoHash hash2 = IoHash.Compute(data2);

			List<BundleExport> exports = new List<BundleExport>();

			exports.Add(new BundleExport(0, 0, 0, data1.Length, Array.Empty<BundleExportRef>()));
			exports.Add(new BundleExport(0, 0, 0, data1.Length, Array.Empty<BundleExportRef>()));
			exports.Add(new BundleExport(0, 0, data1.Length, data2.Length, Array.Empty<BundleExportRef>()));

			BundleHeader header = new BundleHeader(types.ToArray(), Array.Empty<BlobLocator>(), exports.ToArray(), new BundlePacket[1]);
			Bundle bundle = new Bundle(header, Array.Empty<ReadOnlyMemory<byte>>());
			BlobLocator locator = await client.WriteBundleAsync(bundle).GetLocatorAsync();

			await client.AddAliasAsync("foo", client.CreateBlobHandle(new BlobLocator($"{locator}#0")));
			await client.AddAliasAsync("foo", client.CreateBlobHandle(new BlobLocator($"{locator}#1")));
			await client.AddAliasAsync("bar", client.CreateBlobHandle(new BlobLocator($"{locator}#2")));

			BlobAlias[] aliases;

			aliases = await client.FindAliasesAsync("foo");
			Assert.AreEqual(2, aliases.Length);
			Assert.AreEqual(new BlobLocator($"{locator}#0"), aliases[0].Target.GetLocator());
			Assert.AreEqual(new BlobLocator($"{locator}#1"), aliases[1].Target.GetLocator());

			aliases = await client.FindAliasesAsync("bar");
			Assert.AreEqual(1, aliases.Length);
			Assert.AreEqual(new BlobLocator($"{locator}#2"), aliases[0].Target.GetLocator());
		}
	}
}

