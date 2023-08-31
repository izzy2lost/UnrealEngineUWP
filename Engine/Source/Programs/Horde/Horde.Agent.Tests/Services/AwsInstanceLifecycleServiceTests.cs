// Copyright Epic Games, Inc. All Rights Reserved.

using System.Net;
using System.Net.Http;
using System.Threading;
using System.Threading.Tasks;
using Horde.Agent.Services;
using Microsoft.Extensions.Logging;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Moq;
using Moq.Protected;

namespace Horde.Agent.Tests.Services;

/// <summary>
/// Fake implementation of the AWS EC2 metadata server (IMDS)
/// </summary>
public class FakeAwsImds
{
	public const string BaseUrl = "http://169.254.169.254/latest/meta-data";
	public const string SpotInstanceData = "{\"action\": \"terminate\", \"time\": \"2017-09-18T08:22:00Z\"}";
	public const string OnDemand = "on-demand";
	public const string Spot = "spot";
	
	private readonly Mock<HttpMessageHandler> _mock;
	public string InstanceLifeCycle { get; set; } = OnDemand;
	public string? TargetLifecycleState { get; set; }
	public string? SpotInstanceAction { get; set; }
	
	public FakeAwsImds()
	{
		_mock = new (MockBehavior.Strict);
		_mock
			.Protected()
			.Setup<Task<HttpResponseMessage>>(
				"SendAsync",
				ItExpr.IsAny<HttpRequestMessage>(),
				ItExpr.IsAny<CancellationToken>()
			)
			.Returns(SendAsync);
		
		// Ignore Dispose() calls
		_mock.Protected().Setup("Dispose", ItExpr.IsAny<bool>());
	}

	public HttpClient GetHttpClient()
	{
		return new (_mock.Object);
	}

	private Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken cancellationToken)
	{
		switch (request.RequestUri!.ToString())
		{
			case $"{BaseUrl}/autoscaling/target-lifecycle-state":
				return Task.FromResult(CreateResponse(HttpStatusCode.OK, TargetLifecycleState));

			case $"{BaseUrl}/spot/instance-action":
				return Task.FromResult(CreateResponse(HttpStatusCode.OK, SpotInstanceAction));

			case $"{BaseUrl}/instance-life-cycle":
				return Task.FromResult(CreateResponse(HttpStatusCode.OK, InstanceLifeCycle));

			default:
				return Task.FromResult(CreateResponse(HttpStatusCode.InternalServerError, "Invalid state"));
		}
	}
	
	private static HttpResponseMessage CreateResponse(HttpStatusCode status, string? data)
	{
		return data != null 
			? new HttpResponseMessage { StatusCode = status, Content = new StringContent(data) }
			: new HttpResponseMessage { StatusCode = HttpStatusCode.NotFound, Content = new StringContent("Not found") };
	}
}

[TestClass]
public sealed class AwsInstanceLifecycleServiceTests : System.IDisposable
{
	private readonly LoggerFactory _loggerFactory = new ();
	private readonly HttpClient _httpClient;
	private readonly FakeAwsImds _fakeImds = new ();
	private readonly AwsInstanceLifecycleService _service;
	private Ec2InstanceState? _terminationState;
	private bool? _terminationIsSpot;
	
	public AwsInstanceLifecycleServiceTests()
	{
		_httpClient = _fakeImds.GetHttpClient();
		_service = new AwsInstanceLifecycleService(_httpClient, null!, _loggerFactory.CreateLogger<AwsInstanceLifecycleService>());
		_service._terminationCallback = (state, isSpot, _) =>
		{
			_terminationState = state;
			_terminationIsSpot = isSpot;
			return Task.CompletedTask;
		};
	}

	[TestMethod]
	public async Task Terminate_Asg_CallbackHasCorrectParameters()
	{
		_fakeImds.TargetLifecycleState = "Terminated";
		await _service.MonitorInstanceLifecycle(CancellationToken.None);
		Assert.AreEqual(Ec2InstanceState.TerminatingAsg, _terminationState);
		Assert.IsFalse(_terminationIsSpot);
	}
	
	[TestMethod]
	public async Task Terminate_Spot_CallbackHasCorrectParameters()
	{
		_fakeImds.SpotInstanceAction = FakeAwsImds.SpotInstanceData;
		_fakeImds.InstanceLifeCycle = FakeAwsImds.Spot;
		await _service.MonitorInstanceLifecycle(CancellationToken.None);
		Assert.AreEqual(Ec2InstanceState.TerminatingSpot, _terminationState);
		Assert.IsTrue(_terminationIsSpot);
	}

	public void Dispose()
	{
		_loggerFactory.Dispose();
		_httpClient.Dispose();
		_service.Dispose();
	}
}