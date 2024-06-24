// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using HordeServer.Server;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Options;
using Microsoft.Extensions.Primitives;

namespace HordeServer.Plugins
{
	/// <summary>
	/// Factory for accessing plugin config instances
	/// </summary>
	/// <typeparam name="T">Type of the options object to return</typeparam>
	sealed class PluginOptionsFactory<T> : IOptionsFactory<T>, IOptionsChangeTokenSource<T>
		where T : class, IPluginConfig, new()
	{
		readonly string _pluginName;
		readonly IOptionsMonitor<GlobalConfig> _globalConfig;
		readonly IOptionsChangeTokenSource<GlobalConfig> _globalConfigChangeTokenSource;

		/// <inheritdoc/>
		string IOptionsChangeTokenSource<T>.Name => String.Empty;

		/// <summary>
		/// Constructor
		/// </summary>
		public PluginOptionsFactory(string pluginName, IServiceProvider serviceProvider)
			: this(pluginName, serviceProvider.GetRequiredService<IOptionsMonitor<GlobalConfig>>(), serviceProvider.GetRequiredService<IOptionsChangeTokenSource<GlobalConfig>>())
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public PluginOptionsFactory(string pluginName, IOptionsMonitor<GlobalConfig> globalConfig, IOptionsChangeTokenSource<GlobalConfig> globalConfigChangeTokenSource)
		{
			_pluginName = pluginName;
			_globalConfig = globalConfig;
			_globalConfigChangeTokenSource = globalConfigChangeTokenSource;
		}

		/// <inheritdoc/>
		public T Create(string name)
		{
			T? config;
			if (!_globalConfig.CurrentValue.Plugins.TryGetValue<T>(_pluginName, out config))
			{
				config = new T();
			}
			return config;
		}

		/// <inheritdoc/>
		public IChangeToken GetChangeToken()
			=> _globalConfigChangeTokenSource.GetChangeToken();
	}

	/// <summary>
	/// Helper methods for registering 
	/// </summary>
	public static class PluginOptionsFactoryExtensions
	{
		/// <summary>
		/// Register an options factory for the given plugin options type
		/// </summary>
		public static void AddPluginConfig(this IServiceCollection services, string name, Type configType)
		{
			Type helperType = typeof(RegistrationHelper<>).MakeGenericType(configType);
			RegistrationHelper registration = (RegistrationHelper)Activator.CreateInstance(helperType)!;
			registration.Register(services, name);
		}

		abstract class RegistrationHelper
		{
			public abstract void Register(IServiceCollection services, string name);
		}

		class RegistrationHelper<T> : RegistrationHelper where T : class, IPluginConfig, new()
		{
			public override void Register(IServiceCollection services, string name)
			{
				services.AddSingleton<PluginOptionsFactory<T>>(sp => new PluginOptionsFactory<T>(name, sp));
				services.AddSingleton<IOptionsFactory<T>>(sp => sp.GetRequiredService<PluginOptionsFactory<T>>());
				services.AddSingleton<IOptionsChangeTokenSource<T>>(sp => sp.GetRequiredService<PluginOptionsFactory<T>>());
			}
		}
	}
}
