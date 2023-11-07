// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Net;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using Horde.Server.Server;
using Horde.Server.Utilities;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using MongoDB.Driver;
using StackExchange.Redis;

namespace Horde.Server.Tests
{
	// Stub for fulfilling IOptions interface during testing
	public sealed class TestOptions<T> : IOptions<T> where T : class
	{
		public TestOptions(T options) => Value = options;
		public T Value { get; }
	}

	// Stub for fulfilling IOptionsMonitor interface during testing
	public sealed class TestOptionsMonitor<T> : IOptionsMonitor<T>
        where T : class, new()
    {
		sealed class Disposable : IDisposable
		{
			public void Dispose() { }
		}

		public TestOptionsMonitor(T currentValue)
        {
            CurrentValue = currentValue;
        }

        public T Get(string? name)
        {
            return CurrentValue;
        }

        public IDisposable OnChange(Action<T, string> listener)
        {
			return new Disposable();
        }

        public T CurrentValue { get; }
    }

	public sealed class MongoDbInstance : IDisposable
	{
		public string DatabaseName { get; }
		public string ConnectionString { get; }
		MongoClient Client { get; }

		private static readonly object s_lockObject = new object();
		private static MongoDbRunnerLocal? s_mongoDbRunner;
		private static int s_nextDatabaseIndex = 1;
		public const string MongoDbDatabaseNamePrefix = "HordeServerTest_";

		public MongoDbInstance()
		{
			int databaseIndex;
			lock (s_lockObject)
			{
				if (s_mongoDbRunner == null)
				{
					// One-time setup per test run to avoid overhead of starting the external MongoDB process
					Startup.ConfigureMongoDbClient();
					s_mongoDbRunner = new MongoDbRunnerLocal();
					s_mongoDbRunner.Start();

					// Drop all the previous databases
					MongoClientSettings mongoSettings = MongoClientSettings.FromConnectionString(s_mongoDbRunner.GetConnectionString());
					MongoClient client = new MongoClient(mongoSettings);

					List<string> dropDatabaseNames = client.ListDatabaseNames().ToList();
					foreach (string dropDatabaseName in dropDatabaseNames)
					{
						if (dropDatabaseName.StartsWith(MongoDbDatabaseNamePrefix, StringComparison.Ordinal))
						{
							client.DropDatabase(dropDatabaseName);
						}
					}
				}
				databaseIndex = s_nextDatabaseIndex++;
			}

			DatabaseName = $"{MongoDbDatabaseNamePrefix}{databaseIndex}";
			ConnectionString = $"{s_mongoDbRunner.GetConnectionString()}/{DatabaseName}";
			Client = new MongoClient(MongoClientSettings.FromConnectionString(ConnectionString));
		}

		public void Dispose()
		{
			IMongoClient strictClient = Client.WithWriteConcern(new WriteConcern(journal: true));
			for (int i = 0; i < 5; i++)
			{
				strictClient.DropDatabase(DatabaseName);
				List<string> dbNames = strictClient.ListDatabaseNames().ToList();
				if (!dbNames.Contains(DatabaseName))
				{
					return;
				}
				Thread.Sleep(300);
			}

			throw new Exception($"Unable to drop MongoDB database {DatabaseName}");
		}
	}

	public class ServiceTest : IAsyncDisposable
	{
		private ServiceProvider? _serviceProvider = null;

		public IServiceProvider ServiceProvider
		{
			get
			{
				if (_serviceProvider == null)
				{
					IServiceCollection services = new ServiceCollection();
					ConfigureServices(services);

					_serviceProvider = services.BuildServiceProvider();
				}
				return _serviceProvider;
			}
		}

		public virtual async ValueTask DisposeAsync()
		{
			GC.SuppressFinalize(this);

			if (_serviceProvider != null)
			{
				await _serviceProvider.DisposeAsync();
				_serviceProvider = null;
			}
		}

		protected virtual void ConfigureSettings(ServerSettings settings)
		{
		}

		protected virtual void ConfigureServices(IServiceCollection services)
		{
		}
	}

	public class DatabaseIntegrationTest : ServiceTest
    {
		private static readonly object s_lockObject = new object();

		private MongoDbInstance? _mongoDbInstance;
		private MongoService? _mongoService;
		private readonly LoggerFactory _loggerFactory = new LoggerFactory();

		const bool UseExistingRedisInstance = true;
		const int RedisPort = 6379;
		const int RedisDbNum = 15;

		private static int? s_redisPort;
		private static RedisProcess? s_redisProcess;
		private RedisService? _redisService;

		public DatabaseIntegrationTest()
		{
		}

		protected override void ConfigureServices(IServiceCollection services)
		{
			services.AddSingleton(GetMongoServiceSingleton());
			services.AddSingleton(GetRedisServiceSingleton());
		}

		public override async ValueTask DisposeAsync()
		{
			await base.DisposeAsync();

			GC.SuppressFinalize(this);
			_mongoDbInstance?.Dispose();
			_mongoService?.Dispose();

			if (_redisService != null)
			{
				await _redisService.DisposeAsync();
			}

			_loggerFactory.Dispose();
		}

		public MongoService GetMongoServiceSingleton()
        {
			lock(s_lockObject)
			{
				if (_mongoService == null)
				{
					RedisService redisService = GetRedisServiceSingleton();

					_mongoDbInstance = new MongoDbInstance();

					ServerSettings ss = new ServerSettings();
					ss.DatabaseName = _mongoDbInstance.DatabaseName;
					ss.DatabaseConnectionString = _mongoDbInstance.ConnectionString;

					_mongoService = new MongoService(Options.Create(ss), redisService, OpenTelemetryTracers.Horde, _loggerFactory.CreateLogger<MongoService>(), _loggerFactory);
				}
			}
			return _mongoService;
        }

		int GetRedisPort()
		{
			lock (s_lockObject)
			{
				s_redisPort ??= GetRedisPortInternal();
				return s_redisPort.Value;
			}
		}

		int GetRedisPortInternal()
		{
			if (UseExistingRedisInstance && !DatabaseRunner.IsPortAvailable(RedisPort))
			{
				return RedisPort;
			}

			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				s_redisProcess = new RedisProcess(_loggerFactory.CreateLogger("redis"));
				s_redisProcess.Start("--save \"\" --appendonly no");

				return s_redisProcess.Port;
			}

			throw new Exception("Unable to connect to Redis");
		}

		public RedisService GetRedisServiceSingleton()
        {
			if (_redisService == null)
			{
				int port = GetRedisPort();
				_redisService = new RedisService($"localhost:{port},allowAdmin=true", RedisDbNum, _loggerFactory.CreateLogger<RedisService>());

				IConnectionMultiplexer cm = _redisService.ConnectionPool.GetConnection();
				foreach (EndPoint endpoint in cm.GetEndPoints())
				{
					cm.GetServer(endpoint).FlushDatabase(RedisDbNum);
				}
			}
			return _redisService;
        }
        
		public static T Deref<T>(T? item)
		{
			Assert.IsNotNull(item);
			return item!;
		}

		public static T Deref<T>(ActionResult<T>? item) where T : class
		{
			Assert.IsNotNull(item?.Value);
			return item!.Value!;
		}
	}
}