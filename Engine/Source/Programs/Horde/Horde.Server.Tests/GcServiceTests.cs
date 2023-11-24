// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using EpicGames.Horde.Storage;
using Horde.Server.Storage;
using System.Threading;
using Horde.Server.Server;
using Microsoft.Extensions.DependencyInjection;
using EpicGames.Core;

namespace Horde.Server.Tests
{
	[TestClass]
	public sealed class GcServiceTests : TestSetup
	{
		[TestMethod]
		public async Task CreateBasicTreeAsync()
		{
			await StorageService.StartAsync(CancellationToken.None);

			GlobalConfig globalConfig = new GlobalConfig();
			globalConfig.Storage.Backends.Add(new BackendConfig { Id = new BackendId("default-backend"), Type = StorageBackendType.Memory });
			globalConfig.Storage.Namespaces.Add(new NamespaceConfig { Id = new NamespaceId("default"), Backend = new BackendId("default-backend"), GcDelayHrs = 0.0 });
			SetConfig(globalConfig);

			using IStorageClient store = StorageService.CreateClient(new NamespaceId("default"));

			Random random = new Random(0);
			BlobLocator[] blobs = await CreateTestDataAsync(store, 30, 50, 30, 5, random);

			HashSet<BlobLocator> roots = new HashSet<BlobLocator>();
			for (int idx = 0; idx < 10; idx++)
			{
				int blobIdx = (int)(random.NextDouble() * blobs.Length);
				if (roots.Add(blobs[blobIdx]))
				{
					IBlobHandle handle = store.CreateBlobHandle(blobs[blobIdx]);
					await store.WriteRefTargetAsync(new RefName($"ref-{idx}"), handle);
				}
			}

			HashSet<BlobLocator> nodes = await FindNodesAsync(store, roots);

			await Clock.AdvanceAsync(TimeSpan.FromDays(1.0));

			IStorageBackend backend = ServiceProvider.GetRequiredService<IStorageBackendProvider>().CreateBackend(globalConfig.Storage.Backends[0]);

			string[] remaining = await backend.EnumerateAsync().ToArrayAsync();
			Assert.AreEqual(nodes.Count, remaining.Length);

			HashSet<string> nodePaths = new HashSet<string>(nodes.Select(x => x.BaseLocator.ToString()), StringComparer.Ordinal);
			Assert.IsTrue(remaining.All(x => nodePaths.Contains(x)));
		}

		static async Task<HashSet<BlobLocator>> FindNodesAsync(IStorageClient store, IEnumerable<BlobLocator> roots)
		{
			HashSet<BlobLocator> nodes = new HashSet<BlobLocator>();
			await FindNodesAsync(store, roots, nodes);
			return nodes;
		}

		static async Task FindNodesAsync(IStorageClient store, IEnumerable<BlobLocator> roots, HashSet<BlobLocator> nodes)
		{
			foreach (BlobLocator root in roots)
			{
				if (nodes.Add(root))
				{
					IBlobHandle handle = store.CreateBlobHandle(root);
					IReadOnlyList<IBlobHandle> refs = await handle.ReadImportsAsync();
					await FindNodesAsync(store, refs.ConvertAll(x => x.GetLocator()), nodes);
				}
			}
		}

		static async ValueTask<BlobLocator[]> CreateTestDataAsync(IStorageClient store, int numRoots, int numInterior, int numLeaves, int avgChildren, Random random)
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

			BlobType blobType = new BlobType(Guid.Parse("{AFDF76A7-4DEE-5333-F5B5-37B8451251CA}"), 0);

			BlobLocator[] locators = new BlobLocator[children.Length];
			for (int idx = numNodes - 1; idx >= 0; idx--)
			{
				IBlobHandle handle;
				await using (IStorageWriter writer = store.CreateWriter("gctest"))
				{
					List<IBlobHandle> imports = children[idx].ConvertAll(x => store.CreateBlobHandle(locators[x]));
					handle = await writer.WriteBlobAsync(blobType, 0, imports);
				}
				locators[idx] = handle.GetLocator();
			}

			return locators;
		}
	}
}
