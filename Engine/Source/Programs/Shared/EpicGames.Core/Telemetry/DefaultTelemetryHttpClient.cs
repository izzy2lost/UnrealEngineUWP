// Copyright Epic Games, Inc. All Rights Reserved.

using Microsoft.Extensions.Http;
using Polly;
using Polly.Extensions.Http;
using System;
using System.Net.Http;

namespace EpicGames.Core.Telemetry
{
	public static class HttpClientDefaults
	{
		// When services can be used, utilize this helper to get this policy that handles retries
		//ex. ServicesCollection.AddHttpClient<TelemetryService>().AddPolicyHandler(HttpClientDefaults.GetPolicy());
		public static Polly.Retry.AsyncRetryPolicy<HttpResponseMessage> GetRetryPolicy()
		{
			return HttpPolicyExtensions
				// 408, 5XX responses 
				.HandleTransientHttpError()
				// 504 should have been covered by HandleTransientHttpError but adding here to be safe
				.OrResult(msg => msg.StatusCode == System.Net.HttpStatusCode.NotFound ||
								 msg.StatusCode == System.Net.HttpStatusCode.GatewayTimeout)
				.WaitAndRetryAsync(3, retryAttempt => TimeSpan.FromSeconds(Math.Pow(2, retryAttempt)));
		}

		// Returns new HttpClient configured with our default retry policy
		public static HttpClient GetClient()
		{
			// per MS documentation, with a lifetime specified we shouldn't have socket exhaustion issues
			SocketsHttpHandler socketHandler = new SocketsHttpHandler { PooledConnectionLifetime = TimeSpan.FromMinutes(15) };
#pragma warning disable CA2000 // Dispose objects before losing scope
			PolicyHttpMessageHandler pollyHandler = new PolicyHttpMessageHandler(GetRetryPolicy())
			{
				InnerHandler = socketHandler,
			};
#pragma warning restore CA2000 // Dispose objects before losing scope

			return new HttpClient(pollyHandler);
		}
	}
}