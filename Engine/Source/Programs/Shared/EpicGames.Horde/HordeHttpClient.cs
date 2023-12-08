// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Net.Http.Json;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Agents.Telemetry;
using EpicGames.Horde.Artifacts;
using EpicGames.Horde.Dashboard;
using EpicGames.Horde.Projects;
using EpicGames.Horde.Secrets;
using EpicGames.Horde.Server;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Clients;
using EpicGames.Horde.Tools;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Polly;
using Polly.Extensions.Http;
using Polly.Retry;
using Polly.Timeout;

#pragma warning disable CA2234

namespace EpicGames.Horde
{
	/// <summary>
	/// Wraps an Http client which communicates with the Horde server
	/// </summary>
	public sealed class HordeHttpClient : IDisposable
	{
		/// <summary>
		/// Name of an environment variable containing the Horde server URL
		/// </summary>
		public const string HordeUrlEnvVarName = "UE_HORDE_URL";

		/// <summary>
		/// Name of an environment variable containing a token for connecting to the Horde server
		/// </summary>
		public const string HordeTokenEnvVarName = "UE_HORDE_TOKEN";

		/// <summary>
		/// Name of clients created from the http client factory
		/// </summary>
		public const string HttpClientName = "HordeHttpClient";

		readonly HttpClient _httpClient;

		static readonly JsonSerializerOptions s_jsonSerializerOptions = CreateJsonSerializerOptions();
		internal static JsonSerializerOptions JsonSerializerOptions => s_jsonSerializerOptions;

