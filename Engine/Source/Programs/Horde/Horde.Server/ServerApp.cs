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

		public static DirectoryReference DataDir => _dataDir ?? throw new InvalidOperationException("DataDir has not been initialized");

		public static DirectoryReference ConfigDir => _configDir ?? throw new InvalidOperationException("ConfigDir has not been initialized");

		public static FileReference ServerConfigFile => _serverConfigFile ?? throw new InvalidOperationException("ServerConfigFile has not been initialized");

		public static Type[] ConfigSchemas = FindSchemaTypes();

		private static DirectoryReference _dataDir = DirectoryReference.Combine(GetAppDir(), "Data");
		private static DirectoryReference _configDir = DirectoryReference.Combine(GetAppDir(), "Defaults");
		private static FileReference? _serverConfigFile;

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

			// Create the base configuration data by just reading from the application directory. We need to check some settings before
			// being able to read user configuration files.
			IConfiguration baseConfig = CreateConfig(false, null);

			ServerSettings baseServerSettings = new ServerSettings();
			Startup.BindServerSettings(baseConfig, baseServerSettings);

			if (baseServerSettings.DataDir != null)
			{
				_dataDir = DirectoryReference.Combine(GetAppDir(), baseServerSettings.DataDir);
			}

			if (baseServerSettings.Installed)
			{
				if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows) && String.IsNullOrEmpty(baseServerSettings.DataDir))
				{
					DirectoryReference? commonDataDir = DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.CommonApplicationData);
					if (commonDataDir != null)
					{
						// Copy default config files to the C:\ProgramData\Epic\Horde\Server directory and let the user modify it there.
						_dataDir = DirectoryReference.Combine(commonDataDir, "Epic", "Horde", "Server");
					}
				}
				CopyDefaultConfigFiles(_configDir, _dataDir);
				_configDir = _dataDir;
			}

			// Create the final configuration, including the server.json file
			_serverConfigFile = FileReference.Combine(_configDir, "server.json");
			IConfiguration config = CreateConfig(baseServerSettings.Installed, _serverConfigFile);

			// Bind the complete settings
			ServerSettings serverSettings = new ServerSettings();
			Startup.BindServerSettings(config, serverSettings);

			DirectoryReference logDir = DirectoryReference.Combine(DataDir, "Logs");
			Serilog.Log.Logger = new LoggerConfiguration()
				.WithHordeConfig(serverSettings)
				.Enrich.FromLogContext()
				.Enrich.WithExceptionDetails(new DestructuringOptionsBuilder()
					.WithDefaultDestructurers()
					.WithDestructurers(new[] { new RpcExceptionDestructurer() }))
				.WriteTo.Console(serverSettings)
				.WriteTo.File(Path.Combine(logDir.FullName, "Log.txt"), outputTemplate: "[{Timestamp:HH:mm:ss} {Level:w3}] {Indent}{Message:l}{NewLine}{Exception} [{SourceContext}]", rollingInterval: RollingInterval.Day, rollOnFileSizeLimit: true, fileSizeLimitBytes: 20 * 1024 * 1024, retainedFileCountLimit: 10)
				.WriteTo.File(new JsonFormatter(renderMessage: true), Path.Combine(logDir.FullName, "Log.json"), rollingInterval: RollingInterval.Day, rollOnFileSizeLimit: true, fileSizeLimitBytes: 20 * 1024 * 1024, retainedFileCountLimit: 10)
				.ReadFrom.Configuration(config)
				.CreateLogger();

			ServiceCollection services = new ServiceCollection();
			services.AddCommandsFromAssembly(Assembly.GetExecutingAssembly());
			services.AddLogging(builder => builder.AddSerilog());
			services.AddSingleton<IConfiguration>(config);
			services.AddSingleton<ServerSettings>(serverSettings);
			services.Configure<ServerSettings>(x => Startup.BindServerSettings(config, x));

			await using (ServiceProvider serviceProvider = services.BuildServiceProvider())
			{
				return await CommandHost.RunAsync(arguments, serviceProvider, typeof(ServerCommand));
			}
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
		/// Constructs a configuration object for the current environment
		/// </summary>
		/// <returns></returns>
		static IConfiguration CreateConfig(bool readInstalledConfig, FileReference? serverConfigFile)
		{
			IConfigurationBuilder builder = new ConfigurationBuilder()
				.SetBasePath(AppDir.FullName)
				.AddJsonFile("appsettings.json", optional: false)
				.AddJsonFile("appsettings.Build.json", optional: true) // specific settings for builds (installer/dockerfile)
				.AddJsonFile($"appsettings.{DeploymentEnvironment}.json", optional: true) // environment variable overrides, also used in k8s setups with Helm
				.AddJsonFile("appsettings.User.json", optional: true);

			if (serverConfigFile != null)
			{
				builder = builder.AddJsonFile(serverConfigFile.FullName, optional: true, reloadOnChange: true);
			}
			if (readInstalledConfig)
			{
				builder = builder.Add(new RegistryConfigSource());
			}

			return builder.AddEnvironmentVariables().Build();
		}

		static void CopyDefaultConfigFiles(DirectoryReference sourceDir, DirectoryReference targetDir)
		{
			foreach (FileReference sourceFile in DirectoryReference.EnumerateFiles(sourceDir))
			{
				if (sourceFile.HasExtension(".json") || sourceFile.HasExtension(".png"))
				{
					FileReference targetFile = FileReference.Combine(targetDir, sourceFile.GetFileName());
					if (!FileReference.Exists(targetFile))
					{
						FileReference.Copy(sourceFile, targetFile);
					}
				}
			}
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
