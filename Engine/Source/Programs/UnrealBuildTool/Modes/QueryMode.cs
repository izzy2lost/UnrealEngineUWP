// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.CodeAnalysis.CSharp.Syntax;
using Microsoft.Extensions.Logging;
using OpenTracing.Util;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	// TODO: Namespacing?
	enum QueryType
	{
		Capabilities,
		AvailableTargets,
		TargetDetails,
	}
	internal class TargetIntellisenseInfo
	{
		internal class CompileSettings
		{
			public List<string> IncludePaths { get; set; } = new();
			public List<string> Defines { get; set; } = new();
			public string? Standard { get; set; }
			public List<string> ForcedIncludes { get; set; } = new();
			public string? CompilerPath { get; set; }
			public List<string> CompilerArgs { get; set; } = new();
			public string? WindowsSdkVersion { get; set; }
		}

		public Dictionary<UEBuildModule, CompileSettings> ModuleToCompileSettings = new();
		public Dictionary<DirectoryReference, UEBuildModule> DirToModule = new();

		public UEBuildModule? FindModuleForFile(FileReference File)
		{
			DirectoryReference? Dir = File.Directory;
			while (Dir != null)
			{
				if (DirToModule.TryGetValue(Dir, out UEBuildModule? Module))
				{
					return Module;
				}
				Dir = Dir.ParentDirectory;
			}
			return null;
		}
		public UEBuildModule? FindModuleForDirectory(DirectoryReference Directory)
		{
			DirectoryReference? Dir = Directory;
			while (Dir != null)
			{
				if (DirToModule.TryGetValue(Dir, out UEBuildModule? Module))
				{
					return Module;
				}
				Dir = Dir.ParentDirectory;
			}
			return null;
		}
	}

	internal class LaunchSettings
	{
		public string? Description { get; set; }
		public string? BinaryPath { get; set; }
		public List<string> Arguments { get; set; } = new();
	}

	internal class TargetConfigs
	{
		public string ProjectPath { get; set; } = "";
		public List<string> Platforms { get; set; } = new();
		public List<string> Configurations { get; set; } = new();
	}

	[ToolMode("Query", ToolModeOptions.BuildPlatforms | ToolModeOptions.XmlConfig | ToolModeOptions.UseStartupTraceListener)]
	class QueryMode : ToolMode
	{
		[CommandLine("-LogDirectory=")]
		public DirectoryReference? LogDirectory = null;

		[CommandLine("-OutputPath=")]
		public FileReference? OutputPath = null;

		[CommandLine("-Query=")]
		public QueryType? Query = null;

		[CommandLine("-IncludeEngineSource=")]
		public bool bIncludeEngineSource = true;

		[CommandLine("-Target=")]
		public string? TargetName;

		[CommandLine("-Configuration=")]
		public string? TargetConfiguration;

		[CommandLine("-Platform=")]
		public string? TargetPlatform;

		[CommandLine("-Indented")]
		public bool bIndented;

		private BuildConfiguration BuildConfiguration = new();

		public override Task<int> ExecuteAsync(CommandLineArguments Arguments, ILogger Logger)
		{
			Arguments.ApplyTo(this);

			if (LogDirectory == null)
			{
				LogDirectory = DirectoryReference.Combine(Unreal.EngineProgramSavedDirectory, "UnrealBuildTool", "QueryMode");
			}

			DirectoryReference.CreateDirectory(LogDirectory);

			string LogFileName;
			switch (Query)
			{
				case QueryType.TargetDetails:
					LogFileName = $"Log_{TargetName}_{TargetConfiguration}_{TargetPlatform}.txt";
					break;
				default:
					LogFileName = $"Log_{Query}.txt";
					break;

			}

			FileReference LogFile = FileReference.Combine(LogDirectory, LogFileName);
			Log.AddFileWriter("DefaultLogTraceListener", LogFile);

			XmlConfig.ApplyTo(BuildConfiguration);
			Arguments.ApplyTo(BuildConfiguration);

			// TODO: Document this hack 
			ProjectFileGenerator.bGenerateProjectFiles = true;

			if (Query == null)
			{
				return Task.FromResult(0);
			}

			JsonSerializerOptions ResponseOptions = new JsonSerializerOptions
			{
				PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
				WriteIndented = bIndented,
			};
			switch (Query)
			{
				case QueryType.Capabilities:
					Logger.LogInformation("QueryCapabilities");
					return Task.FromResult(QueryCapabilities(Arguments, Logger, ResponseOptions));
				case QueryType.AvailableTargets:
					Logger.LogInformation("QueryAvailableTargets");
					return Task.FromResult(QueryAvailableTargets(Arguments, Logger, ResponseOptions));
				case QueryType.TargetDetails:
					Logger.LogInformation("QueryTargetDetails");
					return Task.FromResult(QueryTargetDetails(Arguments, Logger, ResponseOptions));
			}

			return Task.FromResult(0);
		}

		private void WriteResults(object? Value, JsonSerializerOptions JsonOptions, ILogger Logger)
		{
			if (OutputPath == null)
			{
				Console.WriteLine(JsonSerializer.Serialize(Value, JsonOptions));
				return;
			}
			using FileStream Stream = new FileStream(OutputPath.FullName, FileMode.Create, FileAccess.Write);
			Logger.LogInformation("Writing {File}...", OutputPath.FullName);
			JsonSerializer.Serialize(Stream, Value, JsonOptions);
		}

		private int QueryCapabilities(CommandLineArguments Arguments, ILogger Logger, JsonSerializerOptions JsonOptions)
		{
			var Reply = new
			{
				Queries = new List<string> { QueryType.Capabilities.ToString(), QueryType.AvailableTargets.ToString(), QueryType.TargetDetails.ToString() }
			};
			WriteResults(Reply, JsonOptions, Logger);
			return 0;
		}

		private int QueryAvailableTargets(CommandLineArguments Arguments, ILogger Logger, JsonSerializerOptions JsonOptions)
		{
			try
			{
				GenerateProjectFilesMode.TryParseProjectFileArgument(Arguments, Logger, out FileReference? ProjectFileArg);

				List<UnrealTargetPlatform> Platforms = new();
				foreach (UnrealTargetPlatform Platform in UnrealTargetPlatform.GetValidPlatforms())
				{
					// If there is a build platform present, add it to the SupportedPlatforms list
					UEBuildPlatform? BuildPlatform;
					if (UEBuildPlatform.TryGetBuildPlatform(Platform, out BuildPlatform))
					{
						if (InstalledPlatformInfo.IsValidPlatform(Platform, EProjectType.Code))
						{
							Platforms.Add(Platform);
						}
					}
				}

				List<UnrealTargetConfiguration> AllowedTargetConfigurations = new List<UnrealTargetConfiguration>();
				AllowedTargetConfigurations = Enum.GetValues(typeof(UnrealTargetConfiguration)).Cast<UnrealTargetConfiguration>().ToList();

				List<string> Configurations = new();
				foreach (UnrealTargetConfiguration CurConfiguration in AllowedTargetConfigurations)
				{
					if (CurConfiguration != UnrealTargetConfiguration.Unknown)
					{
						if (InstalledPlatformInfo.IsValidConfiguration(CurConfiguration, EProjectType.Code))
						{
							Configurations.Add(CurConfiguration.ToString());
						}
					}
				}

				List<FileReference> Projects = ProjectFileArg != null ? new List<FileReference>(new[] { ProjectFileArg }) : NativeProjects.EnumerateProjectFiles(Logger).ToList();
				List<FileReference> AllTargetFiles = ProjectFileGenerator.DiscoverTargets(Projects, Logger, null, Platforms, bIncludeEngineSource: bIncludeEngineSource, bIncludeTempTargets: false);

				// TODO: Check valid configurations/platforms per target 
				Dictionary<string, TargetConfigs> Targets = new();
				string? DefaultTarget = null;
				foreach (FileReference TargetFilePath in AllTargetFiles)
				{
					string TargetName = TargetFilePath.GetFileNameWithoutAnyExtensions();
					FileReference? ProjectPath = Projects.FirstOrDefault(p => TargetFilePath.IsUnderDirectory(p.Directory));
					Targets.Add(TargetName, new TargetConfigs() { ProjectPath = ProjectPath?.ToString() ?? "", Configurations = Configurations, Platforms = Platforms.Select(x => x.ToString()).ToList() });
					if (DefaultTarget == null || TargetName == "UnrealEditor")
					{
						DefaultTarget = TargetName;
					}
				}

				var Reply = new
				{
					Targets = Targets,

					DefaultTarget = DefaultTarget,
					DefaultPlatform = Platforms[0].ToString(),
					DefaultConfiguration = UnrealTargetConfiguration.Development.ToString(),
				};

				WriteResults(Reply, JsonOptions, Logger);
				return 0;
			}
			catch (Exception e)
			{
				Logger.LogError("Failed to query available targets: {0}", e.Message);
				return 1;
			}
		}
		private int QueryTargetDetails(CommandLineArguments Arguments, ILogger Logger, JsonSerializerOptions JsonOptions)
		{
			if (TargetName == null)
			{
				Logger.LogError("Missing argument Target");
				return 1;
			}
			else if (TargetConfiguration == null)
			{
				Logger.LogError("Missing argument Configuration");
				return 1;
			}
			else if (TargetPlatform == null)
			{
				Logger.LogError("Missing argument Platform");
				return 1;
			}

			GenerateProjectFilesMode.TryParseProjectFileArgument(Arguments, Logger, out FileReference? ProjectFileArg);
			List<string> RawArgs = new List<string> { TargetName, TargetConfiguration, TargetPlatform };
			if (ProjectFileArg != null)
			{
				RawArgs.Add(ProjectFileArg.ToString());
			}
			RawArgs.AddRange(Arguments.GetUnusedArguments());
			CommandLineArguments Args = new CommandLineArguments(RawArgs.ToArray());
			List<TargetDescriptor> TargetDescriptors = new();

			TargetDescriptor.ParseSingleCommandLine(Args, false, false, false, TargetDescriptors, Logger);
			if (TargetDescriptors.Count == 0)
			{
				// TOOD: Error 
				return 1;
			}

			HashSet<string> BrowseConfigurationFolders = new HashSet<string>();
			try
			{
				UEBuildTarget CurrentTarget;
				using (GlobalTracer.Instance.BuildSpan("UEBuildTarget.Create()").StartActive())
				{
					bool bUsePrecompiled = false;

					// Prevent multiple conflicting processes building TargetRules at the same time
					string MutexName = SingleInstanceMutex.GetUniqueMutexForPath("UnrealBuildTool_QueryMode_UEBuildTarget-Create", Unreal.RootDirectory.FullName);
					using (new SingleInstanceMutex(MutexName, true))
					{
						CurrentTarget = UEBuildTarget.Create(TargetDescriptors[0], false, false, bUsePrecompiled, Logger);
					}
				}

				LaunchSettings? CurrentLaunchSettings = null;
				TargetIntellisenseInfo CurrentTargetIntellisenseInfo = new TargetIntellisenseInfo();
				GetBrowseConfigurationResponse CurrentBrowseConfiguration = new GetBrowseConfigurationResponse { Success = true };

				// Partially duplicated from UEBuildTarget.Build because we just want to get C++ compile actions without running UHT
				// or generating link actions / full dependency graph 
				CppConfiguration CppConfiguration = UEBuildTarget.GetCppConfiguration(CurrentTarget.Configuration);
				SourceFileMetadataCache MetadataCache = SourceFileMetadataCache.CreateHierarchy(null, Logger);
				CppCompileEnvironment GlobalCompileEnvironment = new CppCompileEnvironment(CurrentTarget.Platform, CppConfiguration, CurrentTarget.Architectures, MetadataCache);
				LinkEnvironment GlobalLinkEnvironment = new LinkEnvironment(GlobalCompileEnvironment.Platform, GlobalCompileEnvironment.Configuration, GlobalCompileEnvironment.Architectures);

				UEToolChain TargetToolChain = CurrentTarget.CreateToolchain(CurrentTarget.Platform, Logger);
				TargetToolChain.SetEnvironmentVariables();
				CurrentTarget.SetupGlobalEnvironment(TargetToolChain, GlobalCompileEnvironment, GlobalLinkEnvironment);

				CurrentBrowseConfiguration.WindowsSdkVersion = CurrentTarget.Rules.WindowsPlatform.WindowsSdkVersion;

				// TODO: For installed builds, filter out all the binaries that aren't in mods
				foreach (UEBuildBinary Binary in CurrentTarget.Binaries)
				{
					if (Binary.Type == UEBuildBinaryType.Executable && CurrentLaunchSettings == null && Binary.OutputFilePaths.Count == 1)
					{
						CurrentLaunchSettings = new LaunchSettings();
						CurrentLaunchSettings.Description = $"{CurrentTarget.TargetName} {CurrentTarget.Configuration} {CurrentTarget.Platform}";
						CurrentLaunchSettings.BinaryPath = Binary.OutputFilePath.ToString();
						if (CurrentTarget.ProjectFile != null && CurrentTarget.TargetType != TargetType.Program)
						{
							CurrentLaunchSettings.Arguments.Add(CurrentTarget.ProjectFile.ToString());
						}
					}

					HashSet<UEBuildModule> LinkEnvironmentVisitedModules = new HashSet<UEBuildModule>();
					CppCompileEnvironment BinaryCompileEnvironment = Binary.CreateBinaryCompileEnvironment(GlobalCompileEnvironment);
					CurrentBrowseConfiguration.Standard = BinaryCompileEnvironment.CppStandard.ToString();

					foreach (UEBuildModuleCPP Module in Binary.Modules.OfType<UEBuildModuleCPP>())
					{
						if (Module.Binary != null && Module.Binary != Binary)
						{
							continue;
						}

						CppCompileEnvironment ModuleCompileEnvironment = Module.CreateModuleCompileEnvironment(CurrentTarget.Rules, BinaryCompileEnvironment, Logger);
						foreach (DirectoryReference Dir in Module.ModuleDirectories)
						{
							BrowseConfigurationFolders.Add(Dir.ToString());
							CurrentTargetIntellisenseInfo.DirToModule.TryAdd(Dir, Module);
						}
						if (Module.GeneratedCodeDirectory != null)
						{
							CurrentTargetIntellisenseInfo.DirToModule.TryAdd(Module.GeneratedCodeDirectory, Module);
						}

						foreach (DirectoryReference Dir in ModuleCompileEnvironment.SystemIncludePaths)
						{
							BrowseConfigurationFolders.Add(Dir.ToString());
						}
						foreach (DirectoryReference Dir in ModuleCompileEnvironment.UserIncludePaths)
						{
							BrowseConfigurationFolders.Add(Dir.ToString());
						}

						TargetIntellisenseInfo.CompileSettings Settings = new TargetIntellisenseInfo.CompileSettings();
						if (TargetToolChain is VCToolChain TargetVCToolChain)
						{
							Settings.IncludePaths.AddRange(TargetVCToolChain.GetVCIncludePaths().Select(x => x.ToString()));
						}
						Settings.IncludePaths.AddRange(ModuleCompileEnvironment.SystemIncludePaths.Select(x => x.ToString()));
						Settings.IncludePaths.AddRange(ModuleCompileEnvironment.UserIncludePaths.Select(x => x.ToString()));
						Settings.Defines = ModuleCompileEnvironment.Definitions;
						Settings.Standard = ModuleCompileEnvironment.CppStandard.ToString();
						Settings.ForcedIncludes = ModuleCompileEnvironment.ForceIncludeFiles.Select(x => x.ToString()).ToList();
						Settings.CompilerPath = TargetToolChain.GetCppCompilerPath()?.ToString();
						Settings.CompilerArgs = TargetToolChain.GetGlobalCommandLineArgs(ModuleCompileEnvironment).ToList();
						Settings.WindowsSdkVersion = CurrentTarget.Rules.WindowsPlatform.WindowsSdkVersion;
						CurrentTargetIntellisenseInfo.ModuleToCompileSettings.Add(Module, Settings);
					}
				}

				CurrentBrowseConfiguration.Paths = BrowseConfigurationFolders.ToList();

				var Result = new
				{
					DirToModule = CurrentTargetIntellisenseInfo.DirToModule.ToDictionary(x => x.Key.ToString(), x => x.Value.Name),
					ModuleToCompileSettings = CurrentTargetIntellisenseInfo.ModuleToCompileSettings.ToDictionary(x => x.Key.Name, x => x.Value),
					LaunchSettings = CurrentLaunchSettings,
				};

				WriteResults(Result, JsonOptions, Logger);
				return 0;
			}
			catch (Exception e)
			{
				Logger.LogError("Caught exception setting up target: {0}", e);
				return 1;
			}
		}
	}
}
