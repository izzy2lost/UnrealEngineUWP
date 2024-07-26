// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Artifacts;
using EpicGames.Horde.Compute;
using EpicGames.Horde.Logs;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Tools;
using Grpc.Core;
using Grpc.Net.Client;
using Microsoft.Extensions.Logging;

namespace EpicGames.Horde
{
	/// <summary>
	/// Base interface for Horde functionality.
	/// </summary>
	public interface IHordeClient : IAsyncDisposable
	{
		/// <summary>
		/// Base URL of the horde server
		/// </summary>
		Uri ServerUrl { get; }

		/// <summary>
		/// Accessor for the artifact collection
		/// </summary>
		IArtifactCollection Artifacts { get; }

		/// <summary>
		/// Connect to the Horde server
		/// </summary>
		/// <param name="allowPrompt">Whether to allow prompting for credentials</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>True if the connection succeded</returns>
		Task<bool> LoginAsync(bool allowPrompt, CancellationToken cancellationToken);

		/// <summary>
		/// Gets the current connection state
		/// </summary>
		bool HasValidAccessToken();

		/// <summary>
		/// Gets an access token for the server
		/// </summary>
		Task<string?> GetAccessTokenAsync(bool interactive, CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets a grpc channel for communication with the server. This should NOT be disposed by the caller.
		/// </summary>
		Task<GrpcChannel> CreateGrpcChannelAsync(CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets a gRPC client interface
		/// </summary>
		Task<TClient> CreateGrpcClientAsync<TClient>(CancellationToken cancellationToken = default) where TClient : ClientBase<TClient>;

		/// <summary>
		/// Creates a Horde HTTP client 
		/// </summary>
		HordeHttpClient CreateHttpClient();

		/// <summary>
		/// Creates a compute client
		/// </summary>
		IComputeClient CreateComputeClient();

		/// <summary>
		/// Creates a storage client for the given base path
		/// </summary>
		IStorageClient CreateStorageClient(string relativePath, string? accessToken = null);

		/// <summary>
		/// Creates a logger device that writes data to the server
		/// </summary>
		IServerLogger CreateServerLogger(LogId logId, LogLevel minimumLevel = LogLevel.Information);
	}

	/// <summary>
	/// Interface to allow creating custom horde client instances. To obtain a default horde client, get an IHordeClient instance via dependency injection.
	/// </summary>
	public interface IHordeClientFactory
	{
		/// <summary>
		/// Create a client using the user's default access token
		/// </summary>
		IHordeClient Create();

		/// <summary>
		/// Create a client with an explicit access token
		/// </summary>
		IHordeClient Create(string accessToken);
	}

	/// <summary>
	/// Extension methods for <see cref="IHordeClient"/>
	/// </summary>
	public static class HordeClientExtensions
	{
		/// <summary>
		/// Creates a storage client for a particular namespace
		/// </summary>
		public static IStorageClient CreateStorageClient(this IHordeClient hordeClient, NamespaceId namespaceId, string? accessToken = null)
			=> hordeClient.CreateStorageClient($"api/v1/storage/{namespaceId}", accessToken);

		/// <summary>
		/// Creates a storage client for a particular artifact
		/// </summary>
		public static IStorageClient CreateStorageClient(this IHordeClient hordeClient, ArtifactId artifactId)
			=> hordeClient.CreateStorageClient($"api/v2/artifacts/{artifactId}");

		/// <summary>
		/// Creates a storage client for a particular log
		/// </summary>
		public static IStorageClient CreateStorageClient(this IHordeClient hordeClient, LogId logId)
			=> hordeClient.CreateStorageClient($"api/v1/logs/{logId}");

		/// <summary>
		/// Creates a storage client for a particular tool
		/// </summary>
		public static IStorageClient CreateStorageClient(this IHordeClient hordeClient, ToolId toolId)
			=> hordeClient.CreateStorageClient($"api/v1/tools/{toolId}");

	}
}
