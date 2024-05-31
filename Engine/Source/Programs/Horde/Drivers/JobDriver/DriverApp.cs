// Copyright Epic Games, Inc. All Rights Reserved.

using System.Reflection;
using EpicGames.Core;
using EpicGames.Horde;
using JobDriver.Execution;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;

namespace JobDriver
{
	/// <summary>
	/// Application class for the job driver
	/// </summary>
	public static class DriverApp
	{
		/// <summary>
		/// Main entry point
		/// </summary>
		public static async Task<int> Main(string[] args)
		{
			CommandLineArguments arguments = new CommandLineArguments(args);

			// Create the services 
			IServiceCollection services = new ServiceCollection();
			RegisterServices(services);

			// Run the host
			await using ServiceProvider serviceProvider = services.BuildServiceProvider();
			return await CommandHost.RunAsync(arguments, serviceProvider, null);
		}

		/// <summary>
		/// Helper method to register services for this app
		/// </summary>
		/// <param name="services"></param>
		public static void RegisterServices(IServiceCollection services)
		{
			// Read the driver config
			IConfiguration configuration = new ConfigurationBuilder()
				.AddJsonFile("appsettings.json", optional: false)
				.AddJsonFile("appsettings.Epic.json", optional: true)
				.AddEnvironmentVariables()
				.Build();

			// Register the services
			services.AddOptions<DriverSettings>().Configure(options => configuration.GetSection("Driver").Bind(options)).ValidateDataAnnotations();
			services.AddLogging(builder => builder.AddEpicDefault());
			services.AddHorde(options => options.AllowAuthPrompt = false);

			services.AddSingleton<IJobExecutorFactory, PerforceExecutorFactory>();
			services.AddSingleton<IJobExecutorFactory, WorkspaceExecutorFactory>();
			services.AddSingleton<IJobExecutorFactory, LocalExecutorFactory>();
			services.AddSingleton<IJobExecutorFactory, TestExecutorFactory>();

			services.AddSingleton<IWorkspaceMaterializerFactory, ManagedWorkspaceMaterializerFactory>();

			services.AddCommandsFromAssembly(Assembly.GetExecutingAssembly());
		}
	}
}
