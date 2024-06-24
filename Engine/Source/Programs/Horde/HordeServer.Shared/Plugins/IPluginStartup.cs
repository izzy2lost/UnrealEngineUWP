// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using Microsoft.Extensions.DependencyInjection;

namespace HordeServer.Plugins
{
	/// <summary>
	/// Attribute used to identify types that should be constructed for the given plugin name
	/// </summary>
	[AttributeUsage(AttributeTargets.Class)]
	public sealed class PluginStartupAttribute : Attribute
	{
		/// <summary>
		/// Name of the plugin
		/// </summary>
		public string Name { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public PluginStartupAttribute(string name)
			=> Name = name;
	}

	/// <summary>
	/// Interface for modules that can extend the services known by Horde
	/// </summary>
	public interface IPluginStartup
	{
		/// <summary>
		/// Type containing global settings for this plugin. Must implement the <see cref="IPluginConfig"/> interface.
		/// 
		/// Standard <see cref="Microsoft.Extensions.Options.IOptions{TOptions}"/> and 
		/// <see cref="Microsoft.Extensions.Options.IOptionsMonitor{TOptions}"/> objects will be registered for this type 
		/// in the service container.
		/// </summary>
		Type? GlobalConfigType { get; }

		/// <summary>
		/// Configure the services provided by the plugin
		/// </summary>
		/// <param name="serviceCollection">Collection of services to add to</param>
		void ConfigureServices(IServiceCollection serviceCollection);
	}
}
