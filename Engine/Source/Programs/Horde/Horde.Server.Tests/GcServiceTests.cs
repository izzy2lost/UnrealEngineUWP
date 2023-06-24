// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using EpicGames.Horde.Storage;
using Horde.Server.Storage;
using System.Threading;
using Horde.Server.Server;
using Horde.Server.Utilities;

namespace Horde.Server.Tests
{
	[TestClass]
	public sealed class GcServiceTests : TestSetup
	{
		void SetupNamespace()
		{
			GlobalConfig globalConfig = new GlobalConfig();
			globalConfig.Storage.Backends.Add(new BackendConfig { Id = new BackendId("default-backend"), Type = StorageBackendType.Memory });
			globalConfig.Storage.Namespaces.Add(new NamespaceConfig { Id = new NamespaceId("default"), Backend = new BackendId("default-backend"), GcDelayHrs = 0.0 });
			SetConfig(globalConfig);
		}

		[TestMethod]
		public async Task CreateBasicTree()
		{
			await StorageService.StartAsync(CancellationToken.None);

			SetupNamespace();
			StorageClient store = await StorageService.GetClientAsync(new NamespaceId("default"), CancellationToken.None);

			Random random = new Random(0);
			BundleLocator[] blobs = await CreateTestDataAsync(store, 30, 50, 30, 5, random);

			HashSet<BundleLocator> roots = new HashSet<BundleLocator>();
			for (int idx = 0; idx < 10; idx++)
			{
				int blobIdx = (int)(random.NextDouble() * blobs.Length);
				if (roots.Add(blobs[blobIdx]))
				{
					await store.WriteRefTargetAsync(new RefName($"ref-{idx}"), new BundleNodeLocator(IoHash.Zero, blobs[blobIdx], 0));
				}
			}

			HashSet<BundleLocator> nodes = await FindNodes(store, roots);

			await Clock.AdvanceAsync(TimeSpan.FromDays(1.0));

			BundleLocator[] remaining = await store.Backend.EnumerateAsync().Select(x => new BundleLocator(StorageBackend.GetBlobPathFromFileName(x))).ToArrayAsync();
			Assert.AreEqual(nodes.Count, remaining.Length);
			Assert.IsTrue(remaining.All(x => nodes.Contains(x)));
		}

		async Task<HashSet<BundleLocator>> FindNodes(BundleStorageClient store, IEnumerable<BundleLocator> roots)
		{
			HashSet<BundleLocator> nodes = new HashSet<BundleLocator>();
			await FindNodes(store, roots, nodes);
			return nodes;
		}

		async Task FindNodes(BundleStorageClient store, IEnumerable<BundleLocator> roots, HashSet<BundleLocator> nodes)
		{
			foreach (BundleLocator root in roots)
			{
				if (nodes.Add(root))
				{
					Bundle bundle = await store.ReadBundleAsync(root);
					await FindNodes(store, bundle.Header.Imports, nodes);
				}
			}
		}

		static async ValueTask<BundleLocator[]> CreateTestDataAsync(BundleStorageClient store, int numRoots, int numInterior, int numLeaves, int avgChildren, Random random)
		{
			int firstRoot = 0;
			int firstInterior = firstRoot + numRoots;
			int firstLeaf = firstInterior + numInterior;
			int numNodes = firstLeaf + numLeaves;

			List<int>[] children = new List<int>[numNodes];
			for (int idx = 0; idx < numNodes; idx++)
			{
				children[idx] = new List<int>();
			}

			double maxParents = ((numRoots + numInterior) * avgChildren) / (numInterior + numLeaves);
			for (int idx = numRoots; idx < numNodes; idx++)
			{
				int numParents = 1 + (int)(random.NextDouble() * maxParents);
				for (; numParents > 0; numParents--)
				{
					int parentIdx = Math.Min((int)(random.NextDouble() * Math.Min(idx, numRoots + numInterior)), idx - 1);
					children[parentIdx].Add(idx);
				}
			}

			BundleLocator[] locators = new BundleLocator[children.Length];
			for (int idx = numNodes - 1; idx >= 0; idx--)
			{
				List<BlobType> types = new List<BlobType> { new BlobType(Guid.Parse("{AFDF76A7-5333-4DEE-B837-B5F5CA511245}"), 0) };
				List<BundleLocator> imports = children[idx].ConvertAll(x => locators[x]);
				BundleHeader header = BundleHeader.Create(types, imports, Array.Empty<BundleExport>(), Array.Empty<BundlePacket>());
				Bundle bundle = new Bundle(header, Array.Empty<ReadOnlyMemory<byte>>());
				locators[idx] = await store.WriteBundleAsync(bundle, prefix: "gctest");
			}

			return locators;
		}
	}
}
