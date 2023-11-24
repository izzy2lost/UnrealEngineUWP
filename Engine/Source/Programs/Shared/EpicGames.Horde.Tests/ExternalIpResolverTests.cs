// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Net;
using System.Net.Http;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Compute;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace EpicGames.Horde.Tests;

[TestClass]
public class ExternalIpResolverTests
{
	private class StubMessageHandler : HttpMessageHandler
	{
		private readonly HttpStatusCode _statusCode;
		private readonly string _content;

		public StubMessageHandler(HttpStatusCode statusCode, string content)
		{
			_statusCode = statusCode;
			_content = content;
		}

		protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken cancellationToken)
		{
			return Task.FromResult(new HttpResponseMessage(_statusCode) { Content = new StringContent(_content) });
		}
	}
	
	[TestMethod]
	public async Task BasicAsync()
	{
		Assert.AreEqual(IPAddress.Parse("192.168.2.3"), await GetIpAsync("192.168.2.3"));
		await Assert.ThrowsExceptionAsync<ExternalIpResolverException>(() => GetIpAsync("bad-ip"));
		await Assert.ThrowsExceptionAsync<ExternalIpResolverException>(() => GetIpAsync("192.168.2.3", HttpStatusCode.NotFound));
	}
	
	[TestMethod]
	[Ignore]
	public async Task IntegrationAsync()
	{
		using HttpClient httpClient = new ();
		ExternalIpResolver resolver = new (httpClient);
		IPAddress externalIp = await resolver.GetExternalIpAddressAsync();
		Console.WriteLine("External IP: " + externalIp);
	}

	private static async Task<IPAddress> GetIpAsync(string content, HttpStatusCode statusCode = HttpStatusCode.OK, CancellationToken cancellationToken = default)
	{
		using StubMessageHandler handler = new (statusCode, content);
		using HttpClient httpClient = new (handler);
		ExternalIpResolver resolver = new (httpClient);
		return await resolver.GetExternalIpAddressAsync(cancellationToken);
	}
}