// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde.Compute.Transports;
using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Net.Http.Json;
using System.Net.Sockets;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;

namespace EpicGames.Horde.Compute.Clients
{
	/// <summary>
	/// Handshake request message for tunneling server
	/// </summary>
	/// <param name="Host">Target host to relay traffic to/from</param>
	/// <param name="Port">Target port</param>
	public record TunnelHandshakeRequest(string Host, int Port)
	{
		const int Version = 1;
		const string Name = "HANDSHAKE-REQ";
		
		/// <summary>
		/// Serialize the message
		/// </summary>
		/// <returns>A string based representation</returns>
		public string Serialize()
		{
			return $"{Name}\t{Version}\t{Host}\t{Port}";
		}
	
		/// <summary>
		/// Deserialize the message
		/// </summary>
		/// <param name="text">A raw string to deserialize</param>
		/// <returns>A request message</returns>
		/// <exception cref="Exception"></exception>
		public static TunnelHandshakeRequest Deserialize(string? text)
		{
			string[] parts = (text ?? "").Split('\t');
			if (parts.Length != 4 || parts[0] != Name || !Int32.TryParse(parts[1], out int _) || !Int32.TryParse(parts[3], out int port))
			{
				throw new Exception("Failed deserializing handshake request. Content: " + text);
			}
			return new TunnelHandshakeRequest(parts[2], port);
		}
	}
	
	/// <summary>
	/// Handshake response message for tunneling server
	/// </summary>
	/// <param name="IsSuccess">Whether successful or not</param>
	/// <param name="Message">Message with additional information describing the outcome</param>
	public record TunnelHandshakeResponse(bool IsSuccess, string Message)
	{
		const int Version = 1;
		const string Name = "HANDSHAKE-RES";
		
		/// <summary>
		/// Serialize the message
		/// </summary>
		/// <returns>A string based representation</returns>
		public string Serialize()
		{
			return $"{Name}\t{Version}\t{IsSuccess}\t{Message}";
		}
	
		/// <summary>
		/// Deserialize the message
		/// </summary>
		/// <param name="text">A raw string to deserialize</param>
		/// <returns>A request message</returns>
		/// <exception cref="Exception"></exception>
		public static TunnelHandshakeResponse Deserialize(string? text)
		{
			string[] parts = (text ?? "").Split('\t');
			if (parts.Length != 4 || !Int32.TryParse(parts[1], out int _) || !Boolean.TryParse(parts[2], out bool isSuccess))
			{
				throw new Exception("Failed deserializing handshake response. Content: " + text);
			}
			return new TunnelHandshakeResponse(isSuccess, parts[3]);
		}
	}
	
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

