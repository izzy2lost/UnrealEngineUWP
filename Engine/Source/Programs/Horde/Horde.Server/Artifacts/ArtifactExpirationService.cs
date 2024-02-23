// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Artifacts;
using EpicGames.Horde.Storage;
using Horde.Server.Server;
using Horde.Server.Storage;
using Horde.Server.Utilities;
using HordeCommon;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Bson.Serialization.Options;

namespace Horde.Server.Artifacts
{
	/// <summary>
	/// Expires artifacts according to their 
	/// </summary>
	class ArtifactExpirationService : IHostedService
	{
		[SingletonDocument("artifact-expiry-times")]
		class ExpiryTimes : SingletonBase
		{
			[BsonDictionaryOptions(DictionaryRepresentation.ArrayOfDocuments)]
			public Dictionary<ArtifactType, int> TypeToDays { get; set; } = new Dictionary<ArtifactType, int>();
		}

		readonly IArtifactCollection _artifactCollection;
		readonly ISingletonDocument<ExpiryTimes> _expiryTimes;
		readonly IOptionsMonitor<GlobalConfig> _globalConfig;
		readonly StorageService _storageService;
		readonly IClock _clock;
		readonly ITicker _ticker;
		readonly ILogger _logger;

		public ArtifactExpirationService(IArtifactCollection artifactCollection, StorageService storageService, MongoService mongoService, IOptionsMonitor<GlobalConfig> globalConfig, IClock clock, ILogger<ArtifactExpirationService> logger)
		{
			_artifactCollection = artifactCollection;
			_storageService = storageService;
			_expiryTimes = new SingletonDocument<ExpiryTimes>(mongoService);
			_globalConfig = globalConfig;
			_clock = clock;
			_ticker = clock.AddSharedTicker<ArtifactExpirationService>(TimeSpan.FromHours(1.0), TickAsync, logger);
			_logger = logger;
		}

		/// <inheritdoc/>
		public async Task StartAsync(CancellationToken cancellationToken)
		{
			await _ticker.StartAsync();
		}

		/// <inheritdoc/>
		public async Task StopAsync(CancellationToken cancellationToken)
		{
			await _ticker.StopAsync();
		}

		async ValueTask TickAsync(CancellationToken cancellationToken)
		{
			_logger.LogInformation("Checking for expired artifacts...");
			Stopwatch timer = Stopwatch.StartNew();

			GlobalConfig globalConfig = _globalConfig.CurrentValue;

			// Get the new expiry time map
			Dictionary<ArtifactType, int> nextTypeToTime = new Dictionary<ArtifactType, int>();
			foreach (ArtifactTypeConfig artifactTypeConfig in globalConfig.ArtifactTypes)
			{
				nextTypeToTime[artifactTypeConfig.Type] = artifactTypeConfig.KeepDays ?? 0;
			}

			// Get the last expiry time map
			ExpiryTimes prevExpiryTimes = await _expiryTimes.GetAsync(cancellationToken);

			// Update any artifacts with a new expiry time
			foreach ((ArtifactType type, int time) in nextTypeToTime)
			{
				int prevTime;
				if (!prevExpiryTimes.TypeToDays.TryGetValue(type, out prevTime) || time != prevTime)
				{
					await UpdateExpiryTimesAsync(type, time, cancellationToken);
					prevExpiryTimes = await _expiryTimes.UpdateAsync(x => x.TypeToDays[type] = time, cancellationToken);
				}
			}

			// Expire any artifacts which have past their expiry time
			DateTime utcNow = _clock.UtcNow;
			await foreach (IEnumerable<IArtifact> artifacts in _artifactCollection.FindExpiredAsync(utcNow, cancellationToken))
			{
				foreach (IGrouping<NamespaceId, IArtifact> group in artifacts.GroupBy(x => x.NamespaceId))
				{
					using IStorageClient storageClient = _storageService.CreateClient(group.Key);
					foreach (IArtifact artifact in group)
					{
						_logger.LogDebug("Expiring artifact {ArtifactId}, ref {RefName}", artifact.Id, artifact.RefName);
						await storageClient.DeleteRefAsync(artifact.RefName, cancellationToken);
					}
				}
				await _artifactCollection.DeleteAsync(artifacts.Select(x => x.Id), cancellationToken);
			}

			_logger.LogInformation("Finished expiring artifacts in {TimeSecs}s.", (long)timer.Elapsed.TotalSeconds);
		}

		async Task UpdateExpiryTimesAsync(ArtifactType type, int time, CancellationToken cancellationToken)
		{
			_logger.LogInformation("Updating expiry times for {Type} artifacts -> {Time}", type, time);
			await foreach (IArtifact artifact in _artifactCollection.FindAsync(type: type, cancellationToken: cancellationToken))
			{
				IArtifact? original = artifact;
				while (original != null)
				{
					DateTime? newExpiryTime;
					if (time == 0)
					{
						newExpiryTime = null;
					}
					else
					{
						newExpiryTime = original.CreatedAtUtc + TimeSpan.FromDays(time);
					}

					if (original.ExpireAtUtc == newExpiryTime)
					{
						break;
					}

					IArtifact? updated = await _artifactCollection.TryUpdateAsync(original, newExpiryTime, cancellationToken);
					if (updated != null)
					{
						_logger.LogInformation("Updated expiry time for {ArtifactId} from {OldTime} -> {NewTime}", original.Id, original.ExpireAtUtc, updated.ExpireAtUtc);
						break;
					}

					original = await _artifactCollection.GetAsync(artifact.Id, cancellationToken);
				}
			}
			_logger.LogInformation("Finished updating expiry times for {Type}", type);
		}
	}
}
