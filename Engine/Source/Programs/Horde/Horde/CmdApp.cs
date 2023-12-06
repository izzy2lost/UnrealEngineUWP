// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using System.Reflection;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using System.Runtime.InteropServices;
using Serilog.Sinks.SystemConsole.Themes;
using Serilog;
using Serilog.Formatting.Json;
using EpicGames.Horde.Storage;
using Microsoft.Extensions.Options;
using EpicGames.Horde;
using EpicGames.Horde.Storage.Clients;

namespace Horde
{
	class CmdApp
	{
		const string ToolDescription = "Horde Command-Line Tool";

		static DirectoryReference DataDir { get; } = GetDataDir();

		static async Task<int> Main(string[] args)
		{
			CommandLineArguments arguments = new CommandLineArguments(args);
			IConfiguration configuration = new ConfigurationBuilder()
				.AddJsonFile("appsettings.json", optional: false)
				.AddJsonFile("appsettings.User.json", optional: true)
				.AddEnvironmentVariables()
				.Build();

			using ILoggerFactory loggerFactory = CreateLoggerFactory(configuration, arguments.HasOption("-Quiet"));

			IServiceCollection services = new ServiceCollection();
			services.AddCommandsFromAssembly(Assembly.GetExecutingAssembly());
			services.AddSingleton(loggerFactory);
			services.AddLogging();
			services.AddMemoryCache();
			services.AddSingleton(sp => Options.Create(CmdConfig.Read()));
			services.AddHordeHttpClient((sp, client) => client.BaseAddress = sp.GetRequiredService<IOptions<CmdConfig>>().Value.Server);
			services.AddSingleton<BundleCache>(CreateStorageClientCache);
			services.AddSingleton<StorageBackendCache>(CreateStorageBackendCache);
			services.AddSingleton<HttpStorageClientFactory>();

			// Execute all the commands
			await using ServiceProvider serviceProvider = services.BuildServiceProvider();
			return await CommandHost.RunAsync(arguments, serviceProvider, null, ToolDescription);
		}

		static BundleCache CreateStorageClientCache(IServiceProvider serviceProvider)
		{
			CmdConfig cmdConfig = serviceProvider.GetRequiredService<IOptions<CmdConfig>>().Value;

			BundleCacheOptions options = new BundleCacheOptions();
			if (cmdConfig.Cache.HeaderCacheSize.HasValue)
			{
				options.HeaderCacheSize = cmdConfig.Cache.HeaderCacheSize.Value * 1024 * 1024;
			}
			if (cmdConfig.Cache.PacketCacheSize.HasValue)
			{
				options.PacketCacheSize = cmdConfig.Cache.PacketCacheSize.Value * 1024 * 1024;
			}

			return new BundleCache(options);
		}

		static StorageBackendCache CreateStorageBackendCache(IServiceProvider serviceProvider)
		{
			CmdConfig cmdConfig = serviceProvider.GetRequiredService<IOptions<CmdConfig>>().Value;
			DirectoryReference cacheDir = DirectoryReference.Combine(GetDataDir(), String.IsNullOrEmpty(cmdConfig.Cache.CacheDir)? "Cache" : cmdConfig.Cache.CacheDir);
			return new StorageBackendCache(cacheDir, cmdConfig.Cache.CacheSize * 1024 * 1024, serviceProvider.GetRequiredService<ILogger<StorageBackendCache>>());
		}

		static DirectoryReference GetAppDir()
		{
			return new DirectoryReference(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location)!);
		}

		static DirectoryReference GetDataDir()
		{
			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				DirectoryReference? programDataDir = DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.CommonApplicationData);
				if (programDataDir != null)
				{
					return DirectoryReference.Combine(programDataDir, "Horde");
				}
			}
			return GetAppDir();
		}

		public static ILoggerFactory CreateLoggerFactory(IConfiguration configuration, bool quiet)
		{
			Serilog.ILogger logger = CreateSerilogLogger(configuration, quiet);
			return new Serilog.Extensions.Logging.SerilogLoggerFactory(logger, true);
		}

		static Serilog.ILogger CreateSerilogLogger(IConfiguration configuration, bool quiet)
		{
			DirectoryReference.CreateDirectory(CmdApp.DataDir);

			ConsoleTheme theme;
			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows) && Environment.OSVersion.Version < new Version(10, 0))
			{
				theme = SystemConsoleTheme.Literate;
			}
			else
			{
				theme = AnsiConsoleTheme.Code;
			}

			return new LoggerConfiguration()
				.WriteTo.Console(restrictedToMinimumLevel: quiet? Serilog.Events.LogEventLevel.Warning : Serilog.Events.LogEventLevel.Verbose, outputTemplate: "[{Timestamp:HH:mm:ss} {Level:w3}] {Indent}{Message:l}{NewLine}{Exception}", theme: theme)
				.WriteTo.File(FileReference.Combine(CmdApp.DataDir, "Log-.txt").FullName, fileSizeLimitBytes: 50 * 1024 * 1024, rollingInterval: RollingInterval.Day, rollOnFileSizeLimit: true, retainedFileCountLimit: 10)
				.WriteTo.File(new JsonFormatter(renderMessage: true), FileReference.Combine(CmdApp.DataDir, "Log-.json").FullName, fileSizeLimitBytes: 50 * 1024 * 1024, rollingInterval: RollingInterval.Day, rollOnFileSizeLimit: true, retainedFileCountLimit: 10)
				.ReadFrom.Configuration(configuration)
				.Enrich.FromLogContext()
				.CreateLogger();
		}
	}
}