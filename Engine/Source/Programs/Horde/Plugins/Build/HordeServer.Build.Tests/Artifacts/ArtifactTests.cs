// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Acls;
using EpicGames.Horde.Artifacts;
using EpicGames.Horde.Commits;
using EpicGames.Horde.Streams;
using HordeCommon;
using HordeServer.Artifacts;
using HordeServer.Projects;
using HordeServer.Server;
using HordeServer.Streams;
using Microsoft.Extensions.DependencyInjection;

namespace HordeServer.Tests.Artifacts
{
	[TestClass]
	public class ArtifactTests : BuildTestSetup
	{
		public ArtifactTests()
		{
			void SetupFixture(GlobalConfig globalConfig)
			{
				ProjectConfig projectConfig = new ProjectConfig();
				projectConfig.Streams.Add(new StreamConfig { Id = new StreamId("foo") });
				projectConfig.Streams.Add(new StreamConfig { Id = new StreamId("bar") });

				BuildConfig buildConfig = globalConfig.Plugins.GetBuildConfig();
				buildConfig.Projects.Add(projectConfig);
			}

			UpdateConfig(SetupFixture);
		}

		[TestMethod]
		public async Task CreateArtifactAsync()
		{
			StreamId streamId = new StreamId("foo");

			IArtifactCollection artifactCollection = ServiceProvider.GetRequiredService<IArtifactCollection>();
			IArtifact artifact = await artifactCollection.AddAsync(new ArtifactName("default"), ArtifactType.StepOutput, null, streamId, CommitIdWithOrder.FromPerforceChange(1), new string[] { "test1", "test2" }, Array.Empty<string>(), AclScopeName.Root);

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

			ArtifactType type = new ArtifactType("my-artifact");
			UpdateConfig(config => config.Plugins.GetBuildConfig().ArtifactTypes.Add(new ArtifactTypeConfig { Type = type, KeepDays = 1 }));

			await expirationService.StartAsync(CancellationToken.None);

			StreamId streamId = new StreamId("foo");

			IArtifactCollection artifactCollection = ServiceProvider.GetRequiredService<IArtifactCollection>();
			IArtifact artifact = await artifactCollection.AddAsync(new ArtifactName("default"), type, null, streamId, CommitIdWithOrder.FromPerforceChange(1), new string[] { "test1", "test2" }, Array.Empty<string>(), AclScopeName.Root);

			{
				List<IArtifact> artifacts = await artifactCollection.FindAsync(streamId, keys: new[] { "test1" }).ToListAsync();
				Assert.AreEqual(1, artifacts.Count);
				Assert.AreEqual(artifact.Id, artifacts[0].Id);
			}

			await clock.AdvanceAsync(TimeSpan.FromDays(2.0));

			{
				List<IArtifact> artifacts = await artifactCollection.FindAsync(streamId, keys: new[] { "test1" }).ToListAsync();
				Assert.AreEqual(0, artifacts.Count);
			}
		}

		[TestMethod]
		public async Task UpdateExpiryTimesAsync()
		{
			DateTime startTime = Clock.UtcNow;

			ArtifactType type = new ArtifactType("my-artifact");
			UpdateConfig(config => config.Plugins.GetBuildConfig().ArtifactTypes.Add(new ArtifactTypeConfig { Type = type, KeepDays = 1 }));

			ArtifactExpirationService expirationService = ServiceProvider.GetRequiredService<ArtifactExpirationService>();

			await expirationService.StartAsync(CancellationToken.None);

			StreamId streamId = new StreamId("foo");

			IArtifactCollection artifactCollection = ServiceProvider.GetRequiredService<IArtifactCollection>();
			IArtifact artifact = await artifactCollection.AddAsync(new ArtifactName("default"), type, null, streamId, CommitIdWithOrder.FromPerforceChange(1), new string[] { "test1", "test2" }, Array.Empty<string>(), AclScopeName.Root);

			{
				List<IArtifact> artifacts = await artifactCollection.FindAsync(streamId, keys: new[] { "test1" }).ToListAsync();
				Assert.AreEqual(1, artifacts.Count);
				Assert.AreEqual(artifact.Id, artifacts[0].Id);
			}

			UpdateConfig(config => config.Plugins.GetBuildConfig().ArtifactTypes = new List<ArtifactTypeConfig> { new ArtifactTypeConfig { Type = type, KeepDays = 4 } });
			await Clock.AdvanceAsync(TimeSpan.FromDays(2.0));

			{
				List<IArtifact> artifacts = await artifactCollection.FindAsync(streamId, keys: new[] { "test1" }).ToListAsync();
				Assert.AreEqual(1, artifacts.Count);
			}

			UpdateConfig(config => config.Plugins.GetBuildConfig().ArtifactTypes = new List<ArtifactTypeConfig> { new ArtifactTypeConfig { Type = type, KeepDays = 1 } });
			await Clock.AdvanceAsync(TimeSpan.FromDays(1.0));

			{
				List<IArtifact> artifacts = await artifactCollection.FindAsync(streamId, keys: new[] { "test1" }).ToListAsync();
				Assert.AreEqual(0, artifacts.Count);
			}
		}

