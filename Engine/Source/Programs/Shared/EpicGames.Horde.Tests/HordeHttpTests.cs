// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Server;
using EpicGames.OIDC;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace EpicGames.Horde.Tests;

public class StubHttpClientFactory : IHttpClientFactory
{
	private readonly HttpClient _httpClient;

	public StubHttpClientFactory(HttpClient httpClient)
	{
		_httpClient = httpClient;
	}

	public HttpClient CreateClient(string name)
	{
		return _httpClient;
	}
}

public class StubMessageHandler : HttpMessageHandler
{
	public HttpStatusCode StatusCode { get; }
	public string Content { get; }
	public List<HttpRequestMessage> HttpRequests { get; } = new();

	public StubMessageHandler(HttpStatusCode statusCode, string content)
	{
		StatusCode = statusCode;
		Content = content;
	}
	
	protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken cancellationToken)
	{
		Console.WriteLine($"Saving request {request.RequestUri} {request.Headers.Authorization}");
		HttpRequests.Add(request);
		return Task.FromResult(new HttpResponseMessage(StatusCode) { Content = new StringContent(Content) });
	}
}

[TestClass]
public class HordeHttpAuthHandlerTests
{
	const string HordeServerUrl = "http://horde-server-test";
	const string FakeServerResponse = "fakeServerResponse";
	
	[TestMethod]
	public async Task AccessToken_Valid_IsReusedAsync()
	{
		FakeOidcTokenManager oidc = new ();
		(HttpClient client, StubMessageHandler server) = CreateClientServer(oidc);

		await SendHttpRequestAsync(client);
		string firstAccessToken = GetLastUsedAccessToken(server);
		Assert.AreEqual(oidc.AccessToken, firstAccessToken);
		
		await SendHttpRequestAsync(client);
		Assert.AreEqual(oidc.AccessToken, GetLastUsedAccessToken(server));
		Assert.AreEqual(firstAccessToken, GetLastUsedAccessToken(server));
	}
	
	[TestMethod]
	public async Task AccessToken_Expired_IsRefreshedAsync()
	{
		FakeOidcTokenManager oidc = new ();
		(HttpClient client, StubMessageHandler server) = CreateClientServer(oidc);

		oidc.RefreshToken = "someRefreshToken";
		oidc.AccessToken = "expiredAccessToken";
		
		// Set it as expired two hours ago, causing a refresh of access token
		oidc.AccessTokenExpiry = DateTimeOffset.Now.AddHours(-2);
		
		await SendHttpRequestAsync(client);
		Assert.AreEqual(oidc.AccessToken, GetLastUsedAccessToken(server));
	}

	[SuppressMessage("Reliability", "CA2000:Dispose objects before losing scope")]
	private static (HttpClient client, StubMessageHandler server) CreateClientServer(IOidcTokenManager oidcTokenManager, HordeOptions? hordeOptions = null)
	{
		GetAuthConfigResponse authConfig = new () { Method = AuthMethod.OpenIdConnect, ServerUrl = HordeServerUrl, LocalRedirectUrls = new [] { HordeServerUrl } };
		IHttpClientFactory httpFactory = GetHordeHttpClientFactory(HordeServerUrl, authConfig);
		using ILoggerFactory loggerFactory = LoggerFactory.Create(builder =>
		{
			builder.SetMinimumLevel(LogLevel.Debug);
			builder.AddSimpleConsole(options => { options.SingleLine = true; });
		});

		ILogger<HordeHttpAuthHandler> logger = loggerFactory.CreateLogger<HordeHttpAuthHandler>();

		OptionsWrapper<HordeOptions> options = new (hordeOptions ?? new HordeOptions());
		InMemoryTokenStore inMemoryTokenStore = new ();
		HordeHttpAuthHandlerState state = new (httpFactory, options, logger, inMemoryTokenStore, oidcTokenManager);
		HordeHttpAuthHandler authHandler = new (state, options);
		
		StubMessageHandler server = new (HttpStatusCode.OK, FakeServerResponse);
		authHandler.InnerHandler = server;
		HttpClient client = new (authHandler);
		return (client, server);
	}

	private static string GetLastUsedAccessToken(StubMessageHandler server)
	{
		return server.HttpRequests.Last().Headers.Authorization!.Parameter!;
	}
	
	private static async Task SendHttpRequestAsync(HttpClient client)
	{
		using HttpRequestMessage req = new (HttpMethod.Get, HordeServerUrl);
		HttpResponseMessage res = await client.SendAsync(req);
		Assert.AreEqual(HttpStatusCode.OK, res.StatusCode);
		Assert.AreEqual(FakeServerResponse, await res.Content.ReadAsStringAsync());
	}
	
	private static IHttpClientFactory GetHordeHttpClientFactory(string? baseAddress, GetAuthConfigResponse response)
	{
		using StubMessageHandler stubMessageHandler = new (HttpStatusCode.OK, JsonSerializer.Serialize(response));
		return new StubHttpClientFactory(new HttpClient(stubMessageHandler)
		{
			BaseAddress = baseAddress != null ? new Uri(baseAddress) : null,
		});
	}
}
