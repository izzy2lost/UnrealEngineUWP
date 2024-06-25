// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;

namespace HordeServer
{
	/// <summary>
	/// Concrete implementation of <see cref="IServerInfo"/>
	/// </summary>
	class ServerInfo : IServerInfo
	{
		/// <inheritdoc/>
		public SemVer Version => ServerApp.Version;

		/// <inheritdoc/>
		public string Environment => ServerApp.DeploymentEnvironment;

		/// <inheritdoc/>
		public string SessionId => ServerApp.SessionId;

		/// <inheritdoc/>
		public DirectoryReference AppDir => ServerApp.AppDir;

		/// <inheritdoc/>
		public DirectoryReference DataDir => ServerApp.DataDir;
	}
}
