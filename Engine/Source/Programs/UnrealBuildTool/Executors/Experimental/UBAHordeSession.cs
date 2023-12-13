// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde;
using EpicGames.Horde.Compute;
using EpicGames.Horde.Compute.Clients;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Clients;
using EpicGames.Horde.Storage.Nodes;
using EpicGames.OIDC;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Abstractions;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	/// <summary>
	/// Configuration for Unreal Build Accelerator Horde session
	/// </summary>
	[System.Diagnostics.CodeAnalysis.SuppressMessage("Style", "IDE1006:Naming Styles", Justification = "UnrealBuildTool naming style")]
	class UnrealBuildAcceleratorHordeConfig
	{
		/// <summary>
		/// Uri of the Horde server
		/// </summary>
		[XmlConfigFile(Category = "Horde", Name = "Server")]
		[CommandLine("-BoxHorde=")]
		[CommandLine("-UBAHorde=")]
		public string? HordeServer { get; set; }

		/// <summary>
		/// Uri of the Horde server
		/// </summary>
		[XmlConfigFile(Category = "Horde", Name = "Token")]
		[CommandLine("-BoxHordeToken=")]
		[CommandLine("-UBAHordeToken=")]
		public string? HordeToken { get; set; }

		/// <summary>
		/// OIDC id for the login to use
		/// </summary>
		[XmlConfigFile(Category = "Horde", Name = "OidcProvider")]
		[CommandLine("-BoxHordeOidc=")]
		[CommandLine("-UBAHordeOidc=")]
		public string? HordeOidcProvider { get; set; }

		/// <summary>
		/// Pool for the Horde agent to assign
		/// </summary>
		[XmlConfigFile(Category = "Horde", Name = "Pool")]
		[CommandLine("-BoxHordePool=")]
		[CommandLine("-UBAHordePool=")]
		public string? HordePool { get; set; }

		/// <summary>
		/// Requirements for the Horde agent to assign
		/// </summary>
		[XmlConfigFile(Category = "Horde", Name = "Requirements")]
		[CommandLine("-BoxHordeRequirements=")]
		[CommandLine("-UBAHordeRequirements=")]
		public string? HordeCondition { get; set; }

		/// <summary>
		/// Which ip UBA server should give to agents. This will invert so host listens and agents connect
		/// </summary>
		[XmlConfigFile(Category = "Horde", Name = "LocalHost")]
		[CommandLine("-BoxHordeHost")]
		[CommandLine("-UBAHordeHost")]
		public string HordeHost { get; set; } = String.Empty;

		/// <summary>
		/// Max cores allowed to be used by build session
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-BoxHordeMaxCores")]
		[CommandLine("-UBAHordeMaxCores")]
		public int HordeMaxCores { get; set; } = 576;

		/// <summary>
		/// How long UBT should wait to ask for help. Useful in build configs where machine can delay remote work and still get same wall time results (pch dependencies etc)
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-BoxHordeDelay")]
		[CommandLine("-UBAHordeDelay")]
		public int HordeDelay { get; set; } = 0;

		/// <summary>
		/// Allow use of Wine. Only applicable to Horde agents running Linux. Can still be ignored if Wine executable is not set on agent.
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-BoxHordeAllowWine", Value = "true")]
		[CommandLine("-UBAHordeAllowWine", Value = "true")]
		public bool bHordeAllowWine { get; set; } = true;

		/// <summary>
		/// Connection mode for agent/compute communication
		/// <see cref="ConnectionMode" /> for valid modes.
		/// </summary>
		[XmlConfigFile(Category = "Horde", Name = "ConnectionMode")]
		[CommandLine("-BoxHordeConnectionMode=")]
		[CommandLine("-UBAHordeConnectionMode=")]
		public string? HordeConnectionMode { get; set; }
		
		/// <summary>
		/// Encryption to use for agent/compute communication. Note that UBA agent uses its own encryption.
		/// <see cref="Encryption" /> for valid modes.
		/// </summary>
		[XmlConfigFile(Category = "Horde", Name = "Encryption")]
		[CommandLine("-BoxHordeEncryption=")]
		[CommandLine("-UBAHordeEncryption=")]
		public string? HordeEncryption { get; set; }

		/// <summary>
		/// Sentry URL to send box data to. Optional.
		/// </summary>
		[XmlConfigFile(Category = "Horde")]
		[CommandLine("-BoxSentryUrl=")]
		[CommandLine("-UBASentryUrl=")]
		public string? UBASentryUrl { get; set; }

		/// <summary>
		/// Disable horde all together
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-BoxDisableHorde")]
		[CommandLine("-UBADisableHorde")]
		public bool bDisableHorde { get; set; } = false;
	}

	class UBAHordeSession : IAsyncDisposable
	{
		/// <summary>
		/// ID for this Horde session
		/// </summary>
		readonly Guid _id = Guid.NewGuid();

		const string ResourceLogicalCores = "LogicalCores";
		readonly ClusterId _clusterId = new("default");

		readonly Uri _hordeUri;
		readonly string? _pool;
		readonly bool _allowWine;
		readonly int _maxCores;
		readonly bool _strict;
		readonly ConnectionMode? _connectionMode;
		readonly Encryption? _encryption;
		readonly CancellationTokenSource _cancellationTokenSource = new();
		readonly ILogger _logger;

		readonly UBAExecutor _owner;

		readonly IStorageClient _storage = BundleStorageClient.CreateFromMemory(NullLogger.Instance);
		BlobLocator _ubaAgentLocator;

		readonly ServiceProvider _serviceProvider;
		readonly string _crypto;
		IComputeClient? _client;

		public struct Worker
		{
			public Task BackgroundTask { get; set; }
			public int NumLogicalCores { get; set; }
			public Stopwatch StartTime { get; set; }
			public bool Started { get; set; }
			public string Ip { get; set; }
			public ConnectionMetadataPort Port { get; set; }
			public ConnectionMetadataPort ProxyPort { get; set; }
		}

		readonly List<Worker> _workers = new();

		public UBAHordeSession(UBAExecutor owner, Uri hordeUri, AuthenticationHeaderValue? authHeader, string? pool, bool allowWine, int maxCores, bool strict, ConnectionMode? connectionMode, Encryption? encryption, ILogger logger)
		{
			_owner = owner;
			_hordeUri = hordeUri;
			_pool = pool;
			_allowWine = allowWine;
			_maxCores = maxCores;
			_strict = strict;
			_connectionMode = connectionMode;
			_encryption = encryption;
			_logger = logger;
			_crypto = owner.Crypto;

			void ConfigureHttpClient(HttpClient httpClient)
			{
				httpClient.BaseAddress = _hordeUri;
				httpClient.DefaultRequestHeaders.Authorization = authHeader;
			}

			ServiceCollection services = new();
			services.AddHttpClient<HordeHttpClient>(ConfigureHttpClient);
			_serviceProvider = services.BuildServiceProvider();

			if (connectionMode == ConnectionMode.Relay && String.IsNullOrEmpty(_crypto))
			{
				_crypto = UBAExecutor.CreateCrypto();
			}
		}

		public async ValueTask DisposeAsync()
		{
			if (_workers.Count > 0) // Should handle double-dispose, prevent cancelling twice
			{
				_cancellationTokenSource.Cancel();

				for (int idx = _workers.Count - 1; idx >= 0; idx--)
				{
					await _workers[idx].BackgroundTask;
					_workers.RemoveAt(idx);
				}
			}

			if (_client != null)
			{
				// Set CPU resource need to zero (will also expire on the server if not updated)
				await UpdateCpuCoreNeedAsync(0);
				await _client.DisposeAsync();
				_client = null;
			}

			await _serviceProvider.DisposeAsync();
			_cancellationTokenSource.Dispose();

			_storage.Dispose();
		}

		public async Task InitAsync(bool useSentry, CancellationToken cancellationToken)
		{
			if (_client != null)
			{
				throw new InvalidOperationException("Session has already been initialized");
			}

			string? sessionId = null;
			string? jobId = Environment.GetEnvironmentVariable("UE_HORDE_JOBID");
			string? batchId = Environment.GetEnvironmentVariable("UE_HORDE_BATCHID");
			string? stepId = Environment.GetEnvironmentVariable("UE_HORDE_STEPID");

			if (jobId != null && batchId != null && stepId != null)
			{
				sessionId = $"{jobId}-{batchId}-{stepId}";
			}

			_client = new ServerComputeClient(_serviceProvider.GetRequiredService<IHttpClientFactory>(), sessionId, _logger);

			_logger.LogInformation("Creating tool bundle...");
			DirectoryReference ubaDir;
			List<string> agentFiles = new();
			if (OperatingSystem.IsWindows())
			{
#pragma warning disable CA1308 // Normalize strings to uppercase
				ubaDir  = DirectoryReference.Combine(Unreal.EngineDirectory, "Binaries", "Win64", "UnrealBuildAccelerator", RuntimeInformation.ProcessArchitecture.ToString().ToLowerInvariant());
#pragma warning restore CA1308 // Normalize strings to uppercase
				agentFiles.Add("UbaAgent.exe");
				if (useSentry)
				{
					agentFiles.Add("crashpad_handler.exe");
					agentFiles.Add("sentry.dll");
				}
			}
			else if (OperatingSystem.IsLinux())
			{
				agentFiles.Add("UbaAgent");
				if (RuntimeInformation.ProcessArchitecture == Architecture.X64)
				{
					ubaDir = DirectoryReference.Combine(Unreal.EngineDirectory, "Binaries", "Linux", "UnrealBuildAccelerator");
				}
				else if (RuntimeInformation.ProcessArchitecture == Architecture.Arm64)
				{
					ubaDir = DirectoryReference.Combine(Unreal.EngineDirectory, "Binaries", "LinuxArm64", "UnrealBuildAccelerator");
				}
				else
				{
					throw new PlatformNotSupportedException();
				}
			}
			else if (OperatingSystem.IsMacOS())
			{
				agentFiles.Add("UbaAgent");
				ubaDir = DirectoryReference.Combine(Unreal.EngineDirectory, "Binaries", "Mac", "UnrealBuildAccelerator");
			}
			else
			{
				throw new PlatformNotSupportedException();
			}
			_ubaAgentLocator = await CreateToolAsync(ubaDir, agentFiles.Select(x => FileReference.Combine(ubaDir, x)), cancellationToken);
			_logger.LogInformation("Created tool bundle with locator {UbaAgentLocator}", _ubaAgentLocator.ToString());
		}

		async Task<BlobLocator> CreateToolAsync(DirectoryReference baseDir, IEnumerable<FileReference> files, CancellationToken cancellationToken)
		{
			await using IStorageWriter writer = _storage.CreateWriter();
			DirectoryNode sandbox = new();
			await sandbox.AddFilesAsync(baseDir, files, new ChunkingOptions(), writer, null, cancellationToken);
			IBlobHandle handle = await writer.FlushAsync(sandbox, cancellationToken);
			return handle.GetLocator();
		}

		public int NumLogicalCores { get; private set; } = 0;

		public async void RemoveCompleteWorkers()
		{
			for (int idx = 0; idx < _workers.Count; idx++)
			{
				Worker worker = _workers[idx];
				if (worker.BackgroundTask.IsCompleted)
				{
					await worker.BackgroundTask;
					NumLogicalCores -= worker.NumLogicalCores;
					_workers.RemoveAt(idx--);
				}
			}
		}

		public int QueuedUpCores()
		{
			int count = 0;
			foreach (Worker worker in _workers)
			{
				if (!worker.Started)
				{
					count += worker.NumLogicalCores;
				}
			}
			return count;
		}

		int _workerId = 0;

		static readonly HashSet<StringView> s_logProperties = new()
			{
				"ComputeIp",
				"CPU",
				"RAM",
				"DiskFreeSpace",
				"PhysicalCores",
				"LogicalCores"
			};

		public async Task<bool> AddWorkerAsync(Requirements requirements, CancellationToken cancellationToken)
		{
			if (_client == null)
			{
				throw new InvalidOperationException("Call init first");
			}

			PrefixLogger workerLogger = new($"[Worker{_workerId}]", _logger);
			
			const string UbaPortName = "UbaPort";
			const string UbaProxyPortName = "UbaProxyPort";
			const int UbaPort = 7001;
			const int UbaProxyPort = 7002;

			// Request ID that is unique per attempt to acquire the same compute lease/worker
			// Primarily for tracking worker demand on Horde server as UBAExecutor will repeatedly try adding a new worker
			string requestId = $"{_id}-worker-{_workerId}";
			IComputeLease? lease = null;
			try
			{
				Stopwatch stopwatch = Stopwatch.StartNew();
				ConnectionMetadataRequest cmr = new ()
				{
					ModePreference = _connectionMode,
					Encryption = _encryption,
					Ports = { {UbaPortName, UbaPort}, {UbaProxyPortName, UbaProxyPort} }
				};
				lease = await _client.TryAssignWorkerAsync(_clusterId, requirements, requestId, cmr, workerLogger, cancellationToken);
				if (lease == null)
				{
					_logger.LogDebug("Unable to assign a remote worker");

					int missingNumCores = Math.Max(0, _maxCores - NumLogicalCores);
					await UpdateCpuCoreNeedAsync(missingNumCores, cancellationToken);
					return false;
				}

				_workerId++;

				workerLogger.LogInformation("Agent properties:");

				int numLogicalCores = 24; // Assume 24 if something goes wrong here and property is not found
				string computeIp = String.Empty;
				foreach (string property in lease.Properties)
				{
					int equalsIdx = property.IndexOf('=', StringComparison.OrdinalIgnoreCase);
					StringView propertyName = new(property, 0, equalsIdx);
					if (s_logProperties.Contains(propertyName))
					{
						_logger.LogInformation("  {Property}", property);

						if (propertyName == ResourceLogicalCores && Int32.TryParse(property.AsSpan(equalsIdx + 1), out int value))
						{
							numLogicalCores = value;
						}
						else if (propertyName == "ComputeIp")
						{
							computeIp = property[(equalsIdx + 1)..];
						}
					}
				}

				// When using relay connection mode, the IP will be relay server's IP
				string ip = String.IsNullOrEmpty(lease.Ip) ? computeIp : lease.Ip;
				
				if (!lease.Ports.TryGetValue(UbaPortName, out ConnectionMetadataPort? ubaPort))
				{
					ubaPort = new ConnectionMetadataPort(UbaPort, UbaPort);
				}
				
				if (!lease.Ports.TryGetValue(UbaProxyPortName, out ConnectionMetadataPort? ubaProxyPort))
				{
					ubaProxyPort = new ConnectionMetadataPort(UbaProxyPort, UbaProxyPort);
				}

				string exeName = OperatingSystem.IsWindows() ? "UbaAgent.exe" : "UbaAgent";
				BlobLocator locator = _ubaAgentLocator;
				Worker worker = new()
				{
					StartTime = stopwatch,
					NumLogicalCores = numLogicalCores,
					Ip = ip, 
					Port = ubaPort,
					ProxyPort = ubaProxyPort,
				};
				worker.BackgroundTask = RunWorkerAsync(worker, lease, locator, exeName, workerLogger, _cancellationTokenSource.Token);
				_workers.Add(worker);
				lease = null; // Will be disposed by RunWorkerAsync

				NumLogicalCores += numLogicalCores;
				return true;
			}
			finally
			{
				if (lease != null)
				{
					await lease.DisposeAsync();
				}
			}
		}

		async Task UpdateCpuCoreNeedAsync(int targetCoreCount, CancellationToken cancellationToken = default)
		{
			if (_client != null && _pool != null)
			{
				_logger.LogDebug("Setting CPU core need to {TargetCoreCount}", targetCoreCount);
				Dictionary<string, int> resourceNeeds = new() { { ResourceLogicalCores, targetCoreCount } };
				try
				{
					await _client.DeclareResourceNeedsAsync(_clusterId, _pool, resourceNeeds, cancellationToken);
				}
				catch (Exception e)
				{
					_logger.Log(_strict ? LogLevel.Error : LogLevel.Information, KnownLogEvents.Systemic_Horde_Compute, e, "Failed updating resource need to {TargetCoreCount} cores", targetCoreCount);
				}
			}
		}

		public static async Task<UBAHordeSession?> TryCreateHordeSession(UnrealBuildAcceleratorHordeConfig hordeConfig, UBAExecutor executor, bool bStrictErrors, ILogger logger, CancellationToken cancellationToken = default)
		{
			if (hordeConfig.bDisableHorde)
			{
				logger.LogInformation("Horde disabled via command line option.");
				return null;
			}

			string? server = hordeConfig.HordeServer;
			string? token = hordeConfig.HordeToken;
			string? oidcProvider = hordeConfig.HordeOidcProvider;

			if (String.IsNullOrEmpty(server))
			{
				server = Environment.GetEnvironmentVariable("UE_HORDE_URL");
				if (String.IsNullOrEmpty(server))
				{
					logger.LogInformation("Horde URL not specified in BuildConfiguration.xml or via UE_HORDE_URL environment variable");
					return null;
				}
				if (String.IsNullOrEmpty(token))
				{
					token = Environment.GetEnvironmentVariable("UE_HORDE_TOKEN");
				}
			}

			ConnectionMode? connectionMode = Enum.TryParse(hordeConfig.HordeConnectionMode, true, out ConnectionMode cm) ? cm : null;
			Encryption? encryption = Enum.TryParse(hordeConfig.HordeEncryption, true, out Encryption enc) ? enc : null;

			oidcProvider ??= Environment.GetEnvironmentVariable("UE_HORDE_OIDC_PROVIDER");

			bool hasOidcProvider = !String.IsNullOrEmpty(oidcProvider);
			logger.LogInformation("Horde URL: {Server}, Pool: {Pool}, Condition: {Condition}, OIDC: {OidcProvider}, Connection: {Connection} HordeEncryption: {Encryption}",
				server, hordeConfig.HordePool ?? "(none)", hordeConfig.HordeCondition ?? "(none)", hasOidcProvider ? oidcProvider! : "Disabled", connectionMode?.ToString() ?? "(none)", encryption?.ToString() ?? "(none)");
			try
			{
				if (String.IsNullOrEmpty(token) && hasOidcProvider)
				{
					token = await GetOidcBearerTokenAsync(null, oidcProvider!, logger, cancellationToken);
				}

				AuthenticationHeaderValue? authHeader = null;
				if (!String.IsNullOrEmpty(token))
				{
					authHeader = new AuthenticationHeaderValue("Bearer", token);
				}

				bool allowWine = hordeConfig.bHordeAllowWine && OperatingSystem.IsWindows();

				UBAHordeSession session = new(executor, new Uri(server), authHeader, hordeConfig.HordePool, allowWine, hordeConfig.HordeMaxCores, bStrictErrors, connectionMode, encryption, logger);
				await session.InitAsync(useSentry: !String.IsNullOrEmpty(hordeConfig.UBASentryUrl), cancellationToken);
				return session;
			}
			catch (TaskCanceledException)
			{
				return null;
			}
			catch (Exception ex)
			{
				logger.Log(bStrictErrors ? LogLevel.Error : LogLevel.Information, ex, "Unable to create Horde session: {Message}", ex.Message);
				return null;
			}
		}

		static async Task<string> GetOidcBearerTokenAsync(DirectoryReference? projectDir, string oidcProvider, ILogger logger, CancellationToken cancellationToken = default)
		{
			logger.LogInformation("Performing OIDC token refresh...");

			using ITokenStore tokenStore = TokenStoreFactory.CreateTokenStore();
			IConfiguration providerConfiguration = ProviderConfigurationFactory.ReadConfiguration(Unreal.EngineDirectory.ToDirectoryInfo(), projectDir?.ToDirectoryInfo());
			OidcTokenManager oidcTokenManager = OidcTokenManager.CreateTokenManager(providerConfiguration, tokenStore, new List<string>() { oidcProvider });

			OidcTokenInfo result;
			try
			{
				result = await oidcTokenManager.GetAccessToken(oidcProvider, cancellationToken);
			}
			catch (NotLoggedInException)
			{
				result = await oidcTokenManager.Login(oidcProvider, cancellationToken);
			}

			if (result.AccessToken == null)
			{
				throw new Exception($"Unable to get access token for {oidcProvider}");
			}

			logger.LogInformation("Received bearer token for {OidcProvider}", oidcProvider);
			return result.AccessToken;
		}

		async Task RunWorkerAsync(Worker self, IComputeLease lease, BlobLocator tool, string executable, ILogger logger, CancellationToken cancellationToken)
		{
			logger.LogInformation("Running worker task..");
			try
			{
				await using (_ = lease)
				{
					// Create a message channel on channel id 0. The Horde Agent always listens on this channel for requests.
					const int PrimaryChannelId = 0;
					using (AgentMessageChannel channel = lease.Socket.CreateAgentMessageChannel(PrimaryChannelId, 4 * 1024 * 1024))
					{
						logger.LogInformation("Waiting for attach...");

						TimeSpan attachTimeout = TimeSpan.FromSeconds(20.0);
						try
						{
							Task attachTask = channel.WaitForAttachAsync(cancellationToken).AsTask();
							await attachTask.WaitAsync(attachTimeout, cancellationToken);
						}
						catch (TimeoutException)
						{
							logger.Log(_strict ? LogLevel.Error : LogLevel.Information, KnownLogEvents.Systemic_Horde_Compute, "Waited {Time}s on attach message. Giving up", (int)attachTimeout.TotalSeconds);
							throw;
						}

						logger.LogInformation("Uploading files...");
						await channel.UploadFilesAsync("", tool, _storage, cancellationToken);

						string hordeHost = _owner.UBAConfig.Host;
						if (!String.IsNullOrEmpty(_owner.HordeConfig.HordeHost))
						{
							hordeHost = _owner.HordeConfig.HordeHost;
						}

						bool useListen = !String.IsNullOrEmpty(_owner.HordeConfig.HordeHost);
						List<string> arguments = new();

						if (useListen)
						{
							arguments.Add($"-Host={hordeHost}:{_owner.UBAConfig.Port}");
						}
						else
						{
							arguments.Add($"-Listen={self.Port.AgentPort}");
						}

						if (!String.IsNullOrEmpty(_crypto))
						{
							arguments.Add($"-crypto={_crypto}");
						}

						arguments.Add("-NoPoll");
						arguments.Add("-Quiet");
						if (!String.IsNullOrEmpty(_owner.HordeConfig.UBASentryUrl))
						{
							arguments.Add($"-Sentry=\"{_owner.HordeConfig.UBASentryUrl}\"");
						}
						arguments.Add("-ProxyPort=" + self.ProxyPort.AgentPort);
						if (_owner.UBAConfig.bUseQuic)
						{
							arguments.Add("-quic");
						}
						//arguments.Add("-NoStore");
						//arguments.Add("-KillRandom"); // For debugging

						arguments.Add("-Dir=%UE_HORDE_SHARED_DIR%\\Uba");
						arguments.Add("-Eventfile=%UE_HORDE_TERMINATION_SIGNAL_FILE%");
						arguments.Add("-MaxIdle=15");
						if (_owner.UBAConfig.bLogEnabled)
						{
							arguments.Add("-Log");
						}

						if (OperatingSystem.IsMacOS())
						{
							arguments.Add("-populateCasFromXcode");
						}

						logger.LogInformation("Executing child process: {Executable} {Arguments}", executable, CommandLineArguments.Join(arguments));

						ExecuteProcessFlags execFlags = _allowWine ? ExecuteProcessFlags.UseWine : ExecuteProcessFlags.None;
						await using AgentManagedProcess process = await channel.ExecuteAsync(executable, arguments, null, null, execFlags, cancellationToken);
						bool shouldConnect = !useListen;
						self.Started = true;
						string? line;
						while ((line = await process.ReadLineAsync(cancellationToken)) != null)
						{
							if (shouldConnect && line.Contains("Listening on")) // This log entry means that the agent is ready for connections.
							{
								long totalMs = self.StartTime.ElapsedMilliseconds;
								logger.LogInformation("Connecting to UbaAgent on {Ip}:{Port} (local agent port {AgentPort}) {Seconds}.{Milliseconds} seconds after assigned", 
									self.Ip, self.Port.Port, self.Port.AgentPort, totalMs / 1000, totalMs % 1000);
								
								_owner.Server!.AddClient(self.Ip, self.Port.Port, _crypto);
								shouldConnect = false;
							}

							logger.LogInformation("{Line}", line);
						}
						logger.LogInformation("Shutting down process");
					}

					logger.LogInformation("Closing channel");
					await lease.CloseAsync(cancellationToken);
				}
			}
			catch (TimeoutException)
			{
			}
			catch (Exception ex)
			{
				if (!cancellationToken.IsCancellationRequested)
				{
					logger.Log(_strict ? LogLevel.Error : LogLevel.Information, KnownLogEvents.Systemic_Horde_Compute, ex, "Exception in worker task: {Ex}", ex.ToString());

					// Add additional properties to aid debugging
					logger.LogInformation(KnownLogEvents.Systemic_Horde_Compute, ex, "UBA agent locator {UBAAgentLocator}", _ubaAgentLocator.ToString());
				}
			}
		}
	}
}
