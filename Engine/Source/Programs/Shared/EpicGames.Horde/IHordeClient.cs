// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Tools;
using Grpc.Core;
using Grpc.Net.Client;

namespace EpicGames.Horde
{
	/// <summary>
	/// Base interface for Horde functionality.
	/// </summary>
	public interface IHordeClient
	{
		/// <summary>
		/// Base URL of the horde server
		/// </summary>
		Uri ServerUrl { get; }

		/// <summary>
		/// Connect to the Horde server
		/// </summary>
		/// <param name="allowLogin">Whether to allow interactive logins</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>True if the connection succeded</returns>
		Task<bool> ConnectAsync(bool allowLogin, CancellationToken cancellationToken);

		/// <summary>
		/// Gets the current connection state
		/// </summary>
		bool IsConnected();

		/// <summary>
		/// Gets a grpc channel for communication with the server. This should NOT be disposed by the caller.
		/// </summary>
		Task<GrpcChannel> GetGrpcChannelAsync(CancellationToken cancellationToken = default);

		/// <summary>
		/// Creates a Horde HTTP client 
		/// </summary>
		HordeHttpClient CreateHttpClient();

		/// <summary>
		/// Creates a storage client for the given base path
		/// </summary>
		IStorageClient CreateStorageClient(string relativePath);
	}

	/// <summary>
	/// Extension methods for <see cref="IHordeClient"/>
	/// </summary>
	public static class HordeClientExtensions
	{
		/// <summary>
		/// Creates a storage client for a particular namespace
		/// </summary>
		public static IStorageClient CreateStorageClient(this IHordeClient hordeClient, NamespaceId namespaceId)
			=> hordeClient.CreateStorageClient($"api/v1/storage/{namespaceId}");

		/// <summary>
		/// Creates a storage client for a particular tool
		/// </summary>
		public static IStorageClient CreateStorageClient(this IHordeClient hordeClient, ToolId toolId)
			=> hordeClient.CreateStorageClient($"api/v1/tools/{toolId}");

		/// <summary>
		/// Attempts to get a client reference, returning immediately if there's not one available
		/// </summary>
		public static async Task<TClient> GetGrpcClientAsync<TClient>(this IHordeClient hordeClient, CancellationToken cancellationToken = default) 
			where TClient : ClientBase<TClient>
		{
			GrpcChannel channel = await hordeClient.GetGrpcChannelAsync(cancellationToken);
			return (TClient)Activator.CreateInstance(typeof(TClient), channel)!;
		}
	}
}
