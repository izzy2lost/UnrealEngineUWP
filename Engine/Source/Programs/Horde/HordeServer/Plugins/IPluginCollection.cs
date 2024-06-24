// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;

namespace HordeServer.Plugins
{
	/// <summary>
	/// Interface for querying the state of plugins on the server
	/// </summary>
	public interface IPluginCollection
	{
		/// <summary>
		/// The enabled plugins
		/// </summary>
		IReadOnlyDictionary<string, IPluginStartup> EnabledPlugins { get; }
	}
}
