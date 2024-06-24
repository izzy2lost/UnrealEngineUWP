// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using Microsoft.Extensions.Configuration;

namespace HordeServer
{
	/// <summary>
	/// Provides access to server deployment information
	/// </summary>
	public interface IServerInfo
	{
		/// <summary>
		/// Current version of the server
		/// </summary>
		SemVer Version { get; }

		/// <summary>
		/// Environment that the server is deployed to
		/// </summary>
		string Environment { get; }

		/// <summary>
		/// Unique session id
		/// </summary>
		string SessionId { get; }

		/// <summary>
		/// Directory containing the server executable
		/// </summary>
		DirectoryReference AppDir { get; }

		/// <summary>
		/// Directory to store server data files
		/// </summary>
		DirectoryReference DataDir { get; }
	}
}