		/// <summary>
		/// Base address for the Horde server
		/// </summary>
		public Uri BaseUrl => _httpClient.BaseAddress ?? throw new InvalidOperationException("Expected Horde server base address to be configured");

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="httpClient">The inner HTTP client instance</param>
		public HordeHttpClient(HttpClient httpClient)
		{
			_httpClient = httpClient;
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			_httpClient.Dispose();
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

		#region Artifacts

		/// <summary>
		/// Gets metadata about an artifact object
		/// </summary>
		/// <param name="id">Identifier for the artifact</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public Task<GetArtifactResponse> GetArtifactAsync(ArtifactId id, CancellationToken cancellationToken = default)
		{
			return GetAsync<GetArtifactResponse>(_httpClient, $"api/v2/artifacts/{id}", cancellationToken);
		}

		/// <summary>
		/// Finds artifacts with a set of ids or keys
		/// </summary>
		/// <param name="ids">Artifact ids to return</param>
		/// <param name="keys">Keys to find</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about all the artifacts</returns>
		public async Task<List<GetArtifactResponse>> FindArtifactsAsync(IEnumerable<ArtifactId>? ids = null, IEnumerable<string>? keys = null, CancellationToken cancellationToken = default)
		{
			QueryStringBuilder queryParams = new QueryStringBuilder();
			if (ids != null)
			{
				foreach (ArtifactId id in ids)
				{
					queryParams.Add("id", id.ToString());
				}
			}
			if (keys != null)
			{
				foreach (string key in keys)
				{
					queryParams.Add("key", key);
				}
			}

			FindArtifactsResponse response = await GetAsync<FindArtifactsResponse>(_httpClient, $"api/v2/artifacts?{queryParams}", cancellationToken);
			return response.Artifacts;
		}

		#endregion

		#region Dashboard

		/// <summary>
		/// Create a new dashboard preview item
		/// </summary>
		/// <param name="request">Request to create a new preview item</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Config information needed by the dashboard</returns>
		public Task<GetDashboardPreviewResponse> CreateDashbordPreviewAsync(CreateDashboardPreviewRequest request, CancellationToken cancellationToken = default)
		{
			return PostAsync<GetDashboardPreviewResponse, CreateDashboardPreviewRequest>(_httpClient, $"api/v1/dashboard/preview", request, cancellationToken); 
		}

		/// <summary>
		/// Update a dashboard preview item
		/// </summary>
		/// <returns>Config information needed by the dashboard</returns>
		public Task<GetDashboardPreviewResponse> UpdateDashbordPreviewAsync(UpdateDashboardPreviewRequest request, CancellationToken cancellationToken = default)
		{
			return PutAsync<GetDashboardPreviewResponse, UpdateDashboardPreviewRequest>(_httpClient, $"api/v1/dashboard/preview", request, cancellationToken);
		}

		/// <summary>
		/// Query dashboard preview items
		/// </summary>
		/// <returns>Config information needed by the dashboard</returns>
		public Task<List<GetDashboardPreviewResponse>> GetDashbordPreviewsAsync(bool open = true, CancellationToken cancellationToken = default)
		{
			return GetAsync<List<GetDashboardPreviewResponse>>(_httpClient, $"api/v1/dashboard/preview?open={open}", cancellationToken);
		}

		#endregion

		#region Projects

		/// <summary>
		/// Query all the projects
		/// </summary>
		/// <param name="includeStreams">Whether to include streams in the response</param>
		/// <param name="includeCategories">Whether to include categories in the response</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about all the projects</returns>
		public Task<List<GetProjectResponse>> GetProjectsAsync(bool includeStreams = false, bool includeCategories = false, CancellationToken cancellationToken = default)
		{
			return GetAsync<List<GetProjectResponse>>(_httpClient, $"api/v1/projects?includeStreams={includeStreams}&includeCategories={includeCategories}", cancellationToken);
		}

		/// <summary>
		/// Retrieve information about a specific project
		/// </summary>
		/// <param name="projectId">Id of the project to get information about</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about the requested project</returns>
		public Task<GetProjectResponse> GetProjectAsync(ProjectId projectId, CancellationToken cancellationToken = default)
		{
			return GetAsync<GetProjectResponse>(_httpClient, $"api/v1/projects/{projectId}", cancellationToken);
		}

		#endregion

		#region Secrets

		/// <summary>
		/// Query all the secrets available to the current user
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about all the projects</returns>
		public Task<GetSecretsResponse> GetSecretsAsync(CancellationToken cancellationToken = default)
		{
			return GetAsync<GetSecretsResponse>(_httpClient, $"api/v1/secrets", cancellationToken);
		}

		/// <summary>
		/// Retrieve information about a specific project
		/// </summary>
		/// <param name="secretId">Id of the secret to retrieve</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about the requested project</returns>
		public Task<GetSecretResponse> GetSecretAsync(SecretId secretId, CancellationToken cancellationToken = default)
		{
			return GetAsync<GetSecretResponse>(_httpClient, $"api/v1/secrets/{secretId}", cancellationToken);
		}

		#endregion

		#region Server

		/// <summary>
		/// Gets information about the currently deployed server version
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about the deployed server instance</returns>
		public Task<GetServerInfoResponse> GetServerInfoAsync(CancellationToken cancellationToken = default)
		{
			return GetAsync<GetServerInfoResponse>(_httpClient, "api/v1/server/info", cancellationToken);
		}

		#endregion

		#region Telemetry

		/// <summary>
		/// Gets telemetry for Horde within a given range
		/// </summary>
		/// <param name="endDate">End date for the range</param>
		/// <param name="range">Number of hours to return</param>
		/// <param name="tzOffset">Timezone offset</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public Task<List<UtilizationTelemetryResponse>> GetTelemetryAsync(DateTime endDate, int range, int? tzOffset = null, CancellationToken cancellationToken = default)
		{
			QueryStringBuilder queryParams = new QueryStringBuilder();
			queryParams.Add("Range", range.ToString());
			if (tzOffset != null)
			{
				queryParams.Add("TzOffset", tzOffset.Value.ToString());
			}
			return GetAsync<List<UtilizationTelemetryResponse>>(_httpClient, $"api/v1/reports/utilization/{endDate}?{queryParams}", cancellationToken);
		}

		#endregion

		#region Tools

		/// <summary>
		/// Enumerates all the available tools.
		/// </summary>
		public Task<GetToolsSummaryResponse> GetToolsAsync(CancellationToken cancellationToken = default)
		{
			return GetAsync<GetToolsSummaryResponse>(_httpClient, "api/v1/tools", cancellationToken);
		}

		/// <summary>
		/// Gets information about a particular tool
		/// </summary>
		public Task<GetToolResponse> GetToolAsync(ToolId id, CancellationToken cancellationToken = default)
		{
			return GetAsync<GetToolResponse>(_httpClient, $"api/v1/tools/{id}", cancellationToken);
		}

		/// <summary>
		/// Gets information about a particular deployment
		/// </summary>
		public Task<GetToolDeploymentResponse> GetToolDeploymentAsync(ToolId id, ToolDeploymentId deploymentId, CancellationToken cancellationToken = default)
		{
			return GetAsync<GetToolDeploymentResponse>(_httpClient, $"api/v1/tools/{id}/deployments/{deploymentId}", cancellationToken);
		}

		/// <summary>
		/// Gets a zip stream for a particular deployment
		/// </summary>
		public async Task<Stream> GetToolDeploymentZipAsync(ToolId id, ToolDeploymentId? deploymentId, CancellationToken cancellationToken = default)
		{
			if (deploymentId == null)
			{
				return await _httpClient.GetStreamAsync($"api/v1/tools/{id}?action=zip", cancellationToken);
			}
			else
			{
				return await _httpClient.GetStreamAsync($"api/v1/tools/{id}/deployments/{deploymentId}?action=zip", cancellationToken);
			}
		}

		/// <summary>
		/// Creates a new tool deployment
		/// </summary>
		/// <param name="id">Id for the tool</param>
		/// <param name="version">Version string for the new deployment</param>
		/// <param name="duration">Duration over which to deploy the tool</param>
		/// <param name="createPaused">Whether to create the deployment, but do not start rolling it out yet</param>
		/// <param name="node">Location of a directory node describing the deployment</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task<ToolDeploymentId> CreateToolDeploymentAsync(ToolId id, string? version, double? duration, bool? createPaused, BlobLocator node, CancellationToken cancellationToken = default)
		{
			CreateToolDeploymentRequest request = new CreateToolDeploymentRequest(version ?? String.Empty, duration, createPaused, node);
			CreateToolDeploymentResponse response = await PostAsync<CreateToolDeploymentResponse, CreateToolDeploymentRequest>(_httpClient, $"api/v2/tools/{id}/deployments", request, cancellationToken);
			return response.Id;
		}

		#endregion

		#region Utility Methods

		/// <summary>
		/// Gets a resource from an HTTP endpoint and parses it as a JSON object
		/// </summary>
		/// <typeparam name="TResponse">The object type to return</typeparam>
		/// <param name="httpClient">Http client instance</param>
		/// <param name="relativePath">The url to retrieve</param>
		/// <param name="cancellationToken">Cancels the request</param>
		/// <returns>New instance of the object</returns>
		internal static async Task<TResponse> GetAsync<TResponse>(HttpClient httpClient, string relativePath, CancellationToken cancellationToken = default)
		{
			TResponse? response = await httpClient.GetFromJsonAsync<TResponse>(relativePath, s_jsonSerializerOptions, cancellationToken);
			return response ?? throw new InvalidCastException($"Expected non-null response from GET to {relativePath}");
		}

		/// <summary>
		/// Posts an object to an HTTP endpoint as a JSON object, and parses the response object
		/// </summary>
		/// <typeparam name="TRequest">The object type to post</typeparam>
		/// <param name="httpClient">Http client instance</param>
		/// <param name="relativePath">The url to retrieve</param>
		/// <param name="request">The object to post</param>
		/// <param name="cancellationToken">Cancels the request</param>
		/// <returns>The response parsed into the requested type</returns>
		internal static async Task<HttpResponseMessage> PostAsync<TRequest>(HttpClient httpClient, string relativePath, TRequest request, CancellationToken cancellationToken = default)
		{
			return await httpClient.PostAsJsonAsync<TRequest>(relativePath, request, s_jsonSerializerOptions, cancellationToken);
		}

		/// <summary>
		/// Posts an object to an HTTP endpoint as a JSON object, and parses the response object
		/// </summary>
		/// <typeparam name="TResponse">The object type to return</typeparam>
		/// <typeparam name="TRequest">The object type to post</typeparam>
		/// <param name="httpClient">Http client instance</param>
		/// <param name="relativePath">The url to retrieve</param>
		/// <param name="request">The object to post</param>
		/// <param name="cancellationToken">Cancels the request</param>
		/// <returns>The response parsed into the requested type</returns>
		internal static async Task<TResponse> PostAsync<TResponse, TRequest>(HttpClient httpClient, string relativePath, TRequest request, CancellationToken cancellationToken = default)
		{
			using (HttpResponseMessage response = await PostAsync<TRequest>(httpClient, relativePath, request, cancellationToken))
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
		/// <param name="httpClient">Http client instance</param>
		/// <param name="relativePath">The url to write to</param>
		/// <param name="request">The object to post</param>
		/// <param name="cancellationToken">Cancels the request</param>
		/// <returns>Response message</returns>
		internal static async Task<HttpResponseMessage> PutAsync<TRequest>(HttpClient httpClient, string relativePath, TRequest request, CancellationToken cancellationToken)
		{
			return await httpClient.PutAsJsonAsync<TRequest>(relativePath, request, s_jsonSerializerOptions, cancellationToken);
		}

		/// <summary>
		/// Puts an object to an HTTP endpoint as a JSON object
		/// </summary>
		/// <typeparam name="TResponse">The object type to return</typeparam>
		/// <typeparam name="TRequest">The object type to post</typeparam>
		/// <param name="httpClient">Http client instance</param>
		/// <param name="relativePath">The url to write to</param>
		/// <param name="request">The object to post</param>
		/// <param name="cancellationToken">Cancels the request</param>
		/// <returns>Response message</returns>
		internal static async Task<TResponse> PutAsync<TResponse, TRequest>(HttpClient httpClient, string relativePath, TRequest request, CancellationToken cancellationToken)
		{
			using (HttpResponseMessage response = await httpClient.PutAsJsonAsync<TRequest>(relativePath, request, s_jsonSerializerOptions, cancellationToken))
			{
				response.EnsureSuccessStatusCode();

				TResponse? responseValue = await response.Content.ReadFromJsonAsync<TResponse>(s_jsonSerializerOptions, cancellationToken);
				return responseValue ?? throw new InvalidCastException($"Expected non-null response from PUT to {relativePath}");
			}
		}

		#endregion
	}

	/// <summary>
	/// Extension methods for Horde HTTP clients
	/// </summary>
	public static class HordeHttpClientExtensions
	{
		/// <summary>
		/// Creates a <see cref="HordeHttpClient"/> instance from an http client factory
		/// </summary>
		public static HordeHttpClient CreateHordeClient(this IHttpClientFactory factory)
		{
			return new HordeHttpClient(factory.CreateClient(HordeHttpClient.HttpClientName));
		}

		/// <summary>
		/// Registers a Horde HTTP client type, and configures it to use the default OIDC message handler.
		/// </summary>
		/// <param name="services">Service collection to add services to</param>
		/// <param name="useAuthChallenge">Whether to prompt the user to authenticate if necessary</param>
		public static IHttpClientBuilder AddHordeHttpClient(this IServiceCollection services, bool useAuthChallenge = true)
		{
			return services.AddHordeHttpClient((sp, client) => { }, useAuthChallenge);
		}

		/// <summary>
		/// Registers a Horde HTTP client type, and configures it to use the default OIDC message handler.
		/// </summary>
		/// <param name="services">Service collection to add services to</param>
		/// <param name="configureClient">Callback to modify options for the http client</param>
		/// <param name="useAuthChallenge">Whether to prompt the user to authenticate if necessary</param>
		public static IHttpClientBuilder AddHordeHttpClient(this IServiceCollection services, Action<HttpClient> configureClient, bool useAuthChallenge = true)
		{
			return services.AddHordeHttpClient((sp, client) => configureClient(client), useAuthChallenge);
		}

		/// <summary>
		/// Registers a Horde HTTP client type, and configures it to use the default OIDC message handler.
		/// </summary>
		/// <param name="services">Service collection to add services to</param>
		/// <param name="configureClient">Callback to modify options for the http client</param>
		/// <param name="useAuthChallenge">Whether to prompt the user to authenticate if necessary</param>
		public static IHttpClientBuilder AddHordeHttpClient(this IServiceCollection services, Action<IServiceProvider, HttpClient> configureClient, bool useAuthChallenge = true)
		{
			// Sets defaults from the environment before calling the user provided configuration method
			void ConfigureClientFromEnvironment(IServiceProvider serviceProvider, HttpClient httpClient)
			{
				httpClient.Timeout = TimeSpan.FromSeconds(240); // Global timeout

				configureClient(serviceProvider, httpClient);

				// Only use the token from the environment if the configured base address is missing or matches the one configured in the environment
				string? hordeUrlEnvVar = Environment.GetEnvironmentVariable(HordeHttpClient.HordeUrlEnvVarName);
				if (!String.IsNullOrEmpty(hordeUrlEnvVar))
				{
					Uri hordeUrl = new Uri(hordeUrlEnvVar);
					if (httpClient.BaseAddress == null || String.Equals(httpClient.BaseAddress.Host, hordeUrl.Host, StringComparison.OrdinalIgnoreCase))
					{
						httpClient.BaseAddress ??= hordeUrl;

						if (httpClient.DefaultRequestHeaders.Authorization == null)
						{
							string? hordeToken = Environment.GetEnvironmentVariable(HordeHttpClient.HordeTokenEnvVarName);
							if (!String.IsNullOrEmpty(hordeToken))
							{
								httpClient.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Bearer", hordeToken);
							}
						}
					}
				}
			}

			IHttpClientBuilder builder = services.AddHttpClient<HordeHttpClient>(HordeHttpClient.HttpClientName, ConfigureClientFromEnvironment)
				.AddPolicyHandler((serviceProvider, request) => CreateDefaultTimeoutRetryPolicy(request, serviceProvider.GetRequiredService<ILogger<HttpStorageClient>>()))
				.AddPolicyHandler((serviceProvider, request) => CreateDefaultTransientErrorPolicy(request, serviceProvider.GetRequiredService<ILogger<HttpStorageClient>>()));

			if (useAuthChallenge)
			{
				services.AddSingleton<HordeHttpAuthHandlerState>();
				services.AddTransient<HordeHttpAuthHandler>();
				services.AddHttpClient(HordeHttpAuthHandlerState.HttpClientName, configureClient);

				builder = builder.AddHttpMessageHandler<HordeHttpAuthHandler>();
			}

			return builder;
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
