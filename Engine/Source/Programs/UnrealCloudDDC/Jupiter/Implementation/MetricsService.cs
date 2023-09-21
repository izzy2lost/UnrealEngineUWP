// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.Metrics;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;
using Jupiter.Common;
using Jupiter.Utils;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Jupiter.Implementation
{
	public class MetricsState
	{
		public Task? CalculateMetricsTask { get; set; } = null;
	}

	public class MetricsServiceSettings
	{
		/// <summary>
		/// Set to enable calulcation of metrics in the background. Adds load to database so only enable these if you intend to use it.
		/// </summary>
		public bool Enabled { get; set; } = false;

		public TimeSpan PollFrequency { get; set; } = TimeSpan.FromHours(24);
	}

	public class MetricsService : PollingService<MetricsState>
	{
		private readonly IOptionsMonitor<MetricsServiceSettings> _settings;
		private readonly IReferencesStore _referencesStore;
		private volatile bool _alreadyPolling;

		private readonly ILogger _logger;
		private readonly IServiceProvider _provider;

		public MetricsService(IOptionsMonitor<MetricsServiceSettings> settings, IReferencesStore referencesStore, ILogger<MetricsService> logger, IServiceProvider provider) : base(serviceName: nameof(MetricsService), settings.CurrentValue.PollFrequency, new MetricsState(), logger, startAtRandomTime: false)
		{
			_settings = settings;
			_referencesStore = referencesStore;
			_logger = logger;
			_provider = provider;
		}

		protected override bool ShouldStartPolling()
		{
			return _settings.CurrentValue.Enabled;
		}

		public override async Task<bool> OnPollAsync(MetricsState state, CancellationToken cancellationToken)
		{
			if (_alreadyPolling)
			{
				return false;
			}

			_alreadyPolling = true;
			try
			{
				if (!state.CalculateMetricsTask?.IsCompleted ?? false)
				{
					return false;
				}

				if (state.CalculateMetricsTask != null)
				{
					await state.CalculateMetricsTask;
				}
				state.CalculateMetricsTask = DoCalculateMetricsAsync(state, cancellationToken);

				return true;

			}
			finally
			{
				_alreadyPolling = false;
			}
		}

		private async Task DoCalculateMetricsAsync(MetricsState _, CancellationToken cancellationToken)
		{
			MetricsCalculator calculator = ActivatorUtilities.CreateInstance<MetricsCalculator>(_provider);
			_logger.LogInformation("Attempting to calculate metrics. ");
			try
			{
				await foreach (NamespaceId ns in _referencesStore.GetNamespacesAsync().WithCancellation(cancellationToken))
				{
					await foreach (BucketId bucket in _referencesStore.GetBuckets(ns).WithCancellation(cancellationToken))
					{
						DateTime start = DateTime.UtcNow;
						
						await calculator.CalculateStatsForBucketAsync(ns, bucket);

						TimeSpan duration = DateTime.UtcNow - start;
						_logger.LogInformation("Stats calculated for {Namespace} {Bucket} took {Duration}", ns, bucket, duration);
					}
				}
			}
			catch (Exception e)
			{
				_logger.LogError("Error calculating metrics. {Exception}",  e);
			}
		}
	}

	public class BucketStats
	{
		public NamespaceId Namespace { get; set; }
		public BucketId Bucket { get; set; }
		public long CountOfRefs { get; set; }
		public long CountOfBlobs { get; set; }
		public long TotalSize { get; set; }
		public double AvgSize { get; set; }
		public long LargestBlob { get; set; }
		public long SmallestBlobFound { get; set; }
	}

	public class MetricsCalculator
	{
		private readonly IReferencesStore _referencesStore;
		private readonly IBlobService _blobService;
		private readonly IReferenceResolver _referenceResolver;

		private readonly ILogger _logger;
		private readonly Histogram<long> _blobSizeHistogram;
		private readonly Gauge<double> _blobSizeAvgGauge;
		private readonly Gauge<long> _blobSizeMinGauge;
		private readonly Gauge<long> _blobSizeMaxGauge;
		private readonly Gauge<long> _refsInBucketGauge;
		private readonly Gauge<long> _blobSizeCountGauge;

		public MetricsCalculator(IReferencesStore referencesStore, IBlobService blobService, IReferenceResolver referenceResolver, Meter meter, ILogger<MetricsService> logger)
		{
			_referencesStore = referencesStore;
			_blobService = blobService;
			_referenceResolver = referenceResolver;
			_logger = logger;

			_blobSizeHistogram = meter.CreateHistogram<long>("blobstats.size");
			_blobSizeAvgGauge = meter.CreateGauge<double>("blobstats.bucket_size.avg");
			_blobSizeMinGauge = meter.CreateGauge<long>("blobstats.bucket_size.min");
			_blobSizeMaxGauge = meter.CreateGauge<long>("blobstats.bucket_size.max");
			_blobSizeCountGauge = meter.CreateGauge<long>("blobstats.bucket_size.count");
			_refsInBucketGauge = meter.CreateGauge<long>("blobstats.refs_in_bucket");
		}

		public async Task<BucketStats?> CalculateStatsForBucketAsync(NamespaceId ns, BucketId bucket)
		{
			KeyValuePair<string, object?>[] tags = new[] { new KeyValuePair<string, object?>("Bucket", bucket.ToString()) };

			try
			{
				long countOfRefsInBucket = 0;
				long sizeOfBlobsInBucket = 0;
				long countOfBlobsInBucket = 0;
				long largestBlobFound = 0;
				long smallestBlobFound = long.MaxValue;
				HashSet<byte[]> alreadyCountedBlobs = new HashSet<byte[]>(ByteArrayComparer.Default);
				await foreach ((RefId _, BlobId blobId) in _referencesStore.GetRecordsInBucketAsync(ns, bucket))
				{
					countOfRefsInBucket += 1;

					BlobContents blobContents = await _blobService.GetObjectAsync(ns, blobId);
					byte[] rawBlob = await blobContents.Stream.ToByteArrayAsync();
					CbObject cbObject = new CbObject(rawBlob);
					// enumerate all referenced blobs from the ref
					await foreach (BlobId blob in _referenceResolver.GetReferencedBlobs(ns, cbObject))
					{
						// check to see if we have counted this blob before
						bool added = alreadyCountedBlobs.Add(blob.HashData);
						if (added)
						{
							// new blob, lets count it
							BlobContents referencedBlob = await _blobService.GetObjectAsync(ns, blob);
							sizeOfBlobsInBucket += referencedBlob.Length;

							smallestBlobFound = Math.Min(referencedBlob.Length, smallestBlobFound);
							largestBlobFound = Math.Max(referencedBlob.Length, largestBlobFound);

							countOfBlobsInBucket += 1;

							_blobSizeHistogram.Record(referencedBlob.Length, tags);
						}
					}
				}

				double avgBlobSize = sizeOfBlobsInBucket / (double)countOfBlobsInBucket;
				_blobSizeAvgGauge.Record(avgBlobSize, tags);
				_blobSizeMinGauge.Record(smallestBlobFound, tags);
				_blobSizeMaxGauge.Record(largestBlobFound, tags);
				_blobSizeCountGauge.Record(countOfBlobsInBucket, tags);
				_refsInBucketGauge.Record(countOfRefsInBucket, tags);

				_logger.LogInformation("Stats calculated for {Namespace} {Bucket}. {CountOfRefs} {CountOfBlobs} {TotalSize} {AvgSize} {MaxSize} {MinSize}",
					ns, bucket, countOfRefsInBucket, countOfBlobsInBucket, sizeOfBlobsInBucket, avgBlobSize, largestBlobFound, smallestBlobFound);

				return new BucketStats
				{
					Namespace = ns,
					Bucket = bucket,
					CountOfRefs = countOfRefsInBucket,
					CountOfBlobs = countOfBlobsInBucket,
					TotalSize = sizeOfBlobsInBucket,
					AvgSize = avgBlobSize,
					LargestBlob = largestBlobFound,
					SmallestBlobFound = smallestBlobFound
				};

			}
			catch (Exception e)
			{
				_logger.LogError("Error calculating metrics for {Namespace} {Bucket} due to {Exception}", ns, bucket, e);
			}

			return null;
		}
	}
}
