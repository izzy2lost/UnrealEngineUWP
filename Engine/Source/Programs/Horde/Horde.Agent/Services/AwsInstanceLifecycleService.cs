// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
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
	private readonly FileReference _terminationSignalFile;
	
	internal delegate Task TerminationWarningDelegate(Ec2InstanceState state, bool isSpot, TimeSpan timeToLive, CancellationToken cancellationToken);
	internal delegate Task TerminationDelegate(Ec2InstanceState state, bool isSpot, CancellationToken cancellationToken);
	internal TerminationWarningDelegate _terminationWarningCallback;
	internal TerminationDelegate _terminationCallback;

	/// <summary>
	/// Time to live for EC2 instance once a termination is detected coming from the auto-scaling group (ASG)
	/// In practice, this is dictated by the lifecycle hook set for the ASG.
	/// Set to 120 sec to mimic the TTL for spot interruption, leading to similar handling of both for now.
	/// </summary>
	internal TimeSpan _timeToLiveAsg = TimeSpan.FromSeconds(120); 
	
	/// <summary>
	/// Time to live for EC2 instance once a spot interruption is detected. Strictly defined by AWS EC2.
	/// </summary>
	internal TimeSpan _timeToLiveSpot = TimeSpan.FromSeconds(120); // Strictly defined by AWS EC2
	
	/// <summary>
	/// Duration of the time-to-live to allocate towards shutting down the Horde agent and the machine itself.
	/// Example: if TTL is 120 seconds, 90 seconds will be reported in the termination warning.
	/// </summary>
	internal TimeSpan _terminationBufferTime = TimeSpan.FromSeconds(30);

	/// <summary>
	/// Constructor
	/// </summary>
	public AwsInstanceLifecycleService(HttpClient httpClient, IOptions<AgentSettings> settings, ILogger<AwsInstanceLifecycleService> logger)
	{
		_httpClient = httpClient;
		_httpClient.Timeout = TimeSpan.FromSeconds(2);
		_terminationWarningCallback = OnTerminationWarningAsync;
		_terminationCallback = OnTerminationAsync;
		_logger = logger;
		_terminationSignalFile = settings.Value.GetTerminationSignalFile();
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
	
	private async Task<bool> IsImdsAvailableAsync(CancellationToken cancellationToken)
	{
		try
		{
			HttpResponseMessage res = await _httpClient.GetAsync(new Uri(BaseUri + "/"), cancellationToken);
			return res.StatusCode == HttpStatusCode.OK;
		}
		catch (Exception)
		{
			// Timed out or other error. Can safely assume the metadata server is not available.
			return false;
		}
	}
	
	internal async Task MonitorInstanceLifecycleAsync(CancellationToken cancellationToken)
	{
		if (!await IsImdsAvailableAsync(cancellationToken))
		{
			_logger.LogInformation("EC2 metadata server (IMDS) not available. Will not monitor EC2 lifecycle state");
			return;
		}
		
		_logger.LogInformation("Monitoring EC2 instance lifecycle state...");
		while (!cancellationToken.IsCancellationRequested)
		{
			try
			{
				Ec2InstanceState state = await GetStateAsync(cancellationToken);
				if (state != Ec2InstanceState.InService)
				{
					bool isSpot = await IsSpotInstanceAsync(cancellationToken);
					TimeSpan ttl = GetTimeToLive(state);
					_logger.LogInformation("EC2 instance is terminating. IsSpot={IsSpot} Reason={InstanceState} TimeToLive={Ttk} ms", isSpot, state, ttl.TotalMilliseconds);

					ttl -= _terminationBufferTime;
					ttl = ttl.Ticks >= 0 ? ttl : TimeSpan.Zero; 
					
					await _terminationWarningCallback(state, isSpot, ttl, cancellationToken);
					await Task.Delay(ttl, cancellationToken);
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

	/// <summary>
	/// Determine time to live for the current EC2 instance once a terminating state has been detected
	/// </summary>
	/// <param name="state">Current state</param>
	/// <returns>Time to live</returns>
	/// <exception cref="ArgumentException"></exception>
	private TimeSpan GetTimeToLive(Ec2InstanceState state)
	{
		return state switch
		{
			Ec2InstanceState.TerminatingAsg => _timeToLiveAsg, 
			Ec2InstanceState.TerminatingSpot => _timeToLiveSpot,
			_ => throw new ArgumentException($"Invalid state {state}")
		};
	}

	private async Task OnTerminationWarningAsync(Ec2InstanceState state, bool isSpot, TimeSpan timeToLive, CancellationToken cancellationToken)
	{
		// Create and write the termination signal file, containing the time-to-live for the EC2 instance.
		// Workloads executed by the agent that support this protocol can pick this up and prepare/clean up prior to termination
		await WriteTerminationSignalFileAsync(timeToLive, cancellationToken);
	}
	
	private Task OnTerminationAsync(Ec2InstanceState state, bool isSpot, CancellationToken cancellationToken)
	{
		if (isSpot)
		{
			_logger.LogInformation("Shutting down");
			return Shutdown.ExecuteAsync(false, _logger, cancellationToken);
		}
		
		return Task.CompletedTask;
	}

	private Task WriteTerminationSignalFileAsync(TimeSpan timeToLive, CancellationToken cancellationToken)
	{
		string contents = $"v1\t{timeToLive.TotalMilliseconds}";
		return File.WriteAllTextAsync(_terminationSignalFile.FullName, contents, cancellationToken);
	}

	/// <inheritdoc/>
	protected override async Task ExecuteAsync(CancellationToken stoppingToken)
	{
		await MonitorInstanceLifecycleAsync(stoppingToken);
	}
}

