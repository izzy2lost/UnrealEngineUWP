// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde.Api;
using EpicGames.Horde.Compute.Transports;
using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Http;
using System.Net.Http.Json;
using System.Net.Sockets;
using System.Runtime.CompilerServices;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace EpicGames.Horde.Compute.Clients
{
	/// <summary>
	/// Helper class to enlist remote resources to perform compute-intensive tasks.
	/// </summary>
	public sealed class ServerComputeClient : IComputeClient
	{
		/// <summary>
		/// Length of the nonce sent as part of handshaking between initiator and remote
		/// </summary>
		public const int NonceLength = 64;

		record class LeaseInfo(IReadOnlyList<string> Properties, IReadOnlyDictionary<string, int> AssignedResources, RemoteComputeSocket Socket);

		class LeaseImpl : IComputeLease
		{
			readonly IAsyncEnumerator<LeaseInfo> _source;
			
			BackgroundTask? _pingTask;

			public IReadOnlyList<string> Properties => _source.Current.Properties;
			public IReadOnlyDictionary<string, int> AssignedResources => _source.Current.AssignedResources;
			public RemoteComputeSocket Socket => _source.Current.Socket;

			public LeaseImpl(IAsyncEnumerator<LeaseInfo> source)
			{
				_source = source;
				_pingTask = BackgroundTask.StartNew(PingAsync);
			}

			/// <inheritdoc/>
			public async ValueTask DisposeAsync()
			{
				if (_pingTask != null)
				{
					await _pingTask.DisposeAsync();
					_pingTask = null;
				}

				await _source.MoveNextAsync();
				await _source.DisposeAsync();
			}

			/// <inheritdoc/>
			public async ValueTask CloseAsync(CancellationToken cancellationToken)
			{
				if (_pingTask != null)
				{
					await _pingTask.DisposeAsync();
					_pingTask = null;
				}

				await Socket.CloseAsync(cancellationToken);
			}

			async Task PingAsync(CancellationToken cancellationToken)
			{
				while (!cancellationToken.IsCancellationRequested)
				{
					await Socket.SendKeepAliveMessageAsync(cancellationToken);
					await Task.Delay(TimeSpan.FromSeconds(5.0), cancellationToken);
				}
			}
		}

		readonly HordeHttpClientFactory _hordeHttpClientFactory;
		readonly CancellationTokenSource _cancellationSource = new CancellationTokenSource();
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="hordeHttpClientFactory">Factory for constructing Horde http client instances</param>
		/// <param name="logger">Logger for diagnostic messages</param>
		public ServerComputeClient(HordeHttpClientFactory hordeHttpClientFactory, ILogger logger)
		{
			_hordeHttpClientFactory = hordeHttpClientFactory;
			_logger = logger;
		}

		/// <inheritdoc/>
		public ValueTask DisposeAsync()
		{
			Dispose();
			return new ValueTask();
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			_cancellationSource.Dispose();
		}

		/// <inheritdoc/>
		public async Task<IComputeLease?> TryAssignWorkerAsync(ClusterId clusterId, Requirements? requirements, string? requestId, ILogger logger, CancellationToken cancellationToken)
		{
			IAsyncEnumerator<LeaseInfo> source = ConnectAsync(clusterId, requirements, requestId, logger, cancellationToken).GetAsyncEnumerator(cancellationToken);
			if (!await source.MoveNextAsync())
			{
				await source.DisposeAsync();
				return null;
			}
			return new LeaseImpl(source);
		}

		/// <inheritdoc/>
		async IAsyncEnumerable<LeaseInfo> ConnectAsync(ClusterId clusterId, Requirements? requirements, string? requestId, ILogger workerLogger, [EnumeratorCancellation] CancellationToken cancellationToken)
		{
			_logger.LogDebug("Requesting compute resource");

			JsonSerializerOptions jsonSerializerOptions = new JsonSerializerOptions();
			HordeHttpClient.ConfigureJsonSerializer(jsonSerializerOptions);

			// Assign a compute worker
			HordeHttpClient client = _hordeHttpClientFactory.CreateClient();

			AssignComputeRequest request = new AssignComputeRequest();
			request.Requirements = requirements;
			request.RequestId = requestId;

			AssignComputeResponse? responseMessage;
			using (HttpResponseMessage response = await client.PostAsync($"api/v2/compute/{clusterId}", request, _cancellationSource.Token))
			{
				if (response.StatusCode == HttpStatusCode.NotFound)
				{
					throw new NoComputeAgentsFoundException(clusterId, requirements);
				}

				if (response.StatusCode == HttpStatusCode.ServiceUnavailable)
				{
					_logger.LogDebug("No compute resource is available.");
					yield break;
				}

				response.EnsureSuccessStatusCode();

				responseMessage = await response.Content.ReadFromJsonAsync<AssignComputeResponse>(jsonSerializerOptions, cancellationToken);
				if (responseMessage == null)
				{
					throw new InvalidOperationException();
				}
			}

			workerLogger.LogDebug("Connecting to {AgentId} ({Ip}) with nonce {Nonce}...", responseMessage.AgentId, responseMessage.Ip, responseMessage.Nonce);

			// Connect to the remote machine
			using Socket socket = new Socket(SocketType.Stream, ProtocolType.Tcp);
			await socket.ConnectAsync(IPAddress.Parse(responseMessage.Ip), responseMessage.Port, cancellationToken);

			// Send the nonce
			byte[] nonce = StringUtils.ParseHexString(responseMessage.Nonce);
			await socket.SendMessageAsync(nonce, SocketFlags.None, cancellationToken);
			workerLogger.LogInformation("Connected to {AgentId} ({Ip}) under lease {LeaseId}", responseMessage.AgentId, responseMessage.Ip, responseMessage.LeaseId);

			// Pass the rest of the call over to the handler
			byte[] key = StringUtils.ParseHexString(responseMessage.Key);

			await using RemoteComputeSocket computeSocket = new RemoteComputeSocket(new TcpTransport(socket), workerLogger);
			yield return new LeaseInfo(responseMessage.Properties, responseMessage.AssignedResources, computeSocket);
		}
	}

	/// <summary>
	/// Exception indicating that no matching compute agents were found
	/// </summary>
	public sealed class NoComputeAgentsFoundException : Exception
	{
		/// <summary>
		/// The compute cluster requested
		/// </summary>
		public ClusterId ClusterId { get; }

		/// <summary>
		/// Requested agent requirements
		/// </summary>
		public Requirements? Requirements { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public NoComputeAgentsFoundException(ClusterId clusterId, Requirements? requirements)
			: base($"No compute agents found matching '{requirements}' in cluster '{clusterId}'")
		{
			ClusterId = clusterId;
			Requirements = requirements;
		}
	}
}
