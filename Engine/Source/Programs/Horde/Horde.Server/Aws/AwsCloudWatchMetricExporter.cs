// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Http;
using System.Threading;
using System.Threading.Tasks;
using Amazon.CloudWatch;
using Amazon.CloudWatch.Model;
using Horde.Server.Compute;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Aws;

/// <summary>
/// Exports metric inside Horde server to AWS CloudWatch
/// </summary>
public class AwsCloudWatchMetricExporter : IHostedService
{
	private readonly ComputeService _computeService;
	private readonly IAmazonCloudWatch _cloudWatch;
	private readonly ILogger<AwsCloudWatchMetricExporter> _logger;
	
	/// <summary>
	/// Constructor
	/// </summary>
	/// <param name="computeService"></param>
	/// <param name="cloudWatch"></param>
	/// <param name="logger"></param>
	public AwsCloudWatchMetricExporter(ComputeService computeService, IAmazonCloudWatch cloudWatch, ILogger<AwsCloudWatchMetricExporter> logger)
	{
		_computeService = computeService;
		_cloudWatch = cloudWatch;
		_logger = logger;
	}

	private async void OnResourceNeedsUpdatedAsync(string clusterId, string poolId, string resourceName, int totalValue)
	{
		// Method declared as "async void" - make sure to catch exceptions inside
		try
		{
			DateTime utcNow = DateTime.UtcNow;
			List<Dimension> dimensions = new()
			{
				new() { Name = "ComputeCluster", Value = clusterId },
				new() { Name = "Pool", Value = poolId },
				new() { Name = "Resource", Value = resourceName }
			};
			List<MetricDatum> metricDatums = new ()
			{
				new ()
				{
					MetricName = "ResourceNeed",
					Dimensions = dimensions,
					Unit = StandardUnit.Count,
					Value = totalValue,
					TimestampUtc = utcNow
				}
			};
		
			PutMetricDataRequest request = new() { Namespace = "Horde", MetricData = metricDatums };
			PutMetricDataResponse response = await _cloudWatch.PutMetricDataAsync(request);
			
			if (response.HttpStatusCode != HttpStatusCode.OK)
			{
				throw new HttpRequestException($"PutMetricData failed. Status code {response.HttpStatusCode}");
			}
		}
		catch (Exception e)
		{
			_logger.LogError(e, "Error while updating resource needs with AWS CloudWatch");
		}
	}

	/// <inheritdoc/>
	public Task StartAsync(CancellationToken cancellationToken)
	{
		_computeService.OnResourceNeedsUpdated += OnResourceNeedsUpdatedAsync;
		return Task.CompletedTask;
	}

	/// <inheritdoc/>
	public Task StopAsync(CancellationToken cancellationToken)
	{
		_computeService.OnResourceNeedsUpdated -= OnResourceNeedsUpdatedAsync;
		return Task.CompletedTask;
	}
}

