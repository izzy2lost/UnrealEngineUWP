// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.UBA;
using Microsoft.Extensions.Logging;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;
using System.Management;
using System.Runtime.InteropServices;
using System.Runtime.Versioning;
using System.Threading.Tasks;
using UnrealBuildBase;
using UnrealBuildTool.Artifacts;

namespace UnrealBuildTool
{
	/// <summary>
	/// Configuration for Unreal Build Accelerator
	/// </summary>
	[System.Diagnostics.CodeAnalysis.SuppressMessage("Style", "IDE1006:Naming Styles", Justification = "UnrealBuildTool naming style")]
	class UnrealBuildAcceleratorConfig
	{
		/// <summary>
		/// When set to true, UBA will not use any remote help
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-BoxDisableRemote")]
		[CommandLine("-UBADisableRemote")]
		public bool bDisableRemote { get; set; } = false;

		/// <summary>
		/// When set to true, UBA will force all actions that can be built remotely to be built remotely. This will hang if there are no remote agents available
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-BoxForceRemote")]
		[CommandLine("-UBAForceRemote")]
		public bool bForceBuildAllRemote { get; set; } = false;

		/// <summary>
		/// When set to true, actions that fail locally with UBA will be retried without UBA.
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-BoxForcedRetry")]
		[CommandLine("-UBAForcedRetry")]
		public bool bForcedRetry { get; set; } = false;

		/// <summary>
		/// When set to true, all errors and warnings from UBA will be output at the appropriate severity level to the log (rather than being output as 'information' and attempting to continue regardless).
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-BoxStrict")]
		[CommandLine("-UBAStrict")]
		public bool bStrict { get; set; } = false;

		/// <summary>
		/// If UBA should store cas compressed or raw
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-BoxStoreRaw")]
		[CommandLine("-UBAStoreRaw")]
		public bool bStoreRaw { get; set; } = false;

		/// <summary>
		/// If UBA should distribute linking to remote workers. This needs bandwidth but can be an optimization
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-BoxLinkRemote")]
		[CommandLine("-UBALinkRemote")]
		public bool bLinkRemote { get; set; } = false;

		/// <summary>
		/// The amount of gigabytes UBA is allowed to use to store workset and cached data. It is a good idea to have this >10gb
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-BoxStoreCapacityGb")]
		[CommandLine("-UBAStoreCapacityGb")]
		public int StoreCapacityGb { get; set; } = 40;

		/// <summary>
		/// Max number of worker threads that can handle messages from remotes. 
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-BoxMaxWorkers")]
		[CommandLine("-UBAMaxWorkers")]
		public int MaxWorkers { get; set; } = 192;

		/// <summary>
		/// Max size of each message sent from server to client
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-BoxSendSize")]
		[CommandLine("-UBASendSize")]
		public int SendSize { get; set; } = 256 * 1024;

		/// <summary>
		/// Which ip UBA server should listen to for connections
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-BoxHost")]
		[CommandLine("-UBAHost")]
		public string Host { get; set; } = String.Empty;

		/// <summary>
		/// Which port UBA server should listen to for connections.
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-BoxPort")]
		[CommandLine("-UBAPort")]
		public int Port { get; set; } = 1345;

		/// <summary>
		/// Which directory to store files for UBA.
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-BoxRootDir")]
		[CommandLine("-UBARootDir")]
		public string? RootDir { get; set; } = null;

		/// <summary>
		/// Use Quic protocol instead of Tcp (experimental)
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-BoxQuic", Value = "true")]
		[CommandLine("-UBAQuic", Value = "true")]
		public bool bUseQuic { get; set; } = false;

		/// <summary>
		/// Enable logging of UBA processes
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-BoxLog", Value = "true")]
		[CommandLine("-UBALog", Value = "true")]
		public bool bLogEnabled { get; set; } = false;

		/// <summary>
		/// Prints summary of UBA stats at end of build
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-BoxPrintSummary", Value = "true")]
		[CommandLine("-UBAPrintSummary", Value = "true")]
		public bool bPrintSummary { get; set; } = false;

		/// <summary>
		/// Launch visualizer application which shows build progress
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-BoxVisualizer", Value = "true")]
		[CommandLine("-UBAVisualizer", Value = "true")]
		public bool bLaunchVisualizer { get; set; } = false;

