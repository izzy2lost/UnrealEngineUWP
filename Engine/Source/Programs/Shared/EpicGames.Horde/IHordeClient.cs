// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Storage;
using EpicGames.Horde.Tools;

namespace EpicGames.Horde
{
	/// <summary>
	/// Base interface for Horde functionality.
	/// </summary>
	public interface IHordeClient
	{
		/// <summary>
		/// Creates a http client 
		/// </summary>
		/// <returns></returns>
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
	}
}
