// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Net.Http;
using System.Net.Http.Json;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Options;
using Polly;
using Polly.Extensions.Http;

namespace EpicGames.Horde
{
	/// <summary>
	/// Options for configuring a Horde HTTP client
	/// </summary>
	public class HordeHttpClientOptions
	{
		/// <summary>
		/// URL of the Horde server
		/// </summary>
		public Uri ServerUrl { get; set; } = new Uri("http://localhost:5000");
	}

	/// <summary>
	/// Wraps an Http client which communicates with the Horde server
	/// </summary>
	public sealed class HordeHttpClient
	{
		readonly HttpClient _httpClient;

		static readonly JsonSerializerOptions s_jsonSerializerOptions = CreateJsonSerializerOptions();

		/// <summary>
		/// Static instance of the default retry policy
		/// </summary>
		public static IAsyncPolicy<HttpResponseMessage> DefaultRetryPolicy { get; } = CreateDefaultRetryPolicy();

		/// <summary>
		/// Accessor for the underlying HTTP client
		/// </summary>
		public HttpClient HttpClient => _httpClient;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="httpClient">The inner HTTP client instance</param>
		/// <param name="options">Options for the client</param>
		public HordeHttpClient(HttpClient httpClient, IOptions<HordeHttpClientOptions> options)
		{
			_httpClient = httpClient;
			_httpClient.BaseAddress = options.Value.ServerUrl;
		}

		/// <summary>
		/// Create the shared instance of JSON options for HordeHttpClient instances
		/// </summary>
		static JsonSerializerOptions CreateJsonSerializerOptions()
		{
			JsonSerializerOptions options = new JsonSerializerOptions();
			ConfigureJsonSerializer(options);
			return options;
		}

		/// <summary>
		/// Configures a JSON serializer to read Horde responses
		/// </summary>
		/// <param name="options">options for the serializer</param>
		public static void ConfigureJsonSerializer(JsonSerializerOptions options)
		{
			options.AllowTrailingCommas = true;
			options.ReadCommentHandling = JsonCommentHandling.Skip;
			options.DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull;
			options.PropertyNamingPolicy = JsonNamingPolicy.CamelCase;
			options.PropertyNameCaseInsensitive = true;
			options.Converters.Add(new JsonStringEnumConverter());
			options.Converters.Add(new StringIdJsonConverterFactory());
			options.Converters.Add(new BinaryIdJsonConverterFactory());
		}

		/// <summary>
		/// Gets a resource from an HTTP endpoint and parses it as a JSON object
		/// </summary>
		/// <typeparam name="TResponse">The object type to return</typeparam>
		/// <param name="relativePath">The url to retrieve</param>
		/// <param name="cancellationToken">Cancels the request</param>
		/// <returns>New instance of the object</returns>
		public async Task<TResponse> GetAsync<TResponse>(string relativePath, CancellationToken cancellationToken = default)
		{
			TResponse? response = await _httpClient.GetFromJsonAsync<TResponse>(relativePath, s_jsonSerializerOptions, cancellationToken);
			return response ?? throw new InvalidCastException($"Expected non-null response from GET to {relativePath}");
		}

		/// <summary>
		/// Posts an object to an HTTP endpoint as a JSON object, and parses the response object
		/// </summary>
		/// <typeparam name="TRequest">The object type to post</typeparam>
		/// <param name="relativePath">The url to retrieve</param>
		/// <param name="request">The object to post</param>
		/// <param name="cancellationToken">Cancels the request</param>
		/// <returns>The response parsed into the requested type</returns>
		public async Task<HttpResponseMessage> PostAsync<TRequest>(string relativePath, TRequest request, CancellationToken cancellationToken = default)
		{
			return await _httpClient.PostAsJsonAsync<TRequest>(relativePath, request, s_jsonSerializerOptions, cancellationToken);
		}

		/// <summary>
		/// Posts an object to an HTTP endpoint as a JSON object, and parses the response object
		/// </summary>
		/// <typeparam name="TResponse">The object type to return</typeparam>
		/// <typeparam name="TRequest">The object type to post</typeparam>
		/// <param name="relativePath">The url to retrieve</param>
		/// <param name="request">The object to post</param>
		/// <param name="cancellationToken">Cancels the request</param>
		/// <returns>The response parsed into the requested type</returns>
		public async Task<TResponse> PostAsync<TResponse, TRequest>(string relativePath, TRequest request, CancellationToken cancellationToken = default)
		{
			using (HttpResponseMessage response = await PostAsync<TRequest>(relativePath, request, cancellationToken))
			{
				response.EnsureSuccessStatusCode();

				TResponse? responseValue = await response.Content.ReadFromJsonAsync<TResponse>(s_jsonSerializerOptions, cancellationToken);
				return responseValue ?? throw new InvalidCastException($"Expected non-null response from POST to {relativePath}");
			}
		}

