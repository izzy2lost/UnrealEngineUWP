// Copyright Epic Games, Inc. All Rights Reserved.

using System.Reflection;
using EpicGames.Core;
using Microsoft.Extensions.DependencyInjection;

namespace Horde.Agent.Driver
{
	class DriverApp
	{
		public static async Task<int> MainAsync(string[] args)
		{
			CommandLineArguments arguments = new CommandLineArguments(args);

			IServiceCollection services = new ServiceCollection();
			services.AddCommandsFromAssembly(Assembly.GetExecutingAssembly());

			await using ServiceProvider serviceProvider = services.BuildServiceProvider();
			return await CommandHost.RunAsync(arguments, serviceProvider, null);
		}
	}
}
