// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Net;
using System.Net.Http;
using System.Threading;
using System.Threading.Tasks;
using Horde.Agent.Utility;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Agent.Services;

/// <summary>
/// Describes state of local EC2 instance the agent is running on
/// </summary>
public enum Ec2InstanceState
{
	/// <summary>
	/// Normal state
	/// </summary>
	InService,
	
	/// <summary>
	/// Termination is caused by a spot interruption.
	/// </summary>
	TerminatingSpot,
	
	/// <summary>
	/// Termination is caused by the auto-scaling group
	/// Can be caused by capacity re-balancing, scale-ins etc.
	/// </summary>
	TerminatingAsg
}

/// <summary>
/// Monitors the local EC2 instance lifecycle state. In particular, auto-scaling group and spot instance events.
/// </summary>
class AwsInstanceLifecycleService : BackgroundService
{
	/// <summary>
	/// Name of the HTTP client used for requests to IMDS
	/// </summary>
	public const string HttpClientName = "Horde.HttpAwsInstanceClient";

	private const string BaseUri = "http://169.254.169.254/latest/meta-data";
	private readonly ILogger<AwsInstanceLifecycleService> _logger;
	private readonly HttpClient _httpClient;
	private readonly TimeSpan _pollInterval = TimeSpan.FromSeconds(5);
	internal Func<Ec2InstanceState, bool, CancellationToken, Task> _terminationCallback;

	/// <summary>
	/// Constructor
	/// </summary>
	public AwsInstanceLifecycleService(HttpClient httpClient, IOptions<AgentSettings> settings, ILogger<AwsInstanceLifecycleService> logger)
	{
		_httpClient = httpClient;
		_httpClient.Timeout = TimeSpan.FromSeconds(2);
		_terminationCallback = HandleTermination;
		_logger = logger;
	}

	private async Task<Ec2InstanceState> GetStateAsync(CancellationToken cancellationToken)
	{
		HttpResponseMessage spotRes = await _httpClient.GetAsync(new Uri(BaseUri + "/spot/instance-action"), cancellationToken);
		if (spotRes.StatusCode == HttpStatusCode.OK)
		{
			return Ec2InstanceState.TerminatingSpot;
		}

		HttpResponseMessage asgRes = await _httpClient.GetAsync(new Uri(BaseUri + "/autoscaling/target-lifecycle-state"), cancellationToken);
		if (asgRes.StatusCode == HttpStatusCode.OK)
		{
			string state = await asgRes.Content.ReadAsStringAsync(cancellationToken);
			if (state == "Terminated")
			{
				return Ec2InstanceState.TerminatingAsg;
			}
		}

		return Ec2InstanceState.InService;
	}
	
	private async Task<bool> IsSpotInstanceAsync(CancellationToken cancellationToken)
	{
		HttpResponseMessage res = await _httpClient.GetAsync(new Uri(BaseUri + "/instance-life-cycle"), cancellationToken);
		if (res.StatusCode == HttpStatusCode.OK)
		{
			string state = await res.Content.ReadAsStringAsync(cancellationToken);
			return state == "spot";
		}

		return false;
	}
	
	internal async Task MonitorInstanceLifecycle(CancellationToken cancellationToken)
	{
		_logger.LogInformation("Monitoring EC2 instance lifecycle state...");
		while (!cancellationToken.IsCancellationRequested)
		{
			try
			{
				Ec2InstanceState state = await GetStateAsync(cancellationToken);
				if (state != Ec2InstanceState.InService)
				{
					bool isSpot = await IsSpotInstanceAsync(cancellationToken);
					_logger.LogInformation("EC2 instance is terminating. IsSpot={IsSpot} Reason={InstanceState}", isSpot, state);
					await _terminationCallback(state, isSpot, cancellationToken);
					return;
				}
			}
			catch (Exception e)
			{
				_logger.LogError(e, "Unhandled exception during EC2 instance monitoring");
			}
			
			await Task.Delay(_pollInterval, cancellationToken);
		}
	}

	private Task HandleTermination(Ec2InstanceState state, bool isSpot, CancellationToken cancellationToken)
	{
		if (isSpot)
		{
			_logger.LogInformation("Shutting down");
			return Shutdown.ExecuteAsync(false, _logger, cancellationToken);
		}
		
		return Task.CompletedTask;
	}

	protected override async Task ExecuteAsync(CancellationToken stoppingToken)
	{
		await MonitorInstanceLifecycle(stoppingToken);
	}
}