		/// <summary>
		/// Resets the cas cache
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-BoxResetCas", Value = "true")]
		[CommandLine("-UBAResetCas", Value = "true")]
		public bool bResetCas { get; set; } = false;

		/// <summary>
		/// Provide custom path for trace output file
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-BoxTraceOutputFile")]
		[CommandLine("-UBATraceOutputFile")]
		public string TraceFile { get; set; } = String.Empty;

		/// <summary>
		/// Add verbose details to the UBA trace
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-BoxDetailedTrace", Value = "true")]
		[CommandLine("-UBADetailedTrace", Value = "true")]
		public bool bDetailedTrace { get; set; }

		/// <summary>
		/// Disable UBA waiting on available memory before spawning new processes
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBADisableWaitOnMem", Value = "true")]
		public bool bDisableWaitOnMem { get; set; }

		/// <summary>
		/// Let UBA kill running processes when close to out of memory
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-BoxAllowKillOnMem", Value = "true")]
		[CommandLine("-UBAAllowKillOnMem", Value = "true")]
		public bool bAllowKillOnMem { get; set; }

		/// <summary>
		/// Threshold for when executor should output logging for the process. Defaults to never
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-BoxOutputStatsThresholdMs")]
		[CommandLine("-UBAOutputStatsThresholdMs")]
		public int OutputStatsThresholdMs { get; set; } = Int32.MaxValue;

		/// <summary>
		/// Skip writing intermediate and output files to disk. Useful for validation builds where we don't need the output
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-BoxNoWrite", Value = "false")]
		[CommandLine("-UBANoWrite", Value = "false")]
		public bool bWriteToDisk { get; set; } = true;

		/// <summary>
		/// Set to true to disable mimalloc and detouring of memory allocations.
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-BoxNoCustoMalloc", Value = "true")]
		[CommandLine("-UBANoCustoMalloc", Value = "true")]
		public bool bDisableCustomAlloc { get; set; } = false;

		/// <summary>
		/// The zone to use for UBA.
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-BoxZone=")]
		[CommandLine("-UBAZone=")]
		public string Zone { get; set; } = String.Empty;

		/// <summary>
		/// Set to true to enable encryption when transfering files over the network.
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-BoxCrypto", Value = "true")]
		[CommandLine("-UBACrypto", Value = "true")]
		public bool bUseCrypto { get; set; } = false;

		/// <summary>
		/// Set to true to provide known inputs to processes that are run remote. This is an experimental feature to speed up build times when ping is higher
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBAUseKnownInputs", Value = "true")]
		public bool bUseKnownInputs { get; set; } = false;
	}

	class UBAExecutor : ParallelExecutor
	{
		public UnrealBuildAcceleratorConfig UBAConfig { get; init; } = new();

		public string Crypto { get; private set; } = String.Empty;
		public IServer? Server { get; private set; }
		ISessionServer? _session;
		List<IUBAAgentCoordinator> _agentCoordinators = new List<IUBAAgentCoordinator>();
		DirectoryReference? _rootDirRef;
		bool _bIsCancelled;
		bool _bIsRemoteActionsAllowed = true;
		readonly object _actionsChangedLock = new();
		bool _bActionsChanged = true;
		uint _actionsQueuedThatCanRunRemotely = UInt32.MaxValue;
		readonly ThreadedLogger _threadedLogger;

		// Tracking for LinkedActions that failed remotely that should be retried locally
		readonly ConcurrentDictionary<LinkedAction, bool> _localRetryActions = new();
		// Tracking for LinkedActions that failed locally that should be retried without UBA
		readonly ConcurrentDictionary<LinkedAction, bool> _forcedRetryActions = new();

		protected override void Dispose(bool disposing)
		{
			if (disposing)
			{
				_session?.Dispose();
				_session = null;
			}
			base.Dispose(disposing);
		}

		public override string Name => "Unreal Build Accelerator";

		public static new bool IsAvailable()
		{
			return EpicGames.UBA.Utils.IsAvailable();
		}