		/// <summary>
		/// Puts an object to an HTTP endpoint as a JSON object
		/// </summary>
		/// <typeparam name="TRequest">The object type to post</typeparam>
		/// <param name="relativePath">The url to write to</param>
		/// <param name="request">The object to post</param>
		/// <param name="cancellationToken">Cancels the request</param>
		/// <returns>Response message</returns>
		public async Task<HttpResponseMessage> PutAsync<TRequest>(string relativePath, TRequest request, CancellationToken cancellationToken)
		{
			return await _httpClient.PutAsJsonAsync<TRequest>(relativePath, request, s_jsonSerializerOptions, cancellationToken);
		}

		/// <summary>
		/// Puts an object to an HTTP endpoint as a JSON object
		/// </summary>
		/// <typeparam name="TResponse">The object type to return</typeparam>
		/// <typeparam name="TRequest">The object type to post</typeparam>
		/// <param name="relativePath">The url to write to</param>
		/// <param name="request">The object to post</param>
		/// <param name="cancellationToken">Cancels the request</param>
		/// <returns>Response message</returns>
		public async Task<TResponse> PutAsync<TResponse, TRequest>(string relativePath, TRequest request, CancellationToken cancellationToken)
		{
			using (HttpResponseMessage response = await _httpClient.PutAsJsonAsync<TRequest>(relativePath, request, s_jsonSerializerOptions, cancellationToken))
			{
				response.EnsureSuccessStatusCode();

				TResponse? responseValue = await response.Content.ReadFromJsonAsync<TResponse>(s_jsonSerializerOptions, cancellationToken);
				return responseValue ?? throw new InvalidCastException($"Expected non-null response from PUT to {relativePath}");
			}
		}

		/// <summary>
		/// Helper method to construct a default retry policy for Horde requests
		/// </summary>
		static IAsyncPolicy<HttpResponseMessage> CreateDefaultRetryPolicy()
		{
			return HttpPolicyExtensions
				.HandleTransientHttpError()
				.WaitAndRetryAsync(new[] { TimeSpan.FromSeconds(2.0), TimeSpan.FromSeconds(5.0), TimeSpan.FromSeconds(10), TimeSpan.FromSeconds(30) });
		}
	}

	/// <summary>
	/// Factory for creating HordeHttpClient instances
	/// </summary>
	public class HordeHttpClientFactory
	{
		readonly IServiceProvider _serviceProvider;

		/// <summary>
		/// Constructor
		/// </summary>
		public HordeHttpClientFactory(IServiceProvider serviceProvider) => _serviceProvider = serviceProvider;

		/// <summary>
		/// Creates a client for communicating with the Horde server
		/// </summary>
		public HordeHttpClient CreateClient() => _serviceProvider.GetRequiredService<HordeHttpClient>();
	}

	/// <summary>
	/// Extension methods for Horde HTTP clients
	/// </summary>
	public static class HordeHttpClientExtensions
	{
		/// <summary>
		/// Registers a Horde HTTP client type, and configures it to use the default OIDC message handler.
		/// </summary>
		/// <param name="services">Service collection to add services to</param>
		public static void AddHordeHttpClient(this IServiceCollection services)
		{
			services.AddSingleton<HordeHttpAuthHandler>();
			services.AddHttpClient<HordeHttpClient>().AddPolicyHandler(HordeHttpClient.DefaultRetryPolicy).AddHttpMessageHandler<HordeHttpAuthHandler>();
			services.AddTransient<HordeHttpClientFactory>();
		}

		/// <summary>
		/// Registers a Horde HTTP client type, and configures it to use the default OIDC message handler.
		/// </summary>
		/// <param name="services">Service collection to add services to</param>
		/// <param name="configureOptions">Callback to modify options for the http client</param>
		public static void AddHordeHttpClient(this IServiceCollection services, Action<HordeHttpClientOptions> configureOptions)
		{
			services.AddHordeHttpClient((sp, options) => configureOptions(options));
		}
		/// <summary>
		/// Registers a Horde HTTP client type, and configures it to use the default OIDC message handler.
		/// </summary>
		/// <param name="services">Service collection to add services to</param>
		/// <param name="configureOptions">Callback to modify options for the http client</param>
		public static void AddHordeHttpClient(this IServiceCollection services, Action<IServiceProvider, HordeHttpClientOptions> configureOptions)
		{
			IOptions<HordeHttpClientOptions> CreateOptions(IServiceProvider serviceProvider)
			{
				HordeHttpClientOptions options = new HordeHttpClientOptions();
				configureOptions(serviceProvider, options);
				return Options.Create(options);
			}

			services.AddSingleton<IOptions<HordeHttpClientOptions>>(CreateOptions);
			services.AddHordeHttpClient();
		}
	}
}