		readonly IHttpClientFactory _httpClientFactory;
		readonly CancellationTokenSource _cancellationSource = new CancellationTokenSource();
		readonly string _sessionId;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="httpClientFactory">Factory for constructing http client instances</param>
		/// <param name="logger">Logger for diagnostic messages</param>
		public ServerComputeClient(IHttpClientFactory httpClientFactory, ILogger logger) : this(httpClientFactory, null, logger)
		{
		}
		
		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="httpClientFactory">Factory for constructing http client instances</param>
		/// <param name="sessionId">Arbitrary ID used for identifying this compute client. If not provided, a random one will be generated</param>
		/// <param name="logger">Logger for diagnostic messages</param>
		public ServerComputeClient(IHttpClientFactory httpClientFactory, string? sessionId, ILogger logger)
		{
			_httpClientFactory = httpClientFactory;
			_sessionId = sessionId ?? Guid.NewGuid().ToString();
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
		public async Task<IComputeLease?> TryAssignWorkerAsync(ClusterId clusterId, Requirements? requirements, string? requestId, ConnectionMetadataRequest? connection, ILogger logger, CancellationToken cancellationToken)
		{
			IAsyncEnumerator<LeaseInfo> source = ConnectAsync(clusterId, requirements, requestId, connection, logger, cancellationToken).GetAsyncEnumerator(cancellationToken);
			if (!await source.MoveNextAsync())
			{
				await source.DisposeAsync();
				return null;
			}
			return new LeaseImpl(source);
		}

		/// <inheritdoc/>
		public async Task DeclareResourceNeedsAsync(ClusterId clusterId, string pool, Dictionary<string, int> resourceNeeds, CancellationToken cancellationToken = default)
		{
			HttpClient client = _httpClientFactory.CreateClient(HordeHttpClient.HttpClientName);
			ResourceNeedsMessage request = new () { SessionId = _sessionId, Pool = pool, ResourceNeeds = resourceNeeds };
			using HttpResponseMessage response = await HordeHttpClient.PostAsync(client, $"api/v2/compute/{clusterId}/resource-needs", request, _cancellationSource.Token);
			response.EnsureSuccessStatusCode();
		}

		async IAsyncEnumerable<LeaseInfo> ConnectAsync(ClusterId clusterId, Requirements? requirements, string? requestId, ConnectionMetadataRequest? connection, ILogger workerLogger, [EnumeratorCancellation] CancellationToken cancellationToken)
		{
			_logger.LogDebug("Requesting compute resource");

			// Assign a compute worker
			HttpClient client = _httpClientFactory.CreateClient(HordeHttpClient.HttpClientName);

			AssignComputeRequest request = new AssignComputeRequest();
			request.Requirements = requirements;
			request.RequestId = requestId;
			request.Connection = connection;

			AssignComputeResponse? responseMessage;
			using (HttpResponseMessage response = await HordeHttpClient.PostAsync(client, $"api/v2/compute/{clusterId}", request, _cancellationSource.Token))
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

				responseMessage = await response.Content.ReadFromJsonAsync<AssignComputeResponse>(HordeHttpClient.JsonSerializerOptions, cancellationToken);
				if (responseMessage == null)
				{
					throw new InvalidOperationException();
				}
			}

			string agentAddress = $"{responseMessage.Ip}:{responseMessage.Port}";

			// Connect to the remote machine
			using Socket socket = new Socket(SocketType.Stream, ProtocolType.Tcp);
			
			workerLogger.LogDebug("Connecting to {AgentId} at {AgentAddress} ({ConnectionType} via {ConnectionAddress}) with nonce {Nonce}...", responseMessage.AgentId, agentAddress, responseMessage.ConnectionMode, responseMessage.ConnectionAddress ?? "None", responseMessage.Nonce);
			switch (responseMessage.ConnectionMode)
			{
				case ConnectionMode.Direct:
					await socket.ConnectAsync(IPAddress.Parse(responseMessage.Ip), responseMessage.Port, cancellationToken);
					break;

				case ConnectionMode.Tunnel when !String.IsNullOrEmpty(responseMessage.ConnectionAddress):
					(string host, int port) = ParseHostPort(responseMessage.ConnectionAddress);
					await socket.ConnectAsync(host, port, cancellationToken);
					await TunnelHandshakeAsync(socket, responseMessage, cancellationToken);
					break;
				
				case ConnectionMode.Relay when !String.IsNullOrEmpty(responseMessage.ConnectionAddress):
					throw new NotImplementedException("Relay connection mode not yet implemented");
				
				default:
					throw new Exception($"Unable to resolve connection mode ({responseMessage.ConnectionMode}/{responseMessage.ConnectionAddress}");
			}

			// Send the nonce
			byte[] nonce = StringUtils.ParseHexString(responseMessage.Nonce);
			await socket.SendMessageAsync(nonce, SocketFlags.None, cancellationToken);
			workerLogger.LogInformation("Connected to {AgentId} ({Ip}) under lease {LeaseId}", responseMessage.AgentId, responseMessage.Ip, responseMessage.LeaseId);

			// Pass the rest of the call over to the handler
			byte[] key = StringUtils.ParseHexString(responseMessage.Key);

			await using RemoteComputeSocket computeSocket = new RemoteComputeSocket(new TcpTransport(socket), workerLogger);
			yield return new LeaseInfo(responseMessage.Properties, responseMessage.AssignedResources, computeSocket);
		}

		private static (string host, int port) ParseHostPort(string address)
		{
			try
			{
				string[] parts = address.Split(":");
				string host = parts[0];
				int port = Int32.Parse(parts[1]);
				return (host, port);
			}
			catch (Exception e)
			{
				throw new Exception($"Unable to parse host and port for address: {address}", e);
			}
		}

		private static async Task TunnelHandshakeAsync(Socket socket, AssignComputeResponse response, CancellationToken cancellationToken)
		{
			await using NetworkStream ns = new (socket, false);
			using StreamReader reader = new (ns);
			await using StreamWriter writer = new (ns) { AutoFlush = true };

			string request = new TunnelHandshakeRequest(response.Ip, response.Port).Serialize();
			await writer.WriteLineAsync(request.ToCharArray(), cancellationToken);

			string exceptionMetadata = $"Connection: {response.ConnectionAddress} Target: {response.Ip}:{response.Port}";
			Task<string?> readTask = reader.ReadLineAsync();
			Task timeoutTask = Task.Delay(15000, cancellationToken);
			if (await Task.WhenAny(readTask, timeoutTask) == timeoutTask)
			{
				throw new TimeoutException($"Timed out reading tunnel handshake response. {exceptionMetadata}");
			}

			TunnelHandshakeResponse handshakeResponse = TunnelHandshakeResponse.Deserialize(await readTask);
			if (!handshakeResponse.IsSuccess)
			{
				throw new Exception($"Tunnel handshake failed! Reason: {handshakeResponse.Message} {exceptionMetadata}");
			}
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