		public UBAExecutor(int maxLocalActions, bool bAllCores, bool bCompactOutput, Microsoft.Extensions.Logging.ILogger logger, CommandLineArguments? additionalArguments = null)
			: base(maxLocalActions, bAllCores, bCompactOutput, logger)
		{
			XmlConfig.ApplyTo(this);
			XmlConfig.ApplyTo(UBAConfig);
			CommandLine.ParseArguments(Environment.GetCommandLineArgs(), this, logger);
			additionalArguments?.ApplyTo(this);
			additionalArguments?.ApplyTo(UBAConfig);

			_threadedLogger = new ThreadedLogger(logger);
			_agentCoordinators.Add(new UBAAgentCoordinatorHorde(logger, UBAConfig, additionalArguments));
		}

		private void PrintConfiguration()
		{
			_threadedLogger.LogInformation("  Storage capacity {StoreCapacityGb}Gb", UBAConfig.StoreCapacityGb);
		}

		[System.Diagnostics.CodeAnalysis.SuppressMessage("Globalization", "CA1308:Normalize strings to uppercase", Justification = "lowercase crypto string")]
		public static string CreateCrypto()
		{
			byte[] bytes = new byte[16];
			using System.Security.Cryptography.RandomNumberGenerator random = System.Security.Cryptography.RandomNumberGenerator.Create();
			random.GetBytes(bytes);
			return BitConverter.ToString(bytes).Replace("-", "", StringComparison.OrdinalIgnoreCase).ToLowerInvariant(); // "1234567890abcdef1234567890abcdef";
		}

		public override async Task<bool> ExecuteActionsAsync(IEnumerable<LinkedAction> inputActions, Microsoft.Extensions.Logging.ILogger logger, IActionArtifactCache? actionArtifactCache)
		{
			if (!inputActions.Any())
			{
				return true;
			}

			if (inputActions.Count() < NumParallelProcesses && !UBAConfig.bForceBuildAllRemote)
			{
				UBAConfig.bDisableRemote = true;
			}

			PrintConfiguration();

			logger = _threadedLogger;

			if (UBAConfig.bUseCrypto)
			{
				Crypto = CreateCrypto();
			}

			if (!String.IsNullOrEmpty(UBAConfig.RootDir))
			{
				_rootDirRef = DirectoryReference.FromString(UBAConfig.RootDir);
			}
			else
			{
				_rootDirRef = DirectoryReference.FromString(Environment.GetEnvironmentVariable("UBA_ROOT") ?? Environment.GetEnvironmentVariable("BOX_ROOT"));
			}

			if (!UBAConfig.bDisableRemote)
			{
				foreach (var coordinator in _agentCoordinators)
				{
					await coordinator.InitAsync(this);

					if (_rootDirRef == null)
					{
						_rootDirRef = coordinator.GetUBARootDir();
					}
				}
			}

			if (_rootDirRef == null)
			{
				if (OperatingSystem.IsWindows())
				{
					_rootDirRef = DirectoryReference.Combine(DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.CommonApplicationData)!, "Epic", "UnrealBuildAccelerator");
				}
				else
				{
					_rootDirRef = DirectoryReference.Combine(DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.UserProfile)!, ".epic", "UnrealBuildAccelerator");
				}
			}

			DirectoryReference.CreateDirectory(_rootDirRef);

			FileReference ubaTraceFile;
			if (!String.IsNullOrEmpty(UBAConfig.TraceFile))
			{
				ubaTraceFile = new FileReference(UBAConfig.TraceFile);
			}
			else if (Unreal.IsBuildMachine())
			{
				ubaTraceFile = FileReference.Combine(Unreal.EngineProgramSavedDirectory, "AutomationTool", "Saved", "Logs", "Trace.uba");
			}
			else if (Log.OutputFile != null)
			{
				ubaTraceFile = Log.OutputFile.ChangeExtension(".uba");
			}
			else
			{
				ubaTraceFile = FileReference.Combine(Unreal.EngineProgramSavedDirectory, "UnrealBuildTool", "Trace.uba");
			}

			DirectoryReference.CreateDirectory(ubaTraceFile.Directory);
			Log.BackupLogFile(ubaTraceFile);

			IStorageServer? ubaStorage = null;
			void CancelKeyPress(object? sender, ConsoleCancelEventArgs e)
			{
				_bIsCancelled = true;
				_session?.CancelAll();
				ubaStorage?.SaveCasTable();
				foreach (var coordinator in _agentCoordinators)
				{
					coordinator.CloseAsync().Wait(2000); // Give coordinators some time to close (this makes coordinators like horde return resources faster)
				}
			}
			Console.CancelKeyPress += CancelKeyPress;

