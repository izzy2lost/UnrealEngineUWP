// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Versioning;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Server.Commands;
using Horde.Server.Utilities;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Win32;
using Serilog;
using Serilog.Configuration;
using Serilog.Exceptions;
using Serilog.Exceptions.Core;
using Serilog.Exceptions.Grpc.Destructurers;
using Serilog.Formatting.Json;
using Serilog.Sinks.SystemConsole.Themes;

namespace Horde.Server
{
	static class LoggerExtensions
	{
		public static LoggerConfiguration Console(this LoggerSinkConfiguration sinkConfig, ServerSettings settings)
		{
			if (settings.LogJsonToStdOut)
			{
				return sinkConfig.Console(new JsonFormatter(renderMessage: true));
			}
			else
			{
				ConsoleTheme theme;
				if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows) && Environment.OSVersion.Version < new Version(10, 0))
				{
					theme = SystemConsoleTheme.Literate;
				}
				else
				{
					theme = AnsiConsoleTheme.Code;
				}
				return sinkConfig.Console(outputTemplate: "[{Timestamp:HH:mm:ss} {Level:w3}] {Indent}{Message:l}{NewLine}{Exception}", theme: theme, restrictedToMinimumLevel: Serilog.Events.LogEventLevel.Debug);
			}
		}

		public static LoggerConfiguration WithHordeConfig(this LoggerConfiguration configuration, ServerSettings settings)
		{
			if (settings.OpenTelemetry.EnableDatadogCompatibility)
			{
				configuration = configuration.Enrich.With<OpenTelemetryDatadogLogEnricher>();
			}

			return configuration;
		}
	}

	class ServerApp
	{
		public static SemVer Version { get; } = GetVersion();

		public static string DeploymentEnvironment { get; } = GetEnvironment();

		public static string SessionId { get; } = Guid.NewGuid().ToString("n");

		public static DirectoryReference AppDir { get; } = GetAppDir();

		public static DirectoryReference DataDir { get; } = GetDataDir();

		public static FileReference UserConfigFile { get; } = FileReference.Combine(DataDir, "Horde.json");

		public static Type[] ConfigSchemas = FindSchemaTypes();

		static Type[] FindSchemaTypes()
		{
			List<Type> schemaTypes = new List<Type>();
			foreach (Type type in Assembly.GetExecutingAssembly().GetTypes())
			{
				if (type.GetCustomAttribute<JsonSchemaAttribute>() != null)
				{
					schemaTypes.Add(type);
				}
			}
			return schemaTypes.ToArray();
		}

		static SemVer GetVersion()
		{
			FileVersionInfo versionInfo = FileVersionInfo.GetVersionInfo(Assembly.GetExecutingAssembly().Location);
			if (String.IsNullOrEmpty(versionInfo.ProductVersion))
			{
				return SemVer.Parse("0.0.0");
			}
			else
			{
				return SemVer.Parse(versionInfo.ProductVersion);
			}
		}

		public static async Task<int> Main(string[] args)
		{
			CommandLineArguments arguments = new CommandLineArguments(args);

			IConfiguration config = CreateConfig(UserConfigFile);

			ServerSettings hordeSettings = new ServerSettings();
			Startup.BindServerSettings(config, hordeSettings);

			DirectoryReference logDir = AppDir;
			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				logDir = DirectoryReference.Combine(DataDir);
			}

			Serilog.Log.Logger = new LoggerConfiguration()
				.WithHordeConfig(hordeSettings)
				.Enrich.FromLogContext()
				.Enrich.WithExceptionDetails(new DestructuringOptionsBuilder()
					.WithDefaultDestructurers()
					.WithDestructurers(new[] { new RpcExceptionDestructurer() }))
				.WriteTo.Console(hordeSettings)
				.WriteTo.File(Path.Combine(logDir.FullName, "Log.txt"), outputTemplate: "[{Timestamp:HH:mm:ss} {Level:w3}] {Indent}{Message:l}{NewLine}{Exception} [{SourceContext}]", rollingInterval: RollingInterval.Day, rollOnFileSizeLimit: true, fileSizeLimitBytes: 20 * 1024 * 1024, retainedFileCountLimit: 10)
				.WriteTo.File(new JsonFormatter(renderMessage: true), Path.Combine(logDir.FullName, "Log.json"), rollingInterval: RollingInterval.Day, rollOnFileSizeLimit: true, fileSizeLimitBytes: 20 * 1024 * 1024, retainedFileCountLimit: 10)
				.ReadFrom.Configuration(config)
				.CreateLogger();

			ServiceCollection services = new ServiceCollection();
			services.AddCommandsFromAssembly(Assembly.GetExecutingAssembly());
			services.AddLogging(builder => builder.AddSerilog());
			services.AddSingleton<IConfiguration>(config);
			services.AddSingleton<ServerSettings>(hordeSettings);
			services.Configure<ServerSettings>(x => Startup.BindServerSettings(config, x));

#pragma warning disable ASP0000 // Do not call 'IServiceCollection.BuildServiceProvider' in 'ConfigureServices'
			await using ServiceProvider serviceProvider = services.BuildServiceProvider();
			return await CommandHost.RunAsync(arguments, serviceProvider, typeof(ServerCommand));
#pragma warning restore ASP0000 // Do not call 'IServiceCollection.BuildServiceProvider' in 'ConfigureServices'
		}

		// Used by WebApplicationFactory in controller tests. Uses reflection to call this exact function signature.
		public static IHostBuilder CreateHostBuilder(string[] args) => ServerCommand.CreateHostBuilderForTesting(args);

		/// <summary>
		/// Gets the current environment
		/// </summary>
		/// <returns></returns>
		static string GetEnvironment()
		{
			string? environment = Environment.GetEnvironmentVariable("ASPNETCORE_ENVIRONMENT");
			if (String.IsNullOrEmpty(environment))
			{
				environment = "Production";
			}
			return environment;
		}

		/// <summary>
		/// Get the application directory
		/// </summary>
		/// <returns></returns>
		static DirectoryReference GetAppDir()
		{
			return new FileReference(Assembly.GetExecutingAssembly().Location).Directory;
		}

		/// <summary>
		/// Gets the default directory for storing application data
		/// </summary>
		/// <returns>The default data directory</returns>
		static DirectoryReference GetDataDir()
		{
			IConfiguration config = CreateConfig(null);

			string? dataDir = config.GetSection("Horde").GetValue(typeof(string), nameof(ServerSettings.DataDir)) as string;
			if (dataDir != null)
			{
				return DirectoryReference.Combine(GetAppDir(), dataDir);
			}

			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				DirectoryReference? dir = DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.CommonApplicationData);
				if (dir != null)
				{
					return DirectoryReference.Combine(dir, "HordeServer");
				}
			}
			return DirectoryReference.Combine(GetAppDir(), "Data");
		}

		/// <summary>
		/// Constructs a configuration object for the current environment
		/// </summary>
		/// <param name="userConfigFile"></param>
		/// <returns></returns>
		static IConfiguration CreateConfig(FileReference? userConfigFile)
		{
			IConfigurationBuilder builder = new ConfigurationBuilder()
				.SetBasePath(AppDir.FullName)
				.Add(new RegistryConfigSource())
				.AddJsonFile("appsettings.json", optional: false)
				.AddJsonFile("appsettings.Build.json", optional: true) // specific settings for builds (installer/dockerfile)
				.AddJsonFile($"appsettings.{DeploymentEnvironment}.json", optional: true) // environment variable overrides, also used in k8s setups with Helm
				.AddJsonFile("appsettings.User.json", optional: true);

			if (userConfigFile != null)
			{
				builder = builder.AddJsonFile(userConfigFile.FullName, optional: true, reloadOnChange: true);
			}

			return builder.AddEnvironmentVariables().Build();
		}

		class RegistryConfigSource : IConfigurationSource
		{
			public IConfigurationProvider Build(IConfigurationBuilder builder)
				=> new RegistryConfigProvider();
		}

		class RegistryConfigProvider : ConfigurationProvider
		{
			public override void Load()
			{
				if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
				{
					Dictionary<string, string?> data = new Dictionary<string, string?>();
					GetValues(Registry.LocalMachine, "SOFTWARE\\Epic Games\\Horde\\Server", ServerSettings.SectionName, data);
					Data = data;
				}
			}

			[SupportedOSPlatform("windows")]
			static void GetValues(RegistryKey baseKey, string keyPath, string baseConfigName, Dictionary<string, string?> data)
			{
				using RegistryKey? registryKey = baseKey.OpenSubKey(keyPath);
				if (registryKey != null)
				{
					string[] subKeyNames = registryKey.GetSubKeyNames();
					foreach (string subKeyName in subKeyNames)
					{
						GetValues(registryKey, subKeyName, $"{baseConfigName}:{subKeyName}", data);
					}

					string[] valueNames = registryKey.GetValueNames();
					foreach (string valueName in valueNames)
					{
						object? value = registryKey.GetValue(valueName);
						if (value != null)
						{
							data[$"{baseConfigName}:{valueName}"] = value.ToString();
						}
					}
				}
			}
		}
	}
}