		[TestMethod]
		public async Task ExpireCountAsync()
		{
			DateTime startTime = Clock.UtcNow;

			ArtifactType type = new ArtifactType("my-artifact");
			UpdateConfig(config => config.Plugins.GetBuildConfig().ArtifactTypes.Add(new ArtifactTypeConfig { Type = type, KeepCount = 4 }));

			ArtifactExpirationService expirationService = ServiceProvider.GetRequiredService<ArtifactExpirationService>();

			await expirationService.StartAsync(CancellationToken.None);

			StreamId fooStreamId = new StreamId("foo");
			StreamId barStreamId = new StreamId("bar");

			IArtifactCollection artifactCollection = ServiceProvider.GetRequiredService<IArtifactCollection>();
			for (int idx = 0; idx < 10; idx++)
			{
				await artifactCollection.AddAsync(new ArtifactName($"default-{idx}"), type, null, fooStreamId, CommitIdWithOrder.FromPerforceChange(1), new string[] { "test1", "test2" }, Array.Empty<string>(), AclScopeName.Root);
			}
			for (int idx = 0; idx < 10; idx++)
			{
				await artifactCollection.AddAsync(new ArtifactName($"default-{idx}"), type, null, barStreamId, CommitIdWithOrder.FromPerforceChange(1), new string[] { "test1", "test2" }, Array.Empty<string>(), AclScopeName.Root);
			}

			await Clock.AdvanceAsync(TimeSpan.FromDays(2.0));

			{
				List<IArtifact> artifacts = await artifactCollection.FindAsync(fooStreamId, keys: new[] { "test1" }).ToListAsync();
				Assert.AreEqual(4, artifacts.Count);

				for (int idx = 0; idx < 4; idx++)
				{
					Assert.AreEqual(new ArtifactName($"default-{9 - idx}"), artifacts[idx].Name);
				}
			}
			{
				List<IArtifact> artifacts = await artifactCollection.FindAsync(barStreamId, keys: new[] { "test1" }).ToListAsync();
				Assert.AreEqual(4, artifacts.Count);

				for (int idx = 0; idx < 4; idx++)
				{
					Assert.AreEqual(new ArtifactName($"default-{9 - idx}"), artifacts[idx].Name);
				}
			}

			UpdateConfig(config => config.Plugins.GetBuildConfig().ArtifactTypes = new List<ArtifactTypeConfig> { new ArtifactTypeConfig { Type = type, KeepDays = 1 } });
			await Clock.AdvanceAsync(TimeSpan.FromDays(2.0));

			{
				List<IArtifact> artifacts = await artifactCollection.FindAsync(fooStreamId, keys: new[] { "test1" }).ToListAsync();
				Assert.AreEqual(0, artifacts.Count);
			}
			{
				List<IArtifact> artifacts = await artifactCollection.FindAsync(barStreamId, keys: new[] { "test1" }).ToListAsync();
				Assert.AreEqual(0, artifacts.Count);
			}
		}
	}
}
