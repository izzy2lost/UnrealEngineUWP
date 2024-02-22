// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Artifacts;
using Horde.Server.Acls;
using Horde.Server.Artifacts;
using HordeCommon;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using EpicGames.Horde.Streams;

namespace Horde.Server.Tests.Artifacts
{
	[TestClass]
	public class ArtifactTests : TestSetup
	{
		[TestMethod]
		public async Task CreateArtifactAsync()
		{
			StreamId streamId = new StreamId("foo");

			IArtifactCollection artifactCollection = ServiceProvider.GetRequiredService<IArtifactCollection>();
			IArtifact artifact = await artifactCollection.AddAsync(new ArtifactName("default"), ArtifactType.StepOutput, null, streamId, 1, new string[] { "test1", "test2" }, null, AclScopeName.Root);

			{
				List<IArtifact> artifacts = await artifactCollection.FindAsync(streamId, keys: new[] { "test1" }).ToListAsync();
				Assert.AreEqual(1, artifacts.Count);
				Assert.AreEqual(artifact.Id, artifacts[0].Id);
			}

			{
				List<IArtifact> artifacts = await artifactCollection.FindAsync(streamId, keys: new[] { "test2" }).ToListAsync();
				Assert.AreEqual(1, artifacts.Count);
				Assert.AreEqual(artifact.Id, artifacts[0].Id);
			}

			{
				List<IArtifact> artifacts = await artifactCollection.FindAsync(streamId, keys: new[] { "test3" }).ToListAsync();
				Assert.AreEqual(0, artifacts.Count);
			}
		}

		[TestMethod]
		public async Task ExpireArtifactAsync()
		{
			FakeClock clock = ServiceProvider.GetRequiredService<FakeClock>();
			ArtifactExpirationService expirationService = ServiceProvider.GetRequiredService<ArtifactExpirationService>();

			await expirationService.StartAsync(CancellationToken.None);

			StreamId streamId = new StreamId("foo");

			IArtifactCollection artifactCollection = ServiceProvider.GetRequiredService<IArtifactCollection>();
			IArtifact artifact = await artifactCollection.AddAsync(new ArtifactName("default"), ArtifactType.StepOutput, null, streamId, 1, new string[] { "test1", "test2" }, clock.UtcNow + TimeSpan.FromHours(1.0), AclScopeName.Root);

			{
				List<IArtifact> artifacts = await artifactCollection.FindAsync(streamId, keys: new[] { "test1" }).ToListAsync();
				Assert.AreEqual(1, artifacts.Count);
				Assert.AreEqual(artifact.Id, artifacts[0].Id);
			}

			await clock.AdvanceAsync(TimeSpan.FromHours(2.0));

			{
				List<IArtifact> artifacts = await artifactCollection.FindAsync(streamId, keys: new[] { "test1" }).ToListAsync();
				Assert.AreEqual(0, artifacts.Count);
			}
		}
	}
}
