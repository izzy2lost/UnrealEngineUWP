// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using Microsoft.CodeAnalysis.CSharp.Syntax;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;

namespace HordeServer.Plugins
{
	/// <summary>
	/// Interface for modules that can extend the services known by Horde
	/// </summary>
	public interface IPluginStartup
	{
		/// <summary>
		/// Configure the services provided by the plugin
		/// </summary>
		/// <param name="serviceCollection">Collection of services to add to</param>
		void ConfigureServices(IServiceCollection serviceCollection);
	}
}