			try
			{
				if (UBAConfig.bLaunchVisualizer && OperatingSystem.IsWindows())
				{
					_ = Task.Run(LaunchVisualizer);
				}

				using EpicGames.UBA.ILogger ubaLogger = EpicGames.UBA.ILogger.CreateLogger(logger);
				using (Server = IServer.CreateServer(UBAConfig.MaxWorkers, UBAConfig.SendSize, ubaLogger, UBAConfig.bUseQuic))
				{
					using IStorageServer ubaStorageServer = IStorageServer.CreateStorageServer(Server, ubaLogger, new StorageServerCreateInfo(_rootDirRef.FullName, ((ulong)UBAConfig.StoreCapacityGb) * 1000 * 1000 * 1000, !UBAConfig.bStoreRaw, UBAConfig.Zone));
					using ISessionServerCreateInfo serverCreateInfo = ISessionServerCreateInfo.CreateSessionServerCreateInfo(ubaStorageServer, Server, ubaLogger, new SessionServerCreateInfo(_rootDirRef.FullName, ubaTraceFile.FullName, UBAConfig.bDisableCustomAlloc, false, UBAConfig.bResetCas, UBAConfig.bWriteToDisk, UBAConfig.bDetailedTrace, !UBAConfig.bDisableWaitOnMem, UBAConfig.bAllowKillOnMem));
					using (_session = ISessionServer.CreateSessionServer(serverCreateInfo))
					{

						ubaStorage = ubaStorageServer;

						if (!UBAConfig.bDisableRemote)
						{
							Server.StartServer(UBAConfig.Host, UBAConfig.Port, Crypto);
						}

						bool success = ExecuteActionsInternal(inputActions, _session, logger, actionArtifactCache);

						if (!UBAConfig.bDisableRemote)
						{
							Server.StopServer();
						}

						if (UBAConfig.bPrintSummary)
						{
							_session.PrintSummary();
						}

						return success;
					}
				}
			}
			finally
			{
				Console.CancelKeyPress -= CancelKeyPress;

				foreach (var coordinator in _agentCoordinators)
				{
					await coordinator.CloseAsync();
				}
				await _threadedLogger.FinishAsync();
				_session = null;
			}
		}

		[SupportedOSPlatform("windows")]
		static void LaunchVisualizer()
		{
			FileReference visaulizerPath = FileReference.Combine(Unreal.EngineDirectory, "Binaries", "Win64", "UnrealBuildAccelerator", RuntimeInformation.ProcessArchitecture.ToString(), "UbaVisualizer.exe");
			FileReference tempPath = FileReference.Combine(new DirectoryReference(System.IO.Path.GetTempPath()), visaulizerPath.GetFileName());
			if (!FileReference.Exists(visaulizerPath))
			{
				return;
			}

			try
			{
				// Check if a listening visualizer is already running
				foreach (System.Diagnostics.Process process in System.Diagnostics.Process.GetProcessesByName(visaulizerPath.GetFileNameWithoutAnyExtensions()))
				{
					using ManagementObjectSearcher searcher = new ManagementObjectSearcher($"SELECT CommandLine FROM Win32_Process WHERE ProcessId = {process.Id}");
					using ManagementObjectCollection objects = searcher.Get();
					string args = objects.Cast<ManagementBaseObject>().SingleOrDefault()?["CommandLine"]?.ToString() ?? "";
					if (args.Contains("-listen", StringComparison.OrdinalIgnoreCase))
					{
						return;
					}
				}
				if (!FileReference.Exists(tempPath) || tempPath.ToFileInfo().LastWriteTime < visaulizerPath.ToFileInfo().LastWriteTime)
				{
					FileReference.Copy(visaulizerPath, tempPath, true);
				}
				if (FileReference.Exists(tempPath))
				{
					System.Diagnostics.ProcessStartInfo psi = new(BuildHostPlatform.Current.Shell.FullName, $" /C start \"\" \"{tempPath.FullName}\" -listen")
					{
						WorkingDirectory = System.IO.Path.GetTempPath(),
						WindowStyle = System.Diagnostics.ProcessWindowStyle.Hidden,
						UseShellExecute = true,
					};
					System.Diagnostics.Process.Start(psi);
				}
			}
			catch(Exception)
			{
			}
		}

		public override bool VerifyOutputs => UBAConfig.bWriteToDisk;

		/// <summary>
		/// Executes the provided actions
		/// </summary>
		/// <returns>True if all the tasks successfully executed, or false if any of them failed.</returns>
		bool ExecuteActionsInternal(IEnumerable<LinkedAction> inputActions, ISessionServer session, Microsoft.Extensions.Logging.ILogger logger, IActionArtifactCache? actionArtifactCache)
		{
			using ImmediateActionQueue queue = CreateActionQueue(inputActions, actionArtifactCache, logger);
			int actionLimit = Math.Min(NumParallelProcesses, queue.TotalActions);
			queue.CreateAutomaticRunner(action => RunActionLocal(queue, action), bUseActionWeights, actionLimit, NumParallelProcesses);
			ImmediateActionQueueRunner remoteRunner = queue.CreateManualRunner(action => RunActionRemote(queue, action));

			// Setup a notification that alerts uba when an artifact has been read from the cache
			queue.OnArtifactsRead = (action) =>
			{
				HashSet<DirectoryItem> refreshedDirectories = new();
				foreach (FileItem output in action.ProducedItems)
				{
					if (refreshedDirectories.Add(output.Directory))
					{
						session.RefreshDirectories(output.Directory.FullName);
					}
				}
			};

			// Start the queue
			queue.Start();

			// Handle process available from remote
			session!.RemoteProcessSlotAvailable += (sender, args) =>
			{
				if (!queue.TryStartOneAction(remoteRunner) && (_bIsRemoteActionsAllowed && !UBAConfig.bForceBuildAllRemote))
				{
					uint count = ActionsLeftThatCanRunRemotely(queue);

					// We didn't find an action to start, let's check how many queued items are left.. if there are less than NumParallelProcesses, then disconnect
					if (count <= NumParallelProcesses)
					{
						_bIsRemoteActionsAllowed = false;
						foreach (var coordinator in _agentCoordinators)
						{
							coordinator.Stop();
						}
						session!.DisableRemoteExecution();
					}
					else
					{
						// Tell UBA max number of remote processes left. Providing this information makes it possible for UBA to start disconnecting clients
						session!.SetMaxRemoteProcessCount(count);
					}
				}
			};

			session!.RemoteProcessReturned += (sender, args) =>
			{
				args.Process.Cancel(true);
				LinkedAction action = (LinkedAction)args.Process.UserData!;
				//logger.LogInformation("REQUEUING " + action.ProducedItems.FirstOrDefault()!.Name);
				queue.RequeueAction(action);
			};

			// Add all actions we can add
			queue.StartManyActions();

			try
			{
				foreach (var coordinator in _agentCoordinators)
				{
					coordinator.Start(queue, CanRunRemotely);
				}

				bool res = queue.RunTillDone().Result; // Using inline wait to avoid possible thread switch
				return res;
			}
			finally
			{
				foreach (var coordinator in _agentCoordinators)
				{
					coordinator.Stop();
				}
			}
		}


		/// <summary>
		/// Determine if an action must be run locally and with no detouring
		/// </summary>
		/// <param name="action">The action to check</param>
		/// <returns>If this action must be local, non-detoured</returns>
		bool ForceLocalNoDetour(LinkedAction action)
		{
			// Don't let Mac run shell commands through Uba as interposing dylibs into
			// the shell results in dyld errors about no matching architecture.
			// The shell is used to run various commands during a build like copy/ditto.
			// So for these actions we need to make sure UBA is not used.
			// Linking is similarly currently not working in Uba on Mac
			bool bIsShellAction = action.CommandPath == BuildHostPlatform.Current.Shell;
			bool bIsLinkAction = action.ActionType == ActionType.Link;
			return System.OperatingSystem.IsMacOS() && (bIsShellAction || bIsLinkAction);
		}


		/// <summary>
		/// Determine if an action is able to be run remotely
		/// </summary>
		/// <param name="action">The action to check</param>
		/// <returns>If this action can be run remotely</returns>
		bool CanRunRemotely(LinkedAction action) => 
			action.bCanExecuteInUBA && 
			action.bCanExecuteRemotely && 
			!_localRetryActions.ContainsKey(action) && 
			!_forcedRetryActions.ContainsKey(action) && 
			(UBAConfig.bLinkRemote || action.ActionType != ActionType.Link) &&
			!ForceLocalNoDetour(action);

		ProcessStartInfo GetActionStartInfo(LinkedAction action, out FileItem? pchItem)
		{
			ProcessStartInfo startInfo = new()
			{
				Application = action.CommandPath.FullName,
				WorkingDirectory = action.WorkingDirectory.FullName,
				Arguments = action.CommandArguments,
				Priority = ProcessPriority,
				OutputStatsThresholdMs = (uint)UBAConfig.OutputStatsThresholdMs,
				UserData = action,
				Description = action.StatusDescription,
				Configuration = action.bIsGCCCompiler ? EpicGames.UBA.ProcessStartInfo.CommonProcessConfigs.CompileClang : EpicGames.UBA.ProcessStartInfo.CommonProcessConfigs.CompileMsvc,
				LogFile = UBAConfig.bLogEnabled ? action.Inner.ProducedItems.First().Location.GetFileName() : null,
			};

			bool usingLtcg = true; // ltcg linking checks what pch was used and it seems like it needs to be identical in other ways than timestamp
			pchItem = action.Inner.ProducedItems.FirstOrDefault(item => item.Name.EndsWith(".pch", StringComparison.OrdinalIgnoreCase));
			if (pchItem != null)
			{
				startInfo.Priority = System.Diagnostics.ProcessPriorityClass.AboveNormal;
				if (!usingLtcg && action.ArtifactMode.HasFlag(ArtifactMode.PropagateInputs))
				{
					startInfo.TrackInputs = true;
				}
				else
				{
					pchItem = null;
				}
			}

			return startInfo;
		}

		Func<Task>? RunActionLocal(ImmediateActionQueue queue, LinkedAction action)
		{
			if (UBAConfig.bForceBuildAllRemote && CanRunRemotely(action))
			{
				return null;
			}

			return () =>
			{
				bool enableDetour = !ForceLocalNoDetour(action) && action.bCanExecuteInUBA && !_forcedRetryActions.ContainsKey(action);

				ProcessStartInfo startInfo = GetActionStartInfo(action, out FileItem? pchItem);
				using (IProcess process = _session!.RunProcess(startInfo, false, null, enableDetour))
				{
					if (process.ExitCode != 0 && UBAConfig.bForcedRetry)
					{
						_threadedLogger.LogWarning("{Description} {StatusDescription}: Exited with error code {ExitCode}. This action will retry without UBA", action.CommandDescription, action.StatusDescription, process.ExitCode);
						_forcedRetryActions.AddOrUpdate(action, false, (k, v) => false);
						queue.RequeueAction(action);
						return Task.CompletedTask;
					}

					if (!enableDetour && process.ExitCode == 0)
					{
						_session!.RegisterNewFiles(action.ProducedItems.Where(x => FileReference.Exists(x.Location)).Select(x => x.FullName).ToArray());
					}

					TimeSpan processorTime = process.TotalProcessorTime;
					TimeSpan executionTime = process.TotalWallTime;
					List<string> logLines = process.LogLines;
					logLines.RemoveAll((line) => line.StartsWith("   Creating library ", StringComparison.OrdinalIgnoreCase) && line.EndsWith(".exp", StringComparison.OrdinalIgnoreCase) || line.EndsWith("file(s) copied.", StringComparison.OrdinalIgnoreCase));

					string? additionalDescription = null;
					if (!enableDetour)
					{
						additionalDescription = "(UBA disabled)";
					}

					ActionFinished(queue, new ExecuteResults(logLines, process.ExitCode, executionTime, processorTime, additionalDescription), action, pchItem, process);
				}
				return Task.CompletedTask;
			};
		}

		Func<Task>? RunActionRemote(ImmediateActionQueue queue, LinkedAction action)
		{
			if (!CanRunRemotely(action))
			{
				return null;
			}

			return () =>
			{
				uint knownInputsCount = 0;
				byte[]? knownInputs = null;
				if (UBAConfig.bUseKnownInputs)
				{
					int sizeOfChar = System.OperatingSystem.IsWindows() ? 2 : 1;

					int byteCount = 0;
					foreach (var item in action.PrerequisiteItems)
					{
						byteCount += (item.FullName.Length + 1) * sizeOfChar;
						++knownInputsCount;
					}

					knownInputs = new byte[byteCount + sizeOfChar];

					int byteOffset = 0;
					foreach (var item in action.PrerequisiteItems)
					{
						var str = item.FullName;
						int strBytes = str.Length * sizeOfChar;
						if (sizeOfChar == 1) // Unmanaged size uses ascii
							System.Buffer.BlockCopy(System.Text.Encoding.ASCII.GetBytes(str.ToCharArray()), 0, knownInputs, byteOffset, strBytes);
						else
							System.Buffer.BlockCopy(str.ToCharArray(), 0, knownInputs, byteOffset, strBytes);
						byteOffset += strBytes + sizeOfChar;
					}
				}

				ProcessStartInfo startInfo = GetActionStartInfo(action, out FileItem? pchItem);
				_session!.RunProcessRemote(startInfo, (s, e) =>
				{
					if (e.ExitCode != 0 && !e.LogLines.Any())
					{
						RemoteActionFailedNoOutput(queue, action, e.ExitCode, e.ExecutingHost ?? "Unknown");
						return;
					}

					string additionalDescription = $"[RemoteExecutor: {e.ExecutingHost}]";
					TimeSpan processorTime = e.TotalProcessorTime;
					TimeSpan executionTime = e.TotalWallTime;
					List<string> logLines = e.LogLines;
					logLines.RemoveAll((line) => line.StartsWith("   Creating library ", StringComparison.OrdinalIgnoreCase) && line.EndsWith(".exp", StringComparison.OrdinalIgnoreCase));
					ActionFinished(queue, new ExecuteResults(logLines, e.ExitCode, executionTime, processorTime, additionalDescription), action, pchItem, s as IProcess);
				}, action.Weight, knownInputs, knownInputsCount);
				return Task.CompletedTask;
			};
		}

		uint ActionsLeftThatCanRunRemotely(ImmediateActionQueue queue)
		{
			lock (_actionsChangedLock)
			{
				if (_bActionsChanged)
				{
					_actionsQueuedThatCanRunRemotely = queue.GetQueuedActionsCount(CanRunRemotely);
					_bActionsChanged = false;
				}
				return _actionsQueuedThatCanRunRemotely;
			}
		}

		protected void ActionFinished(ImmediateActionQueue queue, ExecuteResults results, LinkedAction action, FileItem? pchItem = null, IProcess? process = null)
		{
			if (_bIsCancelled)
			{
				return;
			}

			bool success = results.ExitCode == 0;
			if (pchItem != null && process != null && success)
			{
				_session!.SetCustomCasKeyFromTrackedInputs(pchItem.FullName, action.WorkingDirectory.FullName, process);
			}

			queue.OnActionCompleted(action, success, results);

			lock (_actionsChangedLock)
			{
				_bActionsChanged = true;
			}
		}

		void RemoteActionFailedNoOutput(ImmediateActionQueue queue, LinkedAction action, int exitCode, string executingHost)
		{
			if (_bIsCancelled)
			{
				return;
			}

			_threadedLogger.LogWarning("{Description} {StatusDescription} [RemoteExecutor: {ExecutingHost}]: Exited with error code {ExitCode} with no output. This action will retry locally", action.CommandDescription, action.StatusDescription, executingHost, exitCode);
			_localRetryActions.AddOrUpdate(action, false, (k, v) => false);
			queue.RequeueAction(action);

			lock (_actionsChangedLock)
			{
				_bActionsChanged = true;
			}
		}
	}

	/// <summary>
	/// UBAExecutor, but force local only compiles regardless of other settings
	/// </summary>
	class UBALocalExecutor : UBAExecutor
	{
		public override string Name => "Unreal Build Accelerator local";

		public static new bool IsAvailable()
		{
			return EpicGames.UBA.Utils.IsAvailable();
		}

		public UBALocalExecutor(int maxLocalActions, bool bAllCores, bool bCompactOutput, Microsoft.Extensions.Logging.ILogger logger, CommandLineArguments? additionalArguments = null)
			: base(maxLocalActions, bAllCores, bCompactOutput, logger, additionalArguments)
		{
			UBAConfig.bDisableRemote = true;
			UBAConfig.bForceBuildAllRemote = false;
			UBAConfig.Zone = "local";
		}
	}
}
