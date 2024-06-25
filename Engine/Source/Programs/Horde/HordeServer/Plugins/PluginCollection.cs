// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;

namespace HordeServer.Plugins
{
	/// <summary>
	/// Concrete implementation of <see cref="IPluginCollection"/>
	/// </summary>
	class PluginCollection : IPluginCollection
	{
		/// <summary>
		/// Static empty plugin collection
		/// </summary>
		public static IPluginCollection Empty { get; } = new PluginCollection(new Dictionary<string, IPluginStartup>());

		/// <inheritdoc/>
		public IReadOnlyDictionary<string, IPluginStartup> EnabledPlugins { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public PluginCollection(IReadOnlyDictionary<string, IPluginStartup> enabled)
			=> EnabledPlugins = enabled;
	}
}
