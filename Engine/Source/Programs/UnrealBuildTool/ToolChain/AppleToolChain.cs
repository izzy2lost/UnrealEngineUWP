// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	/// <summary>
	/// Helper class for managing Xcode paths, versions, etc. Helps to differentiate between Apple xcode platforms
	/// </summary>
	public abstract class AppleToolChainSettings
	{
		/// <summary>
		/// Which developer directory to root from? If this is "xcode-select", UBT will query for the currently selected Xcode
		/// </summary>
		public static DirectoryReference XcodeDeveloperDir => _XcodeDeveloperDir!;

		// This is not expected to be used before we've made any Settings options, so XcodeDeveloperDir will assert if it's used too early
		private static DirectoryReference? _XcodeDeveloperDir = null;

		/// <summary>
		/// Directory for the developer binaries
		/// </summary>
		public DirectoryReference ToolchainDir;

		/// <summary>
		/// A portion of the target "tuple"
		/// </summary>
		private string TargetOSName;

		/// <summary>
		/// The version of the iOS SDK to target at build time.
		/// </summary>
		public string SDKVersion;

		/// <summary>
		/// The version in a floating point value for easy comparison
		/// </summary>
		public readonly float SDKVersionFloat = 0.0f;

		/// <summary>
		/// Cache SDK dir for device and simulator (for non-Mac)
		/// </summary>
		DirectoryReference SDKDir;
		DirectoryReference? SimulatorSDKDir = null;

		/// <summary>
		/// Constructor, called by platform sybclasses
		/// </summary>
		/// <param name="OSPrefix">SDK name (like "MacOSX")</param>
		/// <param name="SimulatorOSPrefix">Sinulator SDK name (like "iPhoneOSSimulator")</param>
		/// <param name="TargetOSName">Platform name used in the -target parameter</param>
		/// <param name="bVerbose"></param>
		/// <param name="Logger"></param>
		protected AppleToolChainSettings(string OSPrefix, string? SimulatorOSPrefix, string TargetOSName, bool bVerbose, ILogger Logger)
		{
			this.TargetOSName = TargetOSName;

			if (_XcodeDeveloperDir == null)
			{
				SelectXcode(bVerbose, Logger);
			}

			// set up directories
			ToolchainDir = DirectoryReference.Combine(XcodeDeveloperDir, "Toolchains/XcodeDefault.xctoolchain/usr/bin");
			SDKDir = DirectoryReference.Combine(XcodeDeveloperDir, $"Platforms/{OSPrefix}.platform/Developer/SDKs/{OSPrefix}.sdk");
			if (SimulatorOSPrefix != null)
			{   
				SimulatorSDKDir = DirectoryReference.Combine(XcodeDeveloperDir, $"Platforms/{SimulatorOSPrefix}.platform/Developer/SDKs/{SimulatorOSPrefix}.sdk");
			}

			// use one up fro the SDK dir to look in for the version'd name directories next to the SDK dir
			SDKVersion = SelectSDK(SDKDir.ParentDirectory! , OSPrefix, bVerbose, Logger);

			// convert to float for easy comparison
			SDKVersionFloat = Single.Parse(SDKVersion, System.Globalization.CultureInfo.InvariantCulture);
		}

		/// <summary>
		/// Get the path to the SDK diretory in xcode for the given architecture
		/// </summary>
		/// <param name="Architecture"></param>
		/// <returns></returns>
		public DirectoryReference GetSDKPath(UnrealArch Architecture)
		{
			// note that VisionOS uses IOSSimulator (as TVOS should eventually do as well)
			if (Architecture == UnrealArch.IOSSimulator || Architecture == UnrealArch.TVOSSimulator)
			{
				return SimulatorSDKDir!;
			}
			return SDKDir;
		}

		/// <summary>
		/// Gets the string used by xcode to taget a platform and version (will return something like "arm64-apple-ios17.0-simulator"
		/// </summary>
		/// <param name="Architecture"></param>
		/// <returns></returns>
		public string GetTargetTuple(UnrealArch Architecture)
		{
			string Prefix = Architecture.AppleName;
			string Suffix = (Architecture == UnrealArch.IOSSimulator || Architecture == UnrealArch.TVOSSimulator) ? "-simulator" : "";

			return $"{Prefix}-apple-{TargetOSName}{SDKVersion}{Suffix}";
		}

		private static void SelectXcode(bool bVerbose, ILogger Logger)
		{
			// on the Mac, run xcode-select directly.
			int ReturnCode;
			string XcodeSelectResult = Utils.RunLocalProcessAndReturnStdOut("xcode-select", "--print-path", null, out ReturnCode);
			if (ReturnCode != 0)
			{
				string? MinVersion = UEBuildPlatform.GetSDK(UnrealTargetPlatform.Mac)!.GetSDKInfo("Sdk")!.Min;
				throw new BuildException($"We were unable to find your build tools (via 'xcode-select --print-path'). Please install Xcode, version {MinVersion} or later");
			}

			_XcodeDeveloperDir = new DirectoryReference(XcodeSelectResult);

			// make sure we get a full path
			if (DirectoryReference.Exists(XcodeDeveloperDir) == false)
			{
				throw new BuildException("Selected Xcode ('{0}') doesn't exist, cannot continue.", XcodeDeveloperDir);
			}

			if (XcodeDeveloperDir.ContainsName("CommandLineTools", 0))
			{
				throw new BuildException($"Your Mac is set to use CommandLineTools for its build tools ({XcodeDeveloperDir}). Unreal expects Xcode as the build tools. Please install Xcode if it's not already, then do one of the following:\n" +
					"  - Run Xcode, go to Settings, and in the Locations tab, choose your Xcode in Command Line Tools dropdown.\n" +
					"  - In Terminal, run 'sudo xcode-select -s /Applications/Xcode.app' (or an alternate location if you installed Xcode to a non-standard location)\n" +
					"Either way, you will need to enter your Mac password.");
			}

			if (bVerbose && !XcodeDeveloperDir.FullName.StartsWith("/Applications/Xcode.app"))
			{
				Log.TraceInformationOnce("Compiling with non-standard Xcode: {0}", XcodeDeveloperDir);
			}

			// Installed engine requires Xcode 13
			if (Unreal.IsEngineInstalled())
			{
				string? InstalledSdkVersion = UnrealBuildBase.ApplePlatformSDK.InstalledSDKVersion;
				if (String.IsNullOrEmpty(InstalledSdkVersion))
				{
					throw new BuildException("Unable to get xcode version");
				}
				if (Int32.Parse(InstalledSdkVersion.Substring(0, 2)) < 13)
				{
					throw new BuildException("Building for macOS, iOS and tvOS requires Xcode 13.4.1 or newer, Xcode " + InstalledSdkVersion + " detected");
				}
			}
		}

		private static string SelectSDK(DirectoryReference BaseSDKDir, string OSPrefix, bool bVerbose, ILogger Logger)
		{
			string PlatformSDKVersion = "";
			try
			{
				// loop over the subdirs and parse out the version
				int MaxSDKVersionMajor = 0;
				int MaxSDKVersionMinor = 0;
				string? MaxSDKVersionString = null;
				foreach (DirectoryReference SubDir in DirectoryReference.EnumerateDirectories(BaseSDKDir))
				{
					string SubDirName = Path.GetFileNameWithoutExtension(SubDir.GetDirectoryName());
					if (SubDirName.StartsWith(OSPrefix))
					{
						// get the SDK version from the directory name
						string SDKString = SubDirName.Replace(OSPrefix, "");
						int Major = 0;
						int Minor = 0;

						// parse it into whole and fractional parts (since 10.10 > 10.9 in versions, but not in math)
						try
						{
							string[] Tokens = SDKString.Split(".".ToCharArray());
							if (Tokens.Length == 2)
							{
								Major = Int32.Parse(Tokens[0]);
								Minor = Int32.Parse(Tokens[1]);
							}
						}
						catch (Exception)
						{
							// weirdly formatted SDKs
							continue;
						}

						// update largest SDK version number
						if (Major > MaxSDKVersionMajor || (Major == MaxSDKVersionMajor && Minor > MaxSDKVersionMinor))
						{
							MaxSDKVersionString = SDKString;
							MaxSDKVersionMajor = Major;
							MaxSDKVersionMinor = Minor;
						}
					}
				}

				// use the largest version
				if (MaxSDKVersionString != null)
				{
					PlatformSDKVersion = MaxSDKVersionString;
				}
			}
			catch (Exception Ex)
			{
				// on any exception, just use the backup version
				Logger.LogInformation("Triggered an exception while looking for SDK directory in Xcode.app");
				Logger.LogInformation("{Ex}", Ex.ToString());
			}

			if (bVerbose && !ProjectFileGenerator.bGenerateProjectFiles)
			{
				Logger.LogInformation("Compiling with {Os} SDK {Sdk}", OSPrefix, PlatformSDKVersion);
			}

			return PlatformSDKVersion;
		}
	}

	abstract class AppleToolChain : ClangToolChain
	{
		protected class AppleToolChainInfo : ClangToolChainInfo
		{
			public AppleToolChainInfo(FileReference Clang, FileReference Archiver, ILogger Logger)
				: base(Clang, Archiver, Logger)
			{
			}

			// libtool doesn't provide version, just use clang's version string
			/// <inheritdoc/>
			protected override string QueryArchiverVersionString() => ClangVersionString;
		}

		public Lazy<AppleToolChainSettings> ToolChainSettings;

		protected FileReference? ProjectFile;

		protected bool bUseModernXcode => AppleExports.UseModernXcode(ProjectFile);

		public AppleToolChain(FileReference? InProjectFile, Func<AppleToolChainSettings> InCreateSettings, ClangToolChainOptions InOptions, ILogger InLogger) : base(InOptions, InLogger)
		{
			ProjectFile = InProjectFile;
			ToolChainSettings = new Lazy<AppleToolChainSettings>(InCreateSettings);
		}

		/// <summary>
		/// Takes an architecture string as provided by UBT for the target and formats it for Clang. Supports
		/// multiple architectures joined with '+'
		/// </summary>
		/// <param name="InArchitectures"></param>
		/// <returns></returns>
		protected string FormatArchitectureArg(UnrealArchitectures InArchitectures)
		{
			return "-arch " + String.Join('+', InArchitectures.Architectures.Select(x => x.AppleName));
		}

		protected DirectoryReference GetMacDevSrcRoot()
		{
			return Unreal.EngineSourceDirectory;
		}

		protected override CPPOutput CompileCPPFiles(CppCompileEnvironment CompileEnvironment, List<FileItem> InputFiles, DirectoryReference OutputDir, string ModuleName, IActionGraphBuilder Graph)
		{
			List<string> GlobalArguments = new();

			GetCompileArguments_Global(CompileEnvironment, GlobalArguments);

			List<FileItem> FrameworkTokenFiles = new List<FileItem>();
			foreach (UEBuildFramework Framework in CompileEnvironment.AdditionalFrameworks)
			{
				if (Framework.ZipFile != null)
				{
					// even in modern, we have to try to unzip in UBT, so that commandline builds extract the frameworks _before_ building (the Xcode finalize
					// happens after building). We still need the unzip in Xcode pre-build phase, because Xcode wants the frameworks around for its build graph
					ExtractFramework(Framework, Graph, Logger);
					FrameworkTokenFiles.Add(Framework.ExtractedTokenFile!);
				}
			}

			CPPOutput Result = new CPPOutput();
			// Create a compile action for each source file.
			foreach (FileItem SourceFile in InputFiles)
			{
				Action CompileAction = CompileCPPFile(CompileEnvironment, SourceFile, OutputDir, ModuleName, Graph, GlobalArguments, Result);
				CompileAction.PrerequisiteItems.UnionWith(FrameworkTokenFiles);
			}
			return Result;
		}

		protected void StripSymbolsWithXcode(FileReference SourceFile, FileReference TargetFile, DirectoryReference ToolchainDir)
		{
			if (SourceFile != TargetFile)
			{
				// Strip command only works in place so we need to copy original if target is different
				File.Copy(SourceFile.FullName, TargetFile.FullName, true);
			}

			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.FileName = Path.Combine(ToolchainDir.FullName, "strip");
			StartInfo.Arguments = String.Format("\"{0}\" -S", TargetFile.FullName);
			StartInfo.UseShellExecute = false;
			StartInfo.CreateNoWindow = true;
			Utils.RunLocalProcessAndLogOutput(StartInfo, Logger);
		}

		/// <summary>
		/// Writes a versions.xcconfig file for xcode to pull in when making an app plist
		/// </summary>
		/// <param name="LinkEnvironment"></param>
		/// <param name="Prerequisite">FileItem describing the Prerequisite that this will this depends on (executable or similar) </param>
		/// <param name="Graph">List of actions to be executed. Additional actions will be added to this list.</param>
		protected FileItem UpdateVersionFile(LinkEnvironment LinkEnvironment, FileItem Prerequisite, IActionGraphBuilder Graph)
		{
			FileItem DestFile;

			// Make the compile action
			Action UpdateVersionAction = Graph.CreateAction(ActionType.CreateAppBundle);
			UpdateVersionAction.WorkingDirectory = GetMacDevSrcRoot();
			UpdateVersionAction.CommandPath = BuildHostPlatform.Current.Shell;
			UpdateVersionAction.CommandDescription = "";

			// @todo programs right nhow are sharing the Engine build version - one reason for this is that we can't get to the Engine/Programs directory from here
			// (we can't even get to the Engine/Source/Programs directory without searching on disk), and if we did, we would create a _lot_ of Engine/Programs directories
			// on disk that don't exist in p4. So, we just re-use Engine version, not Project version
			//				DirectoryReference ProductDirectory = FindProductDirectory(ProjectFile, LinkEnvironment.OutputDirectory!, Graph.Makefile.TargetType);
			DirectoryReference ProductDirectory = (ProjectFile?.Directory) ?? Unreal.EngineDirectory;
			FileReference OutputVersionFile = FileReference.Combine(ProductDirectory, "Intermediate/Build/Versions.xcconfig");
			DestFile = FileItem.GetItemByFileReference(OutputVersionFile);

			// make path to the script
			FileItem BundleScript = FileItem.GetItemByFileReference(FileReference.Combine(Unreal.EngineDirectory, "Build/BatchFiles/Mac/UpdateVersionAfterBuild.sh"));
			UpdateVersionAction.CommandArguments = $"\"{BundleScript.AbsolutePath}\" {ProductDirectory} {LinkEnvironment.Platform}";
			UpdateVersionAction.PrerequisiteItems.Add(Prerequisite);
			UpdateVersionAction.PrerequisiteItems.Add(BundleScript);
			UpdateVersionAction.ProducedItems.Add(DestFile);
			UpdateVersionAction.StatusDescription = $"Updating version file: {OutputVersionFile}";

			return DestFile;
		}

		protected FileItem ExtractFramework(UEBuildFramework Framework, IActionGraphBuilder Graph, ILogger Logger)
		{
			if (Framework.ZipFile == null)
			{
				throw new BuildException("Unable to extract framework '{0}' - no zip file specified", Framework.Name);
			}
			if (!Framework.bHasMadeUnzipAction)
			{
				Framework.bHasMadeUnzipAction = true;

				FileItem InputFile = FileItem.GetItemByFileReference(Framework.ZipFile);

				StringBuilder ExtractScript = new StringBuilder();
				ExtractScript.AppendLine("#!/bin/sh");
				ExtractScript.AppendLine("set -e");
				// ExtractScript.AppendLine("set -x"); // For debugging
				ExtractScript.AppendLine(String.Format("[ -d {0} ] && rm -rf {0}", Utils.MakePathSafeToUseWithCommandLine(Framework.ZipOutputDirectory!.FullName)));
				ExtractScript.AppendLine(String.Format("unzip -q -o {0} -d {1}", Utils.MakePathSafeToUseWithCommandLine(Framework.ZipFile.FullName), Utils.MakePathSafeToUseWithCommandLine(Framework.ZipOutputDirectory.ParentDirectory!.FullName))); // Zip contains folder with the same name, hence ParentDirectory
				ExtractScript.AppendLine(String.Format("touch {0}", Utils.MakePathSafeToUseWithCommandLine(Framework.ExtractedTokenFile!.AbsolutePath)));

				FileItem ExtractScriptFileItem = Graph.CreateIntermediateTextFile(new FileReference(Framework.ZipOutputDirectory.FullName + ".sh"), ExtractScript.ToString());

				Action UnzipAction = Graph.CreateAction(ActionType.BuildProject);
				UnzipAction.CommandPath = new FileReference("/bin/sh");
				UnzipAction.CommandArguments = Utils.MakePathSafeToUseWithCommandLine(ExtractScriptFileItem.AbsolutePath);
				UnzipAction.WorkingDirectory = Unreal.EngineDirectory;
				UnzipAction.PrerequisiteItems.Add(InputFile);
				UnzipAction.PrerequisiteItems.Add(ExtractScriptFileItem);
				UnzipAction.ProducedItems.Add(Framework.ExtractedTokenFile);
				UnzipAction.DeleteItems.Add(Framework.ExtractedTokenFile);
				UnzipAction.StatusDescription = String.Format("Unzipping : {0} -> {1}", Framework.ZipFile, Framework.ZipOutputDirectory);
				UnzipAction.bCanExecuteRemotely = false;
			}
			return Framework.ExtractedTokenFile!;
		}

		/// <summary>
		/// If the project is a UnrealGame project, Target.ProjectDirectory refers to the engine dir, not the actual dir of the project. So this method gets the 
		/// actual directory of the project whether it is a UnrealGame project or not.
		/// </summary>
		/// <returns>The actual project directory.</returns>
		/// <param name="ProjectFile">The path to the project file</param>
		internal static DirectoryReference GetActualProjectDirectory(FileReference? ProjectFile)
		{
			DirectoryReference ProjectDirectory = (ProjectFile == null ? Unreal.EngineDirectory : DirectoryReference.FromFile(ProjectFile)!);
			return ProjectDirectory;
		}


		/// <inheritdoc/>
		protected override string EscapePreprocessorDefinition(string Definition)
		{
			return Definition.Contains('"') ? Definition.Replace("\"", "\\\"") : Definition;
		}

		protected override void GetCppStandardCompileArgument(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			if (CompileEnvironment.bEnableObjCAutomaticReferenceCounting)
			{
				Arguments.Add("-fobjc-arc");
			}

			base.GetCppStandardCompileArgument(CompileEnvironment, Arguments);
		}

		protected override void GetCompileArguments_CPP(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			Arguments.Add("-x objective-c++");
			GetCppStandardCompileArgument(CompileEnvironment, Arguments);
			Arguments.Add("-stdlib=libc++");
		}

		protected override void GetCompileArguments_MM(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			Arguments.Add("-x objective-c++");
			GetCppStandardCompileArgument(CompileEnvironment, Arguments);
			Arguments.Add("-stdlib=libc++");
		}

		protected override void GetCompileArguments_M(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			Arguments.Add("-x objective-c");
			GetCppStandardCompileArgument(CompileEnvironment, Arguments);
			Arguments.Add("-stdlib=libc++");
		}

		protected override void GetCompileArguments_PCH(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			Arguments.Add("-x objective-c++-header");
			GetCppStandardCompileArgument(CompileEnvironment, Arguments);
			Arguments.Add("-stdlib=libc++");
			Arguments.Add("-fpch-instantiate-templates");
		}

		/// <inheritdoc/>
		protected override void GetCompileArguments_WarningsAndErrors(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			base.GetCompileArguments_WarningsAndErrors(CompileEnvironment, Arguments);

			Arguments.Add("-Wno-unknown-warning-option");
			Arguments.Add("-Wno-range-loop-analysis");
			Arguments.Add("-Wno-single-bit-bitfield-constant-conversion");
		}

		/// <inheritdoc/>
		protected override void GetCompileArguments_Optimizations(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			base.GetCompileArguments_Optimizations(CompileEnvironment, Arguments);

			bool bStaticAnalysis = false;
			string? StaticAnalysisMode = Environment.GetEnvironmentVariable("CLANG_STATIC_ANALYZER_MODE");
			if (!String.IsNullOrEmpty(StaticAnalysisMode))
			{
				bStaticAnalysis = true;
			}

			// Optimize non- debug builds.
			if (CompileEnvironment.bOptimizeCode && !bStaticAnalysis)
			{
				// Don't over optimise if using AddressSanitizer or you'll get false positive errors due to erroneous optimisation of necessary AddressSanitizer instrumentation.
				if (Options.HasFlag(ClangToolChainOptions.EnableAddressSanitizer))
				{
					Arguments.Add("-O1");
					Arguments.Add("-g");
					Arguments.Add("-fno-optimize-sibling-calls");
					Arguments.Add("-fno-omit-frame-pointer");
				}
				else if (Options.HasFlag(ClangToolChainOptions.EnableThreadSanitizer))
				{
					Arguments.Add("-O1");
					Arguments.Add("-g");
				}
				else if (CompileEnvironment.OptimizationLevel == OptimizationMode.Size)
				{
					Arguments.Add("-Oz");
				}
				else if (CompileEnvironment.OptimizationLevel == OptimizationMode.SizeAndSpeed)
				{
					Arguments.Add("-Os");

					if (CompileEnvironment.Architecture == UnrealArch.Arm64)
					{
						Arguments.Add("-moutline");
					}
				}
				else
				{
					Arguments.Add("-O3");
				}
			}
			else
			{
				Arguments.Add("-O0");
			}
		}

		/// <inheritdoc/>
		protected override void GetCompileArguments_Debugging(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			base.GetCompileArguments_Debugging(CompileEnvironment, Arguments);

			// Create DWARF format debug info if wanted,
			if (CompileEnvironment.bCreateDebugInfo)
			{
				Arguments.Add("-gdwarf-4");
			}
		}

		/// <inheritdoc/>
		protected override void GetCompileArguments_Analyze(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			base.GetCompileArguments_Analyze(CompileEnvironment, Arguments);

			// Disable all clang tidy checks
			Arguments.Add($"-Xclang -analyzer-tidy-checker=-*");
		}

		/// <inheritdoc/>
		protected override void GetCompileArguments_Global(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			base.GetCompileArguments_Global(CompileEnvironment, Arguments);

			Arguments.Add(GetRTTIFlag(CompileEnvironment));

			Arguments.Add("-fmessage-length=0");
			Arguments.Add("-fpascal-strings");
		}

		public override CppCompileEnvironment CreateSharedResponseFile(CppCompileEnvironment CompileEnvironment, FileReference OutResponseFile, IActionGraphBuilder Graph)
		{
			// Temporarily turn of shared response files for apple toolchains
			return CompileEnvironment;
		}

		protected override FileItem GetCompileArguments_FileType(CppCompileEnvironment CompileEnvironment, FileItem SourceFile, DirectoryReference OutputDir, List<string> Arguments, Action CompileAction, CPPOutput CompileResult)
		{
			FileItem Output = base.GetCompileArguments_FileType(CompileEnvironment, SourceFile, OutputDir, Arguments, CompileAction, CompileResult);

			// @todo hack fix this better - it's so that files can see the generated -Swift.h headers - we don't know when we need them at this point
			if (CompileEnvironment.Platform == UnrealTargetPlatform.VisionOS)
			{
				if (!SourceFile.HasExtension(".swift") && SourceFile.Location.ContainsName("Launch", 0))
				{
					FileItem OutputInteropHeader = GetBridgingHeader("UESwift", OutputDir);
					Arguments.Add(GetUserIncludePathArgument(OutputInteropHeader.GetDirectoryItem().Location));
					CompileAction.PrerequisiteItems.Add(OutputInteropHeader);
				}
			}
			return Output;
		}

		private FileItem GetBridgingHeader(string SourceFile, DirectoryReference OutputDir)
		{
			string Filename = $"{Path.GetFileNameWithoutExtension(SourceFile)}-Swift.h";
			return FileItem.GetItemByFileReference(FileReference.Combine(OutputDir, "Bridging", Filename));
		}

		protected override Action CompileCPPFile(CppCompileEnvironment CompileEnvironment, FileItem SourceFile, DirectoryReference OutputDir, string ModuleName, IActionGraphBuilder Graph, IReadOnlyCollection<string> GlobalArguments, CPPOutput Result)
		{
			if (SourceFile.HasExtension(".swift"))
			{
				return CompileSwiftFile(CompileEnvironment, SourceFile, OutputDir, Graph, Result);
			}

			return base.CompileCPPFile(CompileEnvironment, SourceFile, OutputDir, ModuleName, Graph, GlobalArguments, Result);
		}

		private Action CompileSwiftFile(CppCompileEnvironment CompileEnvironment, FileItem SourceFile, DirectoryReference OutputDir, IActionGraphBuilder Graph, CPPOutput CompileResult)
		{
			FileItem OutputFile = FileItem.GetItemByFileReference(FileReference.Combine(OutputDir, GetFileNameFromExtension(SourceFile.Name, ".o")));

			List<string> Arguments = new();
			Arguments.Add($"\"{SourceFile}\"");

			// output file settings
			Arguments.Add("-emit-object"); // same as -c
			Arguments.Add($"-parse-as-library"); // don't create a main function
			Arguments.Add($"-o \"{OutputFile}\"");

			// platform settings
			Arguments.Add($"-target {ToolChainSettings.Value.GetTargetTuple(CompileEnvironment.Architecture)}");
			Arguments.Add($"-sdk {ToolChainSettings.Value.GetSDKPath(CompileEnvironment.Architecture)}");

			// misc settings copied from Xcode
			Arguments.Add("-Xllvm");
			Arguments.Add("-aarch64-use-tbi");
			Arguments.Add("-stack-check");
			Arguments.Add($"-swift-version 5");
			
			Arguments.Add($"-module-name Launch");

			Action CompileAction = Graph.CreateAction(ActionType.Compile);
			CompileAction.Weight = CompileActionWeight;
			CompileAction.CommandArguments = String.Join(" ", Arguments);
			CompileAction.CommandPath = FileReference.Combine(ToolChainSettings.Value.ToolchainDir, "swift-frontend");
			CompileAction.PrerequisiteItems.Add(SourceFile);
			CompileAction.ProducedItems.Add(OutputFile);
			CompileResult.ObjectFiles.Add(OutputFile);
			CompileAction.WorkingDirectory = Unreal.EngineSourceDirectory;
			CompileAction.CommandDescription = "Compile";
			UnrealArchitectureConfig ArchConfig = UnrealArchitectureConfig.ForPlatform(CompileEnvironment.Platform);
			if (ArchConfig.Mode != UnrealArchitectureMode.SingleArchitecture)
			{
				string ReadableArch = ArchConfig.ConvertToReadableArchitecture(CompileEnvironment.Architecture);
				CompileAction.CommandDescription += $" [{ReadableArch}]";
			}
			CompileAction.StatusDescription = Path.GetFileName(SourceFile.AbsolutePath);
			CompileAction.bIsGCCCompiler = true;
			CompileAction.bCanExecuteRemotely = true;


			FileItem OutputInteropHeader = GetBridgingHeader(SourceFile.FullName, OutputDir);

			// obj-c bridging header settings
			Arguments.Clear();
			Arguments.Add($"\"{SourceFile}\"");
			Arguments.Add("-parse"); // this will allow it to generate the header without writing out any .o/executable
			Arguments.Add("-emit-objc-header");
			Arguments.Add($"-emit-objc-header-path \"{OutputInteropHeader}\"");

			// platform settings
			Arguments.Add($"-target {ToolChainSettings.Value.GetTargetTuple(CompileEnvironment.Architecture)}");
			Arguments.Add($"-sdk {ToolChainSettings.Value.GetSDKPath(CompileEnvironment.Architecture)}");

			Arguments.Add($"-swift-version 5");

			Arguments.Add($"-module-name Launch");

			// now make an action to export the swift code as a header Obj-C bridging
			Action HeaderAction = Graph.CreateAction(ActionType.CompileModuleInterface);
			HeaderAction.CommandPath = FileReference.Combine(ToolChainSettings.Value.ToolchainDir, "swiftc");
			HeaderAction.CommandArguments = String.Join(" ", Arguments);
			HeaderAction.PrerequisiteItems.Add(SourceFile);
			HeaderAction.ProducedItems.Add(OutputInteropHeader);
			HeaderAction.CommandDescription = "Generate Header";
			HeaderAction.StatusDescription = Path.GetFileName(OutputInteropHeader.AbsolutePath);
			if (ArchConfig.Mode != UnrealArchitectureMode.SingleArchitecture)
			{
				string ReadableArch = ArchConfig.ConvertToReadableArchitecture(CompileEnvironment.Architecture);
				CompileAction.CommandDescription += $" [{ReadableArch}]";
			}

			HeaderAction.WorkingDirectory = CompileAction.WorkingDirectory;
			HeaderAction.bIsGCCCompiler = CompileAction.bIsGCCCompiler;
			HeaderAction.bCanExecuteRemotely = CompileAction.bCanExecuteRemotely;

			Console.WriteLine($"{CompileAction.CommandPath} {CompileAction.CommandArguments}");

			// this is likely ignored, but the Compile action is the important one
			return CompileAction;
		}

		protected string GetDsymutilPath(ILogger Logger, out string ExtraOptions, bool bIsForLTOBuild = false)
		{
			FileReference DsymutilLocation = new FileReference("/usr/bin/dsymutil");

			// dsymutil before 10.0.1 has a bug that causes issues, it's fixed in autosdks but not everyone has those set up so for the timebeing we have
			// a version in P4 - first determine if we need it
			string DsymutilVersionString = Utils.RunLocalProcessAndReturnStdOut(DsymutilLocation.FullName, "-version", Logger);

			bool bUseInstalledDsymutil = true;
			int Major = 0, Minor = 0, Patch = 0;

			// tease out the version number
			string[] Tokens = DsymutilVersionString.Split(" ".ToCharArray());

			// sanity check
			if (Tokens.Length < 4 || Tokens[3].Contains('.') == false)
			{
				Log.TraceInformationOnce("Unable to parse dsymutil version out of: {0}", DsymutilVersionString);
			}
			else
			{
				string[] Versions = Tokens[3].Split(".".ToCharArray());
				if (Versions.Length < 3)
				{
					Log.TraceInformationOnce("Unable to parse version token: {0}", Tokens[3]);
				}
				else
				{
					if (!Int32.TryParse(Versions[0], out Major) || !Int32.TryParse(Versions[1], out Minor) || !Int32.TryParse(Versions[2], out Patch))
					{
						Log.TraceInformationOnce("Unable to parse version tokens: {0}", Tokens[3]);
					}
				}
			}

			// if the installed one is too old, use a fixed up one if it can
			if (bUseInstalledDsymutil == false)
			{
				FileReference PatchedDsymutilLocation = FileReference.Combine(Unreal.EngineDirectory, "Restricted/NotForLicensees/Binaries/Mac/LLVM/bin/dsymutil");

				if (File.Exists(PatchedDsymutilLocation.FullName))
				{
					DsymutilLocation = PatchedDsymutilLocation;
				}

				DirectoryReference? AutoSdkDir;
				if (UEBuildPlatformSDK.TryGetHostPlatformAutoSDKDir(out AutoSdkDir))
				{
					FileReference AutoSdkDsymutilLocation = FileReference.Combine(AutoSdkDir, "Mac", "LLVM", "bin", "dsymutil");
					if (FileReference.Exists(AutoSdkDsymutilLocation))
					{
						DsymutilLocation = AutoSdkDsymutilLocation;
					}
				}
			}

			// 10.0.1 has an issue with LTO builds where we need to limit the number of threads
			ExtraOptions = (bIsForLTOBuild && Major == 10 && Minor == 0 && Patch == 1) ? "-j 1" : "";
			return DsymutilLocation.FullName;
		}

		public override ICollection<FileItem> PostBuild(ReadOnlyTargetRules Target, FileItem Executable, LinkEnvironment BinaryLinkEnvironment, IActionGraphBuilder Graph)
		{
			List<FileItem> OutputFiles = new List<FileItem>(base.PostBuild(Target, Executable, BinaryLinkEnvironment, Graph));

			bool bIsBuildingAppBundle = !BinaryLinkEnvironment.bIsBuildingDLL && !BinaryLinkEnvironment.bIsBuildingLibrary && !BinaryLinkEnvironment.bIsBuildingConsoleApplication;
			if (AppleExports.UseModernXcode(Target.ProjectFile) && bIsBuildingAppBundle)
			{
				Action PostBuildAction = ApplePostBuildSyncMode.CreatePostBuildSyncAction(Target, Executable, BinaryLinkEnvironment.IntermediateDirectory!, Graph);

				PostBuildAction.PrerequisiteItems.UnionWith(OutputFiles);
				OutputFiles.AddRange(PostBuildAction.ProducedItems);

				OutputFiles.Add(UpdateVersionFile(BinaryLinkEnvironment, FileItem.GetItemByFileReference(BinaryLinkEnvironment.OutputFilePath), Graph));
			}

			return OutputFiles;
		}

		#region Stub Xcode Projects

		internal static bool GenerateProjectFiles(FileReference? ProjectFile, string[] Arguments, string? SingleTargetName, ILogger Logger, out DirectoryReference? XcodeProjectFile)
		{
			ProjectFileGenerator.bGenerateProjectFiles = true;
			try
			{
				CommandLineArguments CmdLine = new CommandLineArguments(Arguments);

				PlatformProjectGeneratorCollection PlatformProjectGenerators = new PlatformProjectGeneratorCollection();
				PlatformProjectGenerators.RegisterPlatformProjectGenerator(UnrealTargetPlatform.Mac, new MacProjectGenerator(CmdLine, Logger), Logger);
				PlatformProjectGenerators.RegisterPlatformProjectGenerator(UnrealTargetPlatform.IOS, new IOSProjectGenerator(CmdLine, Logger), Logger);
				PlatformProjectGenerators.RegisterPlatformProjectGenerator(UnrealTargetPlatform.TVOS, new TVOSProjectGenerator(CmdLine, Logger), Logger);

				XcodeProjectFileGenerator Generator = new XcodeProjectFileGenerator(ProjectFile, CmdLine);
				// this could be improved if ProjectFileGenerator constructor took a Arguments param, and it could parse it there instead of the GenerateProjectFilesMode
				Generator.SingleTargetName = SingleTargetName;
				// don't need the editor data since these are stub projects
				bool bSucces = Generator.GenerateProjectFiles(PlatformProjectGenerators, Arguments, bCacheDataForEditor: false, Logger);
				XcodeProjectFile = Generator.XCWorkspace;
				return bSucces;
			}
			catch (Exception ex)
			{
				XcodeProjectFile = null;
				Logger.LogError(ex.ToString());
			}
			finally
			{
				ProjectFileGenerator.bGenerateProjectFiles = false;
			}
			return false;
		}

		/// <summary>
		/// Genearate an run-only Xcode project, that is not meant to be used for anything else besides code-signing/running/etc of the native .app bundle
		/// </summary>
		/// <param name="UProjectFile">Location of .uproject file (or null for the engine project</param>
		/// <param name="Platform">The platform to generate a project for</param>
		/// <param name="TargetName">The name of the target being built, so we can generate a more minimal project</param>
		/// <param name="bForDistribution">True if this is making a bild for uploading to app store</param>
		/// <param name="Logger">Logging object</param>
		/// <param name="GeneratedProjectFile">Returns the .xcworkspace that was made</param>
		internal static void GenerateRunOnlyXcodeProject(FileReference? UProjectFile, UnrealTargetPlatform Platform, string TargetName, bool bForDistribution, ILogger Logger, out DirectoryReference? GeneratedProjectFile)
		{
			List<string> Options = new()
			{
				$"-platforms={Platform}",
				"-DeployOnly",
				"-NoIntellisense",
				"-NoDotNet",
				"-IgnoreJunk",
				bForDistribution ? "-distribution" : "-development",
				"-IncludeTempTargets",
				"-projectfileformat=XCode",
				"-automated",
			};

			if (!String.IsNullOrEmpty(TargetName))
			{
				Options.Add($"-singletarget={TargetName}");
			}

			if (UProjectFile == null || UProjectFile.IsUnderDirectory(Unreal.EngineDirectory))
			{
				// @todo do we need these? where would the bundleid come from if there's no project?
				//				Options.Add("-bundleID=" + BundleID);
				//				Options.Add("-appname=" + AppName);
				// @todo add an option to only add Engine target?
			}
			else
			{
				Options.Add($"-project=\"{UProjectFile.FullName}\"");
				Options.Add("-game");
			}

			// we need to be in Engine/Source for some build.cs files
			string CurrentCWD = Environment.CurrentDirectory;
			Environment.CurrentDirectory = DirectoryReference.Combine(Unreal.EngineDirectory, "Source").FullName;
			GenerateProjectFiles(UProjectFile, Options.ToArray(), TargetName, Logger, out GeneratedProjectFile);
			Environment.CurrentDirectory = CurrentCWD;
		}

		internal static int FinalizeAppWithXcode(DirectoryReference XcodeProject, UnrealTargetPlatform Platform, string SchemeName, string Configuration, string Action, string ExtraOptions, ILogger Logger)
		{
			List<string> Arguments = new()
			{
				"UBT_NO_POST_DEPLOY=true",
				FileReference.Combine(AppleToolChainSettings.XcodeDeveloperDir, "usr/bin/xcodebuild").FullName,
				Action,
				$"-workspace \"{XcodeProject.FullName}\"",
				$"-scheme \"{SchemeName}\"",
				$"-configuration \"{Configuration}\"",
				$"-destination generic/platform={AppleExports.GetDestinationPlatform(Platform)}",
				"-hideShellScriptEnvironment",
				// xcode gets confused it we _just_ wrote out entitlements while generating the temp project, and it thinks it was modified _during_ building
				// but it wasn't, it was written before the build started
				"CODE_SIGN_ALLOW_ENTITLEMENTS_MODIFICATION=YES",
				ExtraOptions,
				//$"-sdk {SDKName}",
			};

			Process LocalProcess = new Process();
			LocalProcess.StartInfo = new ProcessStartInfo("/usr/bin/env", String.Join(" ", Arguments));
			LocalProcess.OutputDataReceived += (Sender, Args) => { LocalProcessOutput(Args, false, Logger); };
			LocalProcess.ErrorDataReceived += (Sender, Args) =>
			{
				if (Args != null && Args.Data != null
				&& Args.Data.Contains("Failed to load profile") && Args.Data.Contains("<stdin>"))
				{
					Logger.LogInformation("Silencing the following provision profile error, it is not affecting code signing:");
					LocalProcessOutput(Args, false, Logger);
				}
				else
				{
					LocalProcessOutput(Args, true, Logger);
				}
			};
			return Utils.RunLocalProcess(LocalProcess);
		}

		static void LocalProcessOutput(DataReceivedEventArgs? Args, bool bIsError, ILogger Logger)
		{
			if (Args != null && Args.Data != null)
			{
				if (bIsError)
				{
					Logger.LogError("{Message}", Args.Data.TrimEnd());
				}
				else
				{
					Logger.LogInformation("{Message}", Args.Data.TrimEnd());
				}
			}
		}

		#endregion
	};

	[Serializable]
	class ApplePostBuildSyncTarget
	{
		public FileReference? ProjectFile;
		public UnrealTargetPlatform Platform;
		public UnrealTargetConfiguration Configuration;
		public string TargetName;

		// For iOS/TVOS
		public bool bCreateStubIPA;
		public FileReference StubOutputPath;

		public ApplePostBuildSyncTarget(ReadOnlyTargetRules Target, FileItem Executable)
		{
			Platform = Target.Platform;
			Configuration = Target.Configuration;
			ProjectFile = Target.ProjectFile;
			TargetName = Target.Name;

			bCreateStubIPA = Target.IOSPlatform.bCreateStubIPA;
			StubOutputPath = Executable.Location;
		}
	}

	[ToolMode("ApplePostBuildSync", ToolModeOptions.XmlConfig | ToolModeOptions.BuildPlatforms)]
	class ApplePostBuildSyncMode : ToolMode
	{
		[CommandLine("-Input=", Required = true)]
		public FileReference? InputFile = null;

		[CommandLine("-XmlConfigCache=")]
		public FileReference? XmlConfigCache = null;

		// this isn't actually used, but is helpful to pass -modernxcode along in CreatePostBuildSyncAction, and UBT won't
		// complain that nothing is using it, because where we _do_ use it is outside the normal cmdline parsing functionality
		[CommandLine("-ModernXcode")]
		public bool bModernXcode;

		public override Task<int> ExecuteAsync(CommandLineArguments Arguments, ILogger Logger)
		{
			Arguments.ApplyTo(this);
			Arguments.CheckAllArgumentsUsed();

			// Run the PostBuildSync command
			ApplePostBuildSyncTarget Target = BinaryFormatterUtils.Load<ApplePostBuildSyncTarget>(InputFile!);
			int ExitCode = PostBuildSync(Target, Logger);

			return Task.FromResult(ExitCode);
		}

		private int PostBuildSync(ApplePostBuildSyncTarget Target, ILogger Logger)
		{
			// generate the IOS plist file every time
			if (Target.Platform.IsInGroup(UnrealPlatformGroup.IOS))
			{
				string GameName = Target.ProjectFile == null ? "UnrealGame" : Target.ProjectFile.GetFileNameWithoutAnyExtensions();
				// most of these params are uused in modern
				UEDeployIOS.GenerateIOSPList(Target.ProjectFile, Target.Configuration, AppleToolChain.GetActualProjectDirectory(Target.ProjectFile).FullName, Target.ProjectFile == null, GameName, bIsClient: false,
					GameName, Unreal.EngineDirectory.FullName, "", null, null, false, Logger);
			}

			// if xcode is building this, it will also do the Run stuff anyway, so no need to do it here as well
			if (Environment.GetEnvironmentVariable("UE_BUILD_FROM_XCODE") == "1")
			{
				return 0;
			}

			int ExitCode = AppleExports.BuildWithStubXcodeProject(Target.ProjectFile, Target.Platform, Target.Configuration, Target.TargetName, AppleExports.XcodeBuildMode.PostBuildSync, Logger);
			if (ExitCode != 0)
			{
				Logger.LogError("ERROR: Failed to finalize the .app with Xcode. Check the log for more information");
			}

			if ((Target.Platform == UnrealTargetPlatform.IOS || Target.Platform == UnrealTargetPlatform.TVOS)
				&& Target.bCreateStubIPA)
			{
				IOSToolChain.PackageStub(Target.StubOutputPath.Directory.FullName, Target.TargetName, Target.StubOutputPath.GetFileNameWithoutExtension(), true);
			}

			return ExitCode;
		}

		private static FileItem GetPostBuildOutputFile(FileReference Executable, string TargetName, UnrealTargetPlatform Platform)
		{
			FileReference StagedExe;
			if (Platform == UnrealTargetPlatform.Mac)
			{
				StagedExe = FileReference.Combine(Executable.Directory, Executable.GetFileName() + ".app/Contents/PkgInfo");
			}
			else
			{
				StagedExe = FileReference.Combine(Executable.Directory, TargetName + ".app", TargetName);
			}
			return FileItem.GetItemByFileReference(StagedExe);
		}

		public static Action CreatePostBuildSyncAction(ReadOnlyTargetRules Target, FileItem Executable, DirectoryReference IntermediateDir, IActionGraphBuilder Graph)
		{
			ApplePostBuildSyncTarget PostBuildSync = new(Target, Executable);
			FileReference PostBuildSyncFile = FileReference.Combine(IntermediateDir!, "PostBuildSync.dat");
			BinaryFormatterUtils.Save(PostBuildSyncFile, PostBuildSync);

			string PostBuildSyncArguments = String.Format("-modernxcode -Input=\"{0}\" -XmlConfigCache=\"{1}\" -remoteini=\"{2}\"", PostBuildSyncFile, XmlConfig.CacheFile, UnrealBuildTool.GetRemoteIniPath());
			Action PostBuildSyncAction = Graph.CreateRecursiveAction<ApplePostBuildSyncMode>(ActionType.CreateAppBundle, PostBuildSyncArguments);

			PostBuildSyncAction.WorkingDirectory = Unreal.EngineSourceDirectory;
			PostBuildSyncAction.PrerequisiteItems.Add(Executable);
			PostBuildSyncAction.ProducedItems.Add(GetPostBuildOutputFile(Executable.Location, Target.Name, Target.Platform));
			PostBuildSyncAction.StatusDescription = $"Executing PostBuildSync [{Executable.Location}]";
			PostBuildSyncAction.bCanExecuteRemotely = false;

			if (Target.Platform == UnrealTargetPlatform.IOS || Target.Platform == UnrealTargetPlatform.TVOS)
			{
				// @todo: do we need one per Target for IOS? Client? I dont think so for Modern
				FileReference PlistFile = FileReference.Combine(AppleToolChain.GetActualProjectDirectory(Target.ProjectFile), "Build/IOS/UBTGenerated/Info.Template.plist");
				PostBuildSyncAction.ProducedItems.Add(FileItem.GetItemByFileReference(PlistFile));

				if (PostBuildSync.bCreateStubIPA)
				{
					FileReference StubFile = FileReference.Combine(Executable.Directory.Location, Executable.Location.GetFileNameWithoutExtension() + ".stub");
					PostBuildSyncAction.ProducedItems.Add(FileItem.GetItemByFileReference(StubFile));
				}
			}

			return PostBuildSyncAction;
		}
	}
}
