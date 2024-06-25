// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Reflection;
using Microsoft.Extensions.DependencyInjection;

namespace HordeServer.Plugins
{
	/// <summary>
	/// Concrete implementation of <see cref="IPluginCollection"/>
	/// </summary>
	class PluginCollection : IPluginCollection
	{
		// Plugin which is compiled into the application or already loaded
		class StaticPlugin : IPlugin
		{
			public PluginName Name => _metadata.Name;
			public IPluginMetadata Metadata => _metadata;

			readonly IPluginMetadata _metadata;
			readonly ILoadedPlugin _loadedPlugin;

			public StaticPlugin(IPluginMetadata metadata, ILoadedPlugin loadedPlugin)
			{
				_metadata = metadata;
				_loadedPlugin = loadedPlugin;
			}

			public ILoadedPlugin Load()
				=> _loadedPlugin;
		}

		class LoadedPlugin<TServerConfig, TGlobalConfig, TStartup> : ILoadedPlugin
			where TGlobalConfig : class, IPluginConfig, new()
			where TStartup : class, IPluginStartup, new()
		{
			readonly IPluginMetadata _metadata;

			public PluginName Name => _metadata.Name;
			public IPluginMetadata Metadata => _metadata;
			public Assembly Assembly => typeof(TStartup).Assembly;
			public Type ServerConfigType => typeof(TServerConfig);
			public Type GlobalConfigType => typeof(TGlobalConfig);

			public LoadedPlugin(IPluginMetadata metadata)
				=> _metadata = metadata;

			public void ConfigureServices(IServiceCollection serviceCollection)
			{
				serviceCollection.AddPluginConfig<TGlobalConfig>(Name);

				TStartup startup = new TStartup();
				startup.ConfigureServices(serviceCollection);
			}

			public ILoadedPlugin Load()
				=> this;
		}

		/// <inheritdoc/>
		public IReadOnlyList<IPlugin> Plugins => _plugins;

		/// <inheritdoc/>
		public IReadOnlyList<ILoadedPlugin> LoadedPlugins => _loadedPlugins;

		readonly List<IPlugin> _plugins = new List<IPlugin>();
		readonly List<ILoadedPlugin> _loadedPlugins = new List<ILoadedPlugin>();
		readonly HashSet<PluginName> _loadedPluginNames = new HashSet<PluginName>();

		/// <summary>
		/// Adds a plugin with the given startup class
		/// </summary>
		ILoadedPlugin AddLoadedPlugin(IPluginMetadata metadata, Type startupType)
		{
			if (!_loadedPluginNames.Add(metadata.Name))
			{
				throw new NotImplementedException();
			}

			PluginAttribute attr = startupType.GetCustomAttribute<PluginAttribute>()
				?? throw new InvalidOperationException($"Cannot add {startupType.Name} as a plugin. No {nameof(PluginAttribute)} was found.");

			Type pluginType = typeof(LoadedPlugin<,,>).MakeGenericType(attr.ServerConfigType ?? typeof(object), attr.GlobalConfigType ?? typeof(object), startupType);
			ILoadedPlugin loadedPlugin = (ILoadedPlugin)Activator.CreateInstance(pluginType, metadata)!;
			_loadedPlugins.Add(loadedPlugin);

			return loadedPlugin;
		}

		/// <summary>
		/// Adds a plugin with the given startup class
		/// </summary>
		/// <typeparam name="T">Type of the startup class</typeparam>
		public ILoadedPlugin Add<T>(IPluginMetadata metadata) where T : class, IPluginStartup
		{
			ILoadedPlugin plugin = AddLoadedPlugin(metadata, typeof(T));
			_plugins.Add(plugin);
			return plugin;
		}
	}
}
