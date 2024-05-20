// Copyright Epic Games, Inc. All Rights Reserved.

using System.Reflection;
using EpicGames.Core;
using EpicGames.Horde;
using JobDriver.Execution;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Serilog;
using Serilog.Extensions.Logging;
using Serilog.Formatting.Json;

namespace JobDriver
{
	class DriverApp
	{
		public static async Task<int> Main(string[] args)
		{
			CommandLineArguments arguments = new CommandLineArguments(args);

			// Read the driver config
			IConfiguration configuration = new ConfigurationBuilder()
				.AddJsonFile("appsettings.json", optional: false)
				.AddEnvironmentVariables()
				.Build();

			// Create the services 
			IServiceCollection services = new ServiceCollection();
			services.AddOptions<DriverSettings>().Configure(options => configuration.GetSection("Driver").Bind(options)).ValidateDataAnnotations();
			services.AddLogging(builder => builder.AddEpicDefault());
			services.AddHorde(options => options.AllowAuthPrompt = false);

			services.AddSingleton<IJobExecutorFactory, PerforceExecutorFactory>();
			services.AddSingleton<IJobExecutorFactory, WorkspaceExecutorFactory>();
			services.AddSingleton<IJobExecutorFactory, LocalExecutorFactory>();
			services.AddSingleton<IJobExecutorFactory, TestExecutorFactory>();

			services.AddSingleton<IWorkspaceMaterializerFactory, WorkspaceMaterializerFactory>();

			services.AddCommandsFromAssembly(Assembly.GetExecutingAssembly());

			// Run the host
			await using ServiceProvider serviceProvider = services.BuildServiceProvider();
			return await CommandHost.RunAsync(arguments, serviceProvider, null);
		}

		public static ILoggerProvider CreateLoggerProvider(IConfiguration configuration)
		{
			//			ConsoleTheme theme;
			//			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows) && Environment.OSVersion.Version < new Version(10, 0))
			//			{
			//				theme = SystemConsoleTheme.Literate;
			//			}
			//			else
			//			{
			//				theme = AnsiConsoleTheme.Code;
			//			}

			LoggerConfiguration loggerConfiguration = new LoggerConfiguration()
				.WriteTo.Console(new JsonFormatter(renderMessage: true))
				.ReadFrom.Configuration(configuration)
				.Enrich.FromLogContext();

			return new SerilogLoggerProvider(loggerConfiguration.CreateLogger());
		}
	}
}
