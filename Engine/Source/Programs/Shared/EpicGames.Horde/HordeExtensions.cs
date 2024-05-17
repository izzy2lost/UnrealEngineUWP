// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Net.Http;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Backends;
using EpicGames.Horde.Storage.Bundles;
using EpicGames.Horde.Storage.Clients;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using Polly;
using Polly.Extensions.Http;
using Polly.Retry;
using Polly.Timeout;

namespace EpicGames.Horde
{
	/// <summary>
	/// Extension methods for Horde
	/// </summary>
	public static class HordeExtensions
	{
		/// <summary>
		/// Adds Horde-related services with the default settings
		/// </summary>
		/// <param name="serviceCollection">Collection to register services with</param>
		public static void AddHorde(this IServiceCollection serviceCollection)
		{
			// Sets defaults from the environment before calling the user provided configuration method
			void ConfigureClientFromEnvironment(IServiceProvider serviceProvider, HttpClient httpClient)
			{
				httpClient.Timeout = TimeSpan.FromSeconds(240); // Global timeout
			}

			// Register the HTTP client for handling login requests
			void ConfigureClientFromOptions(IServiceProvider serviceProvider, HttpClient httpClient)
			{
				IOptions<HordeOptions> options = serviceProvider.GetRequiredService<IOptions<HordeOptions>>();
				if (options.Value.ServerUrl != null)
				{
					httpClient.BaseAddress = options.Value.ServerUrl;
				}
			}

			serviceCollection.AddSingleton<HordeHttpAuthHandlerState>();
			serviceCollection.AddTransient<HordeHttpAuthHandler>();
			serviceCollection.AddHttpClient(HordeHttpAuthHandlerState.HttpClientName, ConfigureClientFromOptions);

			// Register the HTTP client for processing upload redirects
			serviceCollection.AddHttpClient(HordeHttpClient.UploadRedirectHttpClientName)
				.AddPolicyHandler((serviceProvider, request) => CreateDefaultTimeoutRetryPolicy(request, serviceProvider.GetRequiredService<ILogger<HttpStorageBackend>>()))
				.AddPolicyHandler((serviceProvider, request) => CreateDefaultTransientErrorPolicy(request, serviceProvider.GetRequiredService<ILogger<HttpStorageBackend>>()));

			// Create the HTTP client for handling Horde requests
			serviceCollection.AddHttpClient<HordeHttpClient>(HordeHttpClient.HttpClientName, ConfigureClientFromEnvironment)
				.AddHttpMessageHandler<HordeHttpAuthHandler>()
				.AddPolicyHandler((serviceProvider, request) => CreateDefaultTimeoutRetryPolicy(request, serviceProvider.GetRequiredService<ILogger<HttpStorageBackend>>()))
				.AddPolicyHandler((serviceProvider, request) => CreateDefaultTransientErrorPolicy(request, serviceProvider.GetRequiredService<ILogger<HttpStorageBackend>>()));

			static StorageBackendCache CreateBackendCache(IServiceProvider serviceProvider)
			{
				StorageBackendCacheOptions options = serviceProvider.GetRequiredService<IOptions<HordeOptions>>().Value.BackendCache;
				DirectoryReference? cacheDir = String.IsNullOrEmpty(options.CacheDir) ? null : new DirectoryReference(options.CacheDir);
				return new StorageBackendCache(cacheDir, options.MaxSize, serviceProvider.GetRequiredService<ILogger<StorageBackendCache>>());
			}

			serviceCollection.AddSingleton<BundleCache>(sp => new BundleCache(sp.GetRequiredService<IOptions<HordeOptions>>().Value.BundleCache));
			serviceCollection.AddSingleton<StorageBackendCache>(CreateBackendCache);
			serviceCollection.AddSingleton<HttpStorageBackendFactory>();
			serviceCollection.AddSingleton<HttpStorageClientFactory>();
			serviceCollection.AddSingleton<IHordeClient>(sp => sp.GetRequiredService<IHordeClientFactory>().Create());
			serviceCollection.AddSingleton<IHordeClientFactory, HordeClientFactory>();
		}

		/// <summary>
		/// Adds Horde-related services
		/// </summary>
		/// <param name="serviceCollection">Collection to register services with</param>
		/// <param name="configureHorde">Callback to configure options</param>
		public static void AddHorde(this IServiceCollection serviceCollection, Action<HordeOptions> configureHorde)
		{
			serviceCollection.Configure(configureHorde);
			AddHorde(serviceCollection);
		}

		/// <summary>
		/// Create a default timeout retry policy
		/// </summary>
		public static IAsyncPolicy<HttpResponseMessage> CreateDefaultTimeoutRetryPolicy(HttpRequestMessage request, ILogger logger)
		{
			// Wait 30 seconds for operations to timeout
			Task OnTimeoutAsync(Context context, TimeSpan timespan, Task timeoutTask)
			{
				logger.LogWarning(KnownLogEvents.Systemic_Horde_Http, "{Method} {Url} timed out after {Time}s.", request.Method, request.RequestUri, (int)timespan.TotalSeconds);
				return Task.CompletedTask;
			}

			AsyncTimeoutPolicy<HttpResponseMessage> timeoutPolicy = Policy.TimeoutAsync<HttpResponseMessage>(30, OnTimeoutAsync);

			// Retry twice after a timeout
			void OnRetry(Exception ex, TimeSpan timespan)
			{
				logger.LogWarning(KnownLogEvents.Systemic_Horde_Http, ex, "{Method} {Url} retrying after {Time}s.", request.Method, request.RequestUri, timespan.TotalSeconds);
			}

			TimeSpan[] retryTimes = new[] { TimeSpan.FromSeconds(5.0), TimeSpan.FromSeconds(10.0) };
			AsyncRetryPolicy retryPolicy = Policy.Handle<TimeoutRejectedException>().WaitAndRetryAsync(retryTimes, OnRetry);
			return retryPolicy.WrapAsync(timeoutPolicy);
		}

		/// <summary>
		/// Create a default timeout retry policy
		/// </summary>
		public static IAsyncPolicy<HttpResponseMessage> CreateDefaultTransientErrorPolicy(HttpRequestMessage request, ILogger logger)
		{
			Task OnTimeoutAsync(DelegateResult<HttpResponseMessage> outcome, TimeSpan timespan, int retryAttempt, Context context)
			{
				logger.LogWarning(KnownLogEvents.Systemic_Horde_Http, "{Method} {Url} failed ({Result}). Delaying for {DelayMs}ms (attempt #{RetryNum}).", request.Method, request.RequestUri, outcome.Result?.StatusCode, timespan.TotalMilliseconds, retryAttempt);
				return Task.CompletedTask;
			}

			TimeSpan[] retryTimes = new[] { TimeSpan.FromSeconds(1.0), TimeSpan.FromSeconds(5.0), TimeSpan.FromSeconds(10.0), TimeSpan.FromSeconds(30.0), TimeSpan.FromSeconds(30.0) };
			return HttpPolicyExtensions.HandleTransientHttpError().WaitAndRetryAsync(retryTimes, OnTimeoutAsync);
		}
	}
}
