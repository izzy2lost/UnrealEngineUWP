// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;

using Microsoft.Win32;

namespace UnrealBuildTool
{
	public static class Extension
	{
		public static void AddFormat(this List<string> list, string formatString, params object[] args)
		{
			list.Add(String.Format(formatString, args));
		}
	}

	// @ATG_CHANGE : BEGIN UWP support
	public class UniversalWindowsPlatformToolChain : UEToolChain
	{
		public UniversalWindowsPlatformToolChain(CPPTargetPlatform CppPlatform) 
			: base(CppPlatform)
		{
		}
		
		static void AppendCLArguments_Global(CPPEnvironment CompileEnvironment, VCEnvironment EnvVars, List<string> Arguments)
		{
			//Arguments.Append(" /showIncludes");

            // Suppress generation of object code for unreferenced inline functions. Enabling this option is more standards compliant, and causes a big reduction
            // in object file sizes (and link times) due to the amount of stuff we inline.
            Arguments.Add("/Zc:inline");

            if (BuildConfiguration.bEnableCodeAnalysis)
			{
				Arguments.Add("/analyze");

				// Don't cause analyze warnings to be errors
				Arguments.Add("/analyze:WX-");

				// Report functions that use a LOT of stack space.  You can lower this value if you
				// want more aggressive checking for functions that use a lot of stack memory.
				Arguments.Add("/analyze:stacksize81940");

				// Don't bother generating code, only analyze code (may report fewer warnings though.)
				//Arguments.Add("/analyze:only");
			}

			// Prevents the compiler from displaying its logo for each invocation.
			Arguments.Add("/nologo");

			// Enable intrinsic functions.
			Arguments.Add("/Oi");

			// Pack struct members on 8-byte boundaries.
			// Note: we do this even for 32bit builds because it's the required packing mode
			// for WinRT types, and currently enum class forward declarations (which are everywhere)
			// are interpreted as WinRT.
			Arguments.Add("/Zp8");

			if (CompileEnvironment.Config.Platform != CPPTargetPlatform.UWP64)
			{
				// Allow the compiler to generate SSE2 instructions. (On by default in 64bit)
				Arguments.Add("/arch:SSE2");
			}

			// Separate functions for linker.
			Arguments.Add("/Gy");

			// Relaxes floating point precision semantics to allow more optimization.
			Arguments.Add("/fp:fast");

			// Compile into an .obj file, and skip linking.
			Arguments.Add("/c");

			// Allow 900% of the default memory allocation limit.
			Arguments.Add("/Zm900");

			// Allow large object files to avoid hitting the 2^16 section limit when running with -StressTestUnity.
			Arguments.Add("/bigobj");

			// Disable "The file contains a character that cannot be represented in the current code page" warning for non-US windows.
			Arguments.Add("/wd4819");

			if (WindowsPlatform.Compiler >= WindowsCompiler.VisualStudio2015)
			{
				VCToolChain.AddDefinition(Arguments, "_CRT_STDIO_LEGACY_WIDE_SPECIFIERS", "1");
				//VCToolChain.AddDefinition(Arguments, "USE_SECURE_CRT", "1");
			}

			// @todo UWP: Disable "unreachable code" warning since auto-included vccorlib.h triggers it
			Arguments.Add("/wd4702");

			// Disable "usage of ATL attributes is deprecated" since WRL headers generate this
			Arguments.Add("/wd4467");

			// @todo UWP: Silence the hash_map deprecation errors for now. This should be replaced with unordered_map for the real fix.
			if (WindowsPlatform.Compiler >= WindowsCompiler.VisualStudio2015)
			{
				VCToolChain.AddDefinition(Arguments, "_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS");
			}

			if (BuildConfiguration.bUseSharedPCHs)
			{
				// @todo SharedPCH: Disable warning about PCH defines not matching .cpp defines.  We "cheat" these defines a little
				// bit to make shared PCHs work.  But it's totally safe.  Trust us.
				Arguments.Add("/wd4651");

				// @todo SharedPCH: Disable warning about redefining *API macros.  The PCH header is compiled with various DLLIMPORTs, but
				// when a module that uses that PCH header *IS* one of those imports, that module is compiled with EXPORTS, so the macro
				// is redefined on the command-line.  We need to clobber those defines to make shared PCHs work properly!
				Arguments.Add("/wd4005");
			}

			// If compiling as a DLL, set the relevant defines
			if (CompileEnvironment.Config.bIsBuildingDLL)
			{
				VCToolChain.AddDefinition(Arguments, "_WINDLL");
			}

			//
			//	Debug
			//
			if (CompileEnvironment.Config.Configuration == CPPTargetConfiguration.Debug)
			{
				// Disable compiler optimization.
				Arguments.Add("/Od");

				// Favor code size (especially useful for embedded platforms).
				Arguments.Add("/Os");

				// Allow inline method expansion unless E&C support is requested
				if( !BuildConfiguration.bSupportEditAndContinue )
				{
					// @todo UWP: No inlining in Debug builds except in the editor where DLL exports/imports aren't handled properly in module _API macros.
					if (UEBuildConfiguration.bBuildEditor)
					{
						Arguments.Add("/Ob2");
					}
				}

				if (BuildConfiguration.bDebugBuildsActuallyUseDebugCRT)
				{
					Arguments.Add("/RTC1");
				}
			}
			//
			//	Development and LTCG
			//
			else
			{
				// Maximum optimizations if desired.
				if (CompileEnvironment.Config.bOptimizeCode)
				{
					Arguments.Add("/Ox");

					// Allow optimized code to be debugged more easily.  This makes PDBs a bit larger, but doesn't noticeably affect
					// compile times.  The executable code is not affected at all by this switch, only the debugging information.
					if (EnvVars.CLExeVersion >= new Version("18.0.30723"))
					{
						// VC2013 Update 3 has a new flag for doing this
						Arguments.Add("/Zo");
					}
					else
					{
						Arguments.Add("/d2Zi+");
					}
				}

				// Favor code speed.
				Arguments.Add("/Ot");

				// Only omit frame pointers on the PC (which is implied by /Ox) if wanted.
				if (BuildConfiguration.bOmitFramePointers == false)
				{
					Arguments.Add("/Oy-");
				}

				// Allow inline method expansion
				Arguments.Add("/Ob2");

				//
				// LTCG
				//
				if (CompileEnvironment.Config.Configuration == CPPTargetConfiguration.Shipping)
				{
					if (BuildConfiguration.bAllowLTCG)
					{
						// Enable link-time code generation.
						Arguments.Add("/GL");
					}
				}
			}

			//
			//	PC
			//

			// Prompt the user before reporting internal errors to Microsoft.
			Arguments.Add("/errorReport:prompt");

			// Enable C++ exception handling, but not C exceptions.
			Arguments.Add("/EHsc");

			// If enabled, create debug information.
			if (CompileEnvironment.Config.bCreateDebugInfo)
			{
				// Store debug info in .pdb files.
				// @todo clang: PDB files are emited from Clang but do not fully work with Visual Studio yet (breakpoints won't hit due to "symbol read error")
				if (BuildConfiguration.bUsePDBFiles)
				{
					// Create debug info suitable for E&C if wanted.
					if (BuildConfiguration.bSupportEditAndContinue &&
						// We only need to do this in debug as that's the only configuration that supports E&C.
						CompileEnvironment.Config.Configuration == CPPTargetConfiguration.Debug)
					{
						Arguments.Add("/ZI");
					}
					// Regular PDB debug information.
					else
					{
						Arguments.Add("/Zi");
					}
					// We need to add this so VS won't lock the PDB file and prevent synchronous updates. This forces serialization through MSPDBSRV.exe.
					// See http://msdn.microsoft.com/en-us/library/dn502518.aspx for deeper discussion of /FS switch.
					if (BuildConfiguration.bUseIncrementalLinking && WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2013)
					{
						Arguments.Add("/FS");
					}
				}
				// Store C7-format debug info in the .obj files, which is faster.
				else
				{
					Arguments.Add("/Z7");
				}
			}

			// Specify the appropriate runtime library based on the platform and config.
			if (CompileEnvironment.Config.bUseStaticCRT)
			{
				if (CompileEnvironment.Config.Configuration == CPPTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT)
				{
					Arguments.Add("/MTd");
				}
				else
				{
					Arguments.Add("/MT");
				}
			}
			else
			{
				if (CompileEnvironment.Config.Configuration == CPPTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT)
				{
					Arguments.Add("/MDd");
				}
				else
				{
					Arguments.Add("/MD");
				}
			}

			
			VCToolChain.AddDefinition(Arguments, "_BUILD_FOR_STORE", "1");

			// These must be appended to the end of the system include list, lest they override some of the third party sources includes
			if (Directory.Exists(EnvVars.WindowsSDKExtensionDir))
			{
				CompileEnvironment.Config.CPPIncludeInfo.SystemIncludePaths.Add(string.Format(@"{0}\Include\{1}\ucrt", EnvVars.WindowsSDKExtensionDir, EnvVars.WindowsSDKExtensionHeaderLibVersion));
				CompileEnvironment.Config.CPPIncludeInfo.SystemIncludePaths.Add(string.Format(@"{0}\Include\{1}\um", EnvVars.WindowsSDKExtensionDir, EnvVars.WindowsSDKExtensionHeaderLibVersion));
				CompileEnvironment.Config.CPPIncludeInfo.SystemIncludePaths.Add(string.Format(@"{0}\Include\{1}\shared", EnvVars.WindowsSDKExtensionDir, EnvVars.WindowsSDKExtensionHeaderLibVersion));
				CompileEnvironment.Config.CPPIncludeInfo.SystemIncludePaths.Add(string.Format(@"{0}\Include\{1}\winrt", EnvVars.WindowsSDKExtensionDir, EnvVars.WindowsSDKExtensionHeaderLibVersion));
			}

			// Enable Windows Runtime extensions.  Do this even for libs (plugins) so that these too can consume WinRT APIs
			Arguments.Add("/ZW");

			// Don't automatically add metadata references.  We'll do that ourselves to avoid referencing windows.winmd directly:
			// we've hit problems where types are somehow in windows.winmd on some installations but not others, leading to either
			// missing or duplicated type references.
			Arguments.Add("/ZW:nostdlib");
			VCToolChain.AddDefinition(Arguments, "USE_WINRT_MAIN", "1");

			if (WindowsPlatform.Compiler >= WindowsCompiler.VisualStudio2015 &&
				Directory.Exists(Path.Combine(EnvVars.WindowsSDKExtensionDir, "References")))
			{
				Version SDKVersion = VCEnvironment.FindWindowsSDKExtensionLatestVersion(EnvVars.WindowsSDKExtensionDir);
				Arguments.AddFormat(@" /AI""{0}\References""", EnvVars.WindowsSDKExtensionDir);
				Arguments.AddFormat(@" /AI""{0}\References\{1}""", EnvVars.WindowsSDKExtensionDir, SDKVersion);

				// Use the latest version of contracts, consistent with our choice elsewhere to use the latest version of the SDK.
				// These metadata files should bring in everything available on the Universal family.  Extension SDKs should be
				// referenced directly by the modules that depend on them.
				Arguments.AddFormat(@" /FU""{0}""", VCEnvironment.GetLatestMetadataPathForApiContract("Windows.Foundation.FoundationContract"));
				Arguments.AddFormat(@" /FU""{0}""", VCEnvironment.GetLatestMetadataPathForApiContract("Windows.Foundation.UniversalApiContract"));
			}
			DirectoryReference PlatformWinMDLocation = VCEnvironment.GetCppCXMetadataLocation(WindowsPlatform.Compiler);
			if (PlatformWinMDLocation != null)
			{
				Arguments.AddFormat(@" /AI""{0}""", PlatformWinMDLocation);
				Arguments.AddFormat(@" /FU""{0}\platform.winmd""", PlatformWinMDLocation);
			}
		}

		static void AppendCLArguments_CPP(CPPEnvironment CompileEnvironment, List<string> Arguments)
		{
			// Explicitly compile the file as C++.
			Arguments.Add("/TP");

			if (!CompileEnvironment.Config.bEnableBufferSecurityChecks)
			{
				// This will disable buffer security checks (which are enabled by default) that the MS compiler adds around arrays on the stack,
				// Which can add some performance overhead, especially in performance intensive code
				// Only disable this if you know what you are doing, because it will be disabled for the entire module!
				Arguments.Add("/GS-");
			}

			if (CompileEnvironment.Config.bUseRTTI)
			{
				// Enable C++ RTTI.
				Arguments.Add("/GR");
			}
			else
			{
				// Disable C++ RTTI.
				Arguments.Add("/GR-");
			}

			// Level 4 warnings.
			Arguments.Add("/W3");
		}

		static void AppendCLArguments_C(List<string> Arguments)
		{
			// Explicitly compile the file as C.
			Arguments.Add("/TC");

			// Level 0 warnings.  Needed for external C projects that produce warnings at higher warning levels.
			Arguments.Add("/W0");
		}

		static void AppendLinkArguments(LinkEnvironment LinkEnvironment, List<string> Arguments)
		{
			// Don't create a side-by-side manifest file for the executable.
			Arguments.Add("/MANIFEST:NO");

			// Prevents the linker from displaying its logo for each invocation.
			Arguments.Add("/NOLOGO");

			if (LinkEnvironment.Config.bCreateDebugInfo)
			{
                // Output debug info for the linked executable.

                // Allow partial PDBs for faster linking
                if (WindowsPlatform.Compiler >= WindowsCompiler.VisualStudio2015 && BuildConfiguration.bUseFastPDBLinking)
                {
                    Arguments.Add("/DEBUG:FASTLINK");
                }
                else
                { 
                    Arguments.Add("/DEBUG");
				}
			}

			// Prompt the user before reporting internal errors to Microsoft.
			Arguments.Add("/errorReport:prompt");

			//
			//	PC
			//
			// Set machine type/ architecture to be 64 bit, and set as a store app
			if (LinkEnvironment.Config.Platform == CPPTargetPlatform.UWP64)
			{
				Arguments.Add("/MACHINE:x64");
			}
			Arguments.Add("/APPCONTAINER");
			// this helps with store API compliance validation tools, adding additional pdb info
			Arguments.Add("/PROFILE");

			if (LinkEnvironment.Config.bIsBuildingConsoleApplication)
			{
				Arguments.Add("/SUBSYSTEM:CONSOLE");
			}
			else
			{
				Arguments.Add("/SUBSYSTEM:WINDOWS");
			}

			if (LinkEnvironment.Config.bIsBuildingConsoleApplication && !LinkEnvironment.Config.bIsBuildingDLL && !String.IsNullOrEmpty(LinkEnvironment.Config.WindowsEntryPointOverride))
			{
				// Use overridden entry point
				Arguments.Add("/ENTRY:" + LinkEnvironment.Config.WindowsEntryPointOverride);
			}

			// Allow the OS to load the EXE at different base addresses than its preferred base address.
			Arguments.Add("/FIXED:No");

			// Explicitly declare that the executable is compatible with Data Execution Prevention.
			Arguments.Add("/NXCOMPAT");

			// Set the default stack size.
			Arguments.Add("/STACK:5000000,131072");

			// Allow delay-loaded DLLs to be explicitly unloaded.
			Arguments.Add("/DELAY:UNLOAD");

			if (LinkEnvironment.Config.bIsBuildingDLL)
			{
				Arguments.Add("/DLL");
			}

			// Don't embed the full PDB path; we want to be able to move binaries elsewhere. They will always be side by side.
			Arguments.Add("/PDBALTPATH:%_PDB%");

			//
			//	Shipping & LTCG
			//
			if (BuildConfiguration.bAllowLTCG &&
				LinkEnvironment.Config.Configuration == CPPTargetConfiguration.Shipping)
			{
				// Use link-time code generation.
				Arguments.Add("/LTCG");

				// This is where we add in the PGO-Lite linkorder.txt if we are using PGO-Lite
				//Arguments.Add("/ORDER:@linkorder.txt");
				//Arguments.Add("/VERBOSE");
			}

			//
			//	Shipping binary
			//
			if (LinkEnvironment.Config.Configuration == CPPTargetConfiguration.Shipping)
			{
				// Generate an EXE checksum.
				Arguments.Add("/RELEASE");

				// Eliminate unreferenced symbols.
				Arguments.Add("/OPT:REF");

				// Remove redundant COMDATs.
				Arguments.Add("/OPT:ICF");
			}
			//
			//	Regular development binary. 
			//
			else
			{
				// Keep symbols that are unreferenced.
				Arguments.Add("/OPT:NOREF");

				// Disable identical COMDAT folding.
				Arguments.Add("/OPT:NOICF");
			}

			// Enable incremental linking if wanted.
			if (BuildConfiguration.bUseIncrementalLinking)
			{
				Arguments.Add("/INCREMENTAL");
			}
			else
			{
				Arguments.Add("/INCREMENTAL:NO");
			}

			// Disable 
			//LINK : warning LNK4199: /DELAYLOAD:nvtt_64.dll ignored; no imports found from nvtt_64.dll
			// type warning as we leverage the DelayLoad option to put third-party DLLs into a 
			// non-standard location. This requires the module(s) that use said DLL to ensure that it 
			// is loaded prior to using it.
			Arguments.Add("/ignore:4199");

			// Suppress warnings about missing PDB files for statically linked libraries.  We often don't want to distribute
			// PDB files for these libraries.
			Arguments.Add("/ignore:4099");      // warning LNK4099: PDB '<file>' was not found with '<file>'
		}

		static void AppendLibArguments(LinkEnvironment LinkEnvironment, List<string> Arguments)
		{
			// Prevents the linker from displaying its logo for each invocation.
			Arguments.Add("/NOLOGO");

			// Prompt the user before reporting internal errors to Microsoft.
			Arguments.Add("/errorReport:prompt");

			//
			//	PC
			//
			// Set machine type/ architecture to be 64 bit.

			if (LinkEnvironment.Config.Platform == CPPTargetPlatform.UWP64)
			{
				Arguments.Add("/MACHINE:x64");
			}

			if (LinkEnvironment.Config.bIsBuildingConsoleApplication)
			{
				Arguments.Add("/SUBSYSTEM:CONSOLE");
			}
			else
			{
				Arguments.Add("/SUBSYSTEM:WINDOWS");
			}

			//
			//	Shipping & LTCG
			//
			if (LinkEnvironment.Config.Configuration == CPPTargetConfiguration.Shipping)
			{
				// Use link-time code generation.
				Arguments.Add("/LTCG");
			}

			// Ignore warning about /ZW in static libraries. It's not relevant since UE modules
			// have no reason to export new WinRT types, and ignoring it quiets noise when using
			// WinRT APIs from plugins.
			Arguments.Add("/ignore:4264");
		}

		public override CPPOutput CompileCPPFiles(CPPEnvironment CompileEnvironment, List<FileItem> SourceFiles, string ModuleName, ActionGraph ActionGraph)
		{
			var EnvVars = VCEnvironment.SetEnvironment(CompileEnvironment.Config.Platform, false);

            List<string> SharedArguments = new List<string>();
            AppendCLArguments_Global(CompileEnvironment, EnvVars, SharedArguments);

			// Add include paths to the argument list.
			foreach (string IncludePath in CompileEnvironment.Config.CPPIncludeInfo.IncludePaths)
			{
                VCToolChain.AddIncludePath(SharedArguments, IncludePath);
				//string IncludePathRelative = Utils.CleanDirectorySeparators(Utils.MakePathRelativeTo(IncludePath, Path.Combine(ProjectFileGenerator.RootRelativePath, "Engine/Source")), '/');
                //SharedArguments.AddFormat(" /I \"{0}\"", IncludePathRelative);
			}
			foreach (string IncludePath in CompileEnvironment.Config.CPPIncludeInfo.SystemIncludePaths)
			{
                VCToolChain.AddIncludePath(SharedArguments, IncludePath);
                //string IncludePathRelative = Utils.CleanDirectorySeparators(Utils.MakePathRelativeTo(IncludePath, Path.Combine(ProjectFileGenerator.RootRelativePath, "Engine/Source")), '/');
                //SharedArguments.AddFormat(" /I \"{0}\"", IncludePathRelative);
            }

			// @ATG_CHANGE : BEGIN winmd support
			// Add winmd references			
			foreach (string CurAssemblyInfo in CompileEnvironment.Config.WinMDReferences)
			{
                SharedArguments.AddFormat(" /FU \"{0}\"", CurAssemblyInfo);
			}
			// @ATG_CHANGE : END winmd support

			// Add preprocessor definitions to the argument list.
			foreach (string Definition in CompileEnvironment.Config.Definitions)
			{
				// Escape all quotation marks so that they get properly passed with the command line.
				var DefinitionArgument = Definition.Contains("\"") ? Definition.Replace("\"", "\\\"") : Definition;
				VCToolChain.AddDefinition(SharedArguments,  DefinitionArgument);
			}

			var BuildPlatform = UEBuildPlatform.GetBuildPlatformForCPPTargetPlatform(CompileEnvironment.Config.Platform);

			// Create a compile action for each source file.
			CPPOutput Result = new CPPOutput();
			foreach (FileItem SourceFile in SourceFiles)
			{
				Action CompileAction = ActionGraph.Add(ActionType.Compile);
				CompileAction.CommandDescription = "Compile";

                List<string> FileArguments = new List<string>();
				bool bIsPlainCFile = Path.GetExtension(SourceFile.AbsolutePath).ToUpperInvariant() == ".C";

				// Add the C++ source file and its included files to the prerequisite item list.
				AddPrerequisiteSourceFile(BuildPlatform, CompileEnvironment, SourceFile, CompileAction.PrerequisiteItems);

                bool bEmitsObjectFile = true;
				if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
				{
                    // Generate a CPP File that just includes the precompiled header.
                    FileReference PCHCPPPath = CompileEnvironment.Config.PrecompiledHeaderIncludeFilename.ChangeExtension(".cpp");
                    FileItem PCHCPPFile = FileItem.CreateIntermediateTextFile(
                        PCHCPPPath,
                        string.Format("#include \"{0}\"\r\n", CompileEnvironment.Config.PrecompiledHeaderIncludeFilename.FullName.Replace('\\', '/'))
                        );

                    // Make sure the original source directory the PCH header file existed in is added as an include
                    // path -- it might be a private PCH header and we need to make sure that its found!
                    string OriginalPCHHeaderDirectory = Path.GetDirectoryName(SourceFile.AbsolutePath);
                    FileArguments.AddFormat(" /I \"{0}\"", OriginalPCHHeaderDirectory);

                    var PrecompiledFileExtension = UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.UWP64).GetBinaryExtension(UEBuildBinaryType.PrecompiledHeader);
					// Add the precompiled header file to the produced items list.
					FileItem PrecompiledHeaderFile = FileItem.GetItemByFileReference(
						FileReference.Combine(
							CompileEnvironment.Config.OutputDirectory,
							Path.GetFileName(SourceFile.AbsolutePath) + PrecompiledFileExtension
							)
						);
					CompileAction.ProducedItems.Add(PrecompiledHeaderFile);
					Result.PrecompiledHeaderFile = PrecompiledHeaderFile;

					// Add the parameters needed to compile the precompiled header file to the command-line.
					FileArguments.AddFormat(" /Yc\"{0}\"", CompileEnvironment.Config.PrecompiledHeaderIncludeFilename);
					FileArguments.AddFormat(" /Fp\"{0}\"", PrecompiledHeaderFile.AbsolutePath);

					// If we're creating a PCH that will be used to compile source files for a library, we need
					// the compiled modules to retain a reference to PCH's module, so that debugging information
					// will be included in the library.  This is also required to avoid linker warning "LNK4206"
					// when linking an application that uses this library.
					if (CompileEnvironment.Config.bIsBuildingLibrary)
					{
						// NOTE: The symbol name we use here is arbitrary, and all that matters is that it is
						// unique per PCH module used in our library
					string FakeUniquePCHSymbolName = CompileEnvironment.Config.PrecompiledHeaderIncludeFilename.GetFileNameWithoutExtension();
						FileArguments.AddFormat(" /Yl{0}", FakeUniquePCHSymbolName);
					}

					FileArguments.AddFormat(" \"{0}\"", PCHCPPFile.AbsolutePath);

					CompileAction.StatusDescription = PCHCPPPath.GetFileName();
				}
				else
				{
					if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
					{
						CompileAction.bIsUsingPCH = true;
						CompileAction.PrerequisiteItems.Add(CompileEnvironment.PrecompiledHeaderFile);

                        FileArguments.Add(String.Format("/FI\"{0}\"", CompileEnvironment.Config.PrecompiledHeaderIncludeFilename.FullName));
                        FileArguments.Add(String.Format("/Yu\"{0}\"", CompileEnvironment.Config.PrecompiledHeaderIncludeFilename.FullName));
                        FileArguments.Add(String.Format("/Fp\"{0}\"", CompileEnvironment.PrecompiledHeaderFile.AbsolutePath));
                    }

                    // Add the source file path to the command-line.
                    FileArguments.AddFormat(" \"{0}\"", SourceFile.AbsolutePath);

					CompileAction.StatusDescription = Path.GetFileName(SourceFile.AbsolutePath);
				}

                foreach (FileReference ForceIncludeFile in CompileEnvironment.Config.ForceIncludeFiles)
                {
                    FileArguments.AddFormat(" /FI\"{0}\"", ForceIncludeFile.FullName);
                }

                if (bEmitsObjectFile)
				{
					var ObjectFileExtension = UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.UWP64).GetBinaryExtension(UEBuildBinaryType.Object);
					// Add the object file to the produced item list.
					FileItem ObjectFile = FileItem.GetItemByFileReference(
						FileReference.Combine(
							CompileEnvironment.Config.OutputDirectory,
							Path.GetFileName(SourceFile.AbsolutePath) + ObjectFileExtension
							)
						);
					CompileAction.ProducedItems.Add(ObjectFile);
					Result.ObjectFiles.Add(ObjectFile);
					FileArguments.AddFormat(" /Fo\"{0}\"", ObjectFile.AbsolutePath);
				}

				// Create PDB files if we were configured to do that.
				//
				// Also, when debug info is off and XGE is enabled, force PDBs, otherwise it will try to share
				// a PDB file, which causes PCH creation to be serial rather than parallel (when debug info is disabled)
				//		--> See https://udn.epicgames.com/lists/showpost.php?id=50619&list=unprog3
				if (BuildConfiguration.bUsePDBFiles ||
					(BuildConfiguration.bAllowXGE && !CompileEnvironment.Config.bCreateDebugInfo))
				{
					string PDBFileName;
					bool bActionProducesPDB = false;

					// All files using the same PCH are required to share the same PDB that was used when compiling the PCH
					if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
					{
                        PDBFileName = CompileEnvironment.Config.PrecompiledHeaderIncludeFilename.GetFileName();
                    }
                    // Files creating a PCH use a PDB per file.
                    else if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
					{
                        PDBFileName = CompileEnvironment.Config.PrecompiledHeaderIncludeFilename.GetFileName();
                        bActionProducesPDB = true;
					}
					// Ungrouped C++ files use a PDB per file.
					else if (!bIsPlainCFile)
					{
						PDBFileName = Path.GetFileName(SourceFile.AbsolutePath);
						bActionProducesPDB = true;
					}
					// Group all plain C files that doesn't use PCH into the same PDB
					else
					{
						PDBFileName = "MiscPlainC";
					}

						// Specify the PDB file that the compiler should write to.
					FileItem PDBFile = FileItem.GetItemByFileReference(
							FileReference.Combine(
									CompileEnvironment.Config.OutputDirectory,
									PDBFileName + ".pdb"
									)
								);
						FileArguments.AddFormat(" /Fd\"{0}\"", PDBFile.AbsolutePath);

						// Only use the PDB as an output file if we want PDBs and this particular action is
						// the one that produces the PDB (as opposed to no debug info, where the above code
						// is needed, but not the output PDB, or when multiple files share a single PDB, so
						// only the action that generates it should count it as output directly)
					if (BuildConfiguration.bUsePDBFiles && bActionProducesPDB)
						{
							CompileAction.ProducedItems.Add(PDBFile);
							Result.DebugDataFiles.Add(PDBFile);
						}
				}

				// Add C or C++ specific compiler arguments.
				if (bIsPlainCFile)
				{
					AppendCLArguments_C(FileArguments);
				}
				else
				{
					AppendCLArguments_CPP(CompileEnvironment, FileArguments);
				}

				CompileAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory.FullName;
				CompileAction.CommandPath = EnvVars.CompilerPath.FullName;

                //CompileAction.CommandArguments = Arguments.ToString() + FileArguments.ToString() + CompileEnvironment.Config.AdditionalArguments;
                List<string> UnrealCodeAnalyzerArguments = new List<string>();
                if (BuildConfiguration.bRunUnrealCodeAnalyzer)
                {
                    string UCAMode = BuildConfiguration.bUCACheckUObjectThreadSafety ? @"-CheckThreadSafety " : @"-CreateIncludeFiles ";
                    string ObjectFileExtension = BuildConfiguration.bUCACheckUObjectThreadSafety ? @".tsc" : @".includes";
                    FileItem ObjectFile = FileItem.GetItemByFileReference(FileReference.Combine(CompileEnvironment.Config.OutputDirectory, Path.GetFileName(SourceFile.AbsolutePath) + ObjectFileExtension));
                    string ClangPath = System.IO.Path.Combine(CompileAction.WorkingDirectory, @"ThirdParty", @"llvm", @"3.6.2", @"bin", @"vs2015", @"x86", @"release", @"clang++.exe");
                    UnrealCodeAnalyzerArguments.Add(UCAMode + SourceFile.AbsolutePath + @" -OutputFile=""" + ObjectFile.AbsolutePath + @""" -- " + ClangPath + @" --driver-mode=cl ");
                }

                string[] AdditionalArguments = String.IsNullOrEmpty(CompileEnvironment.Config.AdditionalArguments) ? new string[0] : new string[] { CompileEnvironment.Config.AdditionalArguments };

                if (!ProjectFileGenerator.bGenerateProjectFiles
                    && !WindowsPlatform.bCompileWithClang
                    && CompileAction.ProducedItems.Count > 0)
                {
                    FileItem TargetFile = CompileAction.ProducedItems[0];
                    string ResponseFileName = TargetFile.AbsolutePath + ".response";
                    ResponseFile.Create(new FileReference(ResponseFileName), UnrealCodeAnalyzerArguments.Concat(SharedArguments).Concat(FileArguments).Concat(AdditionalArguments).Select(x => ActionThread.ExpandEnvironmentVariables(x)));
                    CompileAction.CommandArguments = " @\"" + ResponseFileName + "\"";
                    CompileAction.PrerequisiteItems.Add(FileItem.GetExistingItemByPath(ResponseFileName));
                }
                else
                {
                    CompileAction.CommandArguments = String.Join(" ", UnrealCodeAnalyzerArguments.Concat(SharedArguments).Concat(FileArguments).Concat(AdditionalArguments));
                }


                if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
				{
					Log.TraceVerbose("Creating PCH: " + CompileEnvironment.Config.PrecompiledHeaderIncludeFilename);
					Log.TraceVerbose("     Command: " + CompileAction.CommandArguments);
				}
				else
				{
					Log.TraceVerbose("   Compiling: " + CompileAction.StatusDescription);
					Log.TraceVerbose("     Command: " + CompileAction.CommandArguments);
				}

				// VC++ always outputs the source file name being compiled, so we don't need to emit this ourselves
				CompileAction.bShouldOutputStatusDescription = false;

				// Don't farm out creation of precompiled headers as it is the critical path task.
				CompileAction.bCanExecuteRemotely =
					CompileEnvironment.Config.PrecompiledHeaderAction != PrecompiledHeaderAction.Create ||
					BuildConfiguration.bAllowRemotelyCompiledPCHs
					;

			}
			return Result;
		}

		public override CPPOutput CompileRCFiles(CPPEnvironment Environment, List<FileItem> RCFiles, ActionGraph ActionGraph) //(UEBuildTarget Target, CPPEnvironment Environment, List<FileItem> RCFiles)
        {
			var EnvVars = VCEnvironment.SetEnvironment(Environment.Config.Platform, false);

			CPPOutput Result = new CPPOutput();

			var BuildPlatform = UEBuildPlatform.GetBuildPlatformForCPPTargetPlatform(Environment.Config.Platform);

			foreach (FileItem RCFile in RCFiles)
			{
				Action CompileAction = ActionGraph.Add(ActionType.Compile);
				CompileAction.CommandDescription = "Resource";
				CompileAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory.FullName;
				CompileAction.CommandPath = EnvVars.ResourceCompilerPath.FullName;
				CompileAction.StatusDescription = Path.GetFileName(RCFile.AbsolutePath);

				// Suppress header spew
				CompileAction.CommandArguments += " /nologo";

				// If we're compiling for 64-bit Windows, also add the _WIN64 definition to the resource
				// compiler so that we can switch on that in the .rc file using #ifdef.
				if (Environment.Config.Platform == CPPTargetPlatform.UWP64)
				{
					CompileAction.CommandArguments += " /D_WIN64";
				}

				// Language
				CompileAction.CommandArguments += " /l 0x409";

				// Include paths.
				foreach (string IncludePath in Environment.Config.CPPIncludeInfo.IncludePaths)
				{
					CompileAction.CommandArguments += string.Format(" /i \"{0}\"", IncludePath);
				}

				// System include paths.
				foreach (var SystemIncludePath in Environment.Config.CPPIncludeInfo.SystemIncludePaths)
				{
					CompileAction.CommandArguments += string.Format(" /i \"{0}\"", SystemIncludePath);
				}

				// Preprocessor definitions.
				foreach (string Definition in Environment.Config.Definitions)
				{
					CompileAction.CommandArguments += string.Format(" /d \"{0}\"", Definition);
				}

				// Add the RES file to the produced item list.
				FileItem CompiledResourceFile = FileItem.GetItemByFileReference(
					FileReference.Combine(
						Environment.Config.OutputDirectory,
						Path.GetFileName(RCFile.AbsolutePath) + ".res"
						)
					);
				CompileAction.ProducedItems.Add(CompiledResourceFile);
				CompileAction.CommandArguments += string.Format(" /fo \"{0}\"", CompiledResourceFile.AbsolutePath);
				Result.ObjectFiles.Add(CompiledResourceFile);

				// Add the RC file as a prerequisite of the action.
				CompileAction.CommandArguments += string.Format(" \"{0}\"", RCFile.AbsolutePath);

				// Add the C++ source file and its included files to the prerequisite item list.
				AddPrerequisiteSourceFile(BuildPlatform, Environment, RCFile, CompileAction.PrerequisiteItems);
			}

			return Result;
		}

		public override FileItem LinkFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly, ActionGraph ActionGraph) //(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly)
        {
			var EnvVars = VCEnvironment.SetEnvironment(LinkEnvironment.Config.Platform, false);

			if (LinkEnvironment.Config.bIsBuildingDotNetAssembly)
			{
				return FileItem.GetItemByFileReference(LinkEnvironment.Config.OutputFilePath);
			}

			bool bIsBuildingLibrary = LinkEnvironment.Config.bIsBuildingLibrary || bBuildImportLibraryOnly;
			bool bIncludeDependentLibrariesInLibrary = bIsBuildingLibrary && LinkEnvironment.Config.bIncludeDependentLibrariesInLibrary;


            // Get link arguments.
            List<string> Arguments = new List<string>();
			if (bIsBuildingLibrary)
			{
				AppendLibArguments(LinkEnvironment, Arguments);
			}
			else
			{
				AppendLinkArguments(LinkEnvironment, Arguments);
			}



			// If we're only building an import library, add the '/DEF' option that tells the LIB utility
			// to simply create a .LIB file and .EXP file, and don't bother validating imports
			if (bBuildImportLibraryOnly)
			{
				Arguments.Add("/DEF");

				// Ensure that the import library references the correct filename for the linked binary.
				Arguments.AddFormat("/NAME:\"{0}\"", LinkEnvironment.Config.OutputFilePath.GetFileName());
			}


			// Add delay loaded DLLs.
			if (!bIsBuildingLibrary)
			{
				// Delay-load these DLLs.
				foreach (string DelayLoadDLL in LinkEnvironment.Config.DelayLoadDLLs)
				{
					Arguments.AddFormat("/DELAYLOAD:\"{0}\"", DelayLoadDLL);
				}
			}

			// @todo UE4 DLL: Why do I need LIBPATHs to build only export libraries with /DEF? (tbbmalloc.lib)
			if (!LinkEnvironment.Config.bIsBuildingLibrary || (LinkEnvironment.Config.bIsBuildingLibrary && bIncludeDependentLibrariesInLibrary))
			{
				// Add the library paths to the argument list.
				foreach (string LibraryPath in LinkEnvironment.Config.LibraryPaths)
				{
					Arguments.AddFormat("/LIBPATH:\"{0}\"", LibraryPath);
				}

				// Add the excluded default libraries to the argument list.
				foreach (string ExcludedLibrary in LinkEnvironment.Config.ExcludedLibraries)
				{
					Arguments.AddFormat("/NODEFAULTLIB:\"{0}\"", ExcludedLibrary);
				}
			}

			// For targets that are cross-referenced, we don't want to write a LIB file during the link step as that
			// file will clobber the import library we went out of our way to generate during an earlier step.  This
			// file is not needed for our builds, but there is no way to prevent MSVC from generating it when
			// linking targets that have exports.  We don't want this to clobber our LIB file and invalidate the
			// existing timstamp, so instead we simply emit it with a different name
			FileReference ImportLibraryFilePath = FileReference.Combine(LinkEnvironment.Config.IntermediateDirectory,
														 LinkEnvironment.Config.OutputFilePath.GetFileNameWithoutExtension() + ".lib");

			if (LinkEnvironment.Config.bIsCrossReferenced && !bBuildImportLibraryOnly)
			{
				ImportLibraryFilePath += ".suppressed";
			}

			FileItem OutputFile;
			if (bBuildImportLibraryOnly)
			{
				OutputFile = FileItem.GetItemByFileReference(ImportLibraryFilePath);
			}
			else
			{
				OutputFile = FileItem.GetItemByFileReference(LinkEnvironment.Config.OutputFilePath);
				OutputFile.bNeedsHotReloadNumbersDLLCleanUp = LinkEnvironment.Config.bIsBuildingDLL;
			}

			List<FileItem> ProducedItems = new List<FileItem>();
			ProducedItems.Add(OutputFile);

			List<FileItem> PrerequisiteItems = new List<FileItem>();

			// Add the input files to a response file, and pass the response file on the command-line.
			List<string> InputFileNames = new List<string>();
			foreach (FileItem InputFile in LinkEnvironment.InputFiles)
			{
				InputFileNames.Add(string.Format("\"{0}\"", InputFile.AbsolutePath));
				PrerequisiteItems.Add(InputFile);
			}

			if (!bBuildImportLibraryOnly)
			{
				// Add input libraries as prerequisites, too!
				foreach (FileItem InputLibrary in LinkEnvironment.InputLibraries)
				{
					InputFileNames.Add(string.Format("\"{0}\"", InputLibrary.AbsolutePath));
					PrerequisiteItems.Add(InputLibrary);
				}
			}

			if (!bIsBuildingLibrary || (LinkEnvironment.Config.bIsBuildingLibrary && bIncludeDependentLibrariesInLibrary))
			{
				foreach (string AdditionalLibrary in LinkEnvironment.Config.AdditionalLibraries)
				{
					InputFileNames.Add(string.Format("\"{0}\"", AdditionalLibrary));

					// If the library file name has a relative path attached (rather than relying on additional
					// lib directories), then we'll add it to our prerequisites list.  This will allow UBT to detect
					// when the binary needs to be relinked because a dependent external library has changed.
					//if( !String.IsNullOrEmpty( Path.GetDirectoryName( AdditionalLibrary ) ) )
					{
						PrerequisiteItems.Add(FileItem.GetItemByPath(AdditionalLibrary));
					}
				}
			}

            Arguments.AddRange(InputFileNames);

            // Add the output file to the command-line.
            Arguments.AddFormat("/OUT:\"{0}\"", OutputFile.AbsolutePath);

			if (bBuildImportLibraryOnly || (LinkEnvironment.Config.bHasExports && !bIsBuildingLibrary))
			{
				// An export file is written to the output directory implicitly; add it to the produced items list.
				FileReference ExportFilePath = ImportLibraryFilePath.ChangeExtension(".exp");
				FileItem ExportFile = FileItem.GetItemByFileReference(ExportFilePath);
				ProducedItems.Add(ExportFile);
			}

			if (!bIsBuildingLibrary)
			{
				// Xbox 360 LTCG does not seem to produce those.
				if (LinkEnvironment.Config.bHasExports &&
					(LinkEnvironment.Config.Configuration != CPPTargetConfiguration.Shipping))
				{
					// Write the import library to the output directory for nFringe support.
					FileItem ImportLibraryFile = FileItem.GetItemByFileReference(ImportLibraryFilePath);
					Arguments.AddFormat("/IMPLIB:\"{0}\"", ImportLibraryFilePath);
					ProducedItems.Add(ImportLibraryFile);
				}

				if (LinkEnvironment.Config.bCreateDebugInfo)
				{
					// Write the PDB file to the output directory.			
					FileReference PDBFilePath = FileReference.Combine(LinkEnvironment.Config.OutputDirectory, Path.GetFileNameWithoutExtension(OutputFile.AbsolutePath) + ".pdb");
					FileItem PDBFile = FileItem.GetItemByFileReference(PDBFilePath);
						Arguments.AddFormat("/PDB:\"{0}\"", PDBFilePath);
						ProducedItems.Add(PDBFile);

                    // Write the MAP file to the output directory.			
					if (BuildConfiguration.bCreateMapFile)
					{
                        FileReference MAPFilePath = FileReference.Combine(LinkEnvironment.Config.OutputDirectory, Path.GetFileNameWithoutExtension(OutputFile.AbsolutePath) + ".map");
                        FileItem MAPFile = FileItem.GetItemByFileReference(MAPFilePath);
                        Arguments.Add(String.Format("/MAP:\"{0}\"", MAPFilePath));
                        ProducedItems.Add(MAPFile);

                        // Export a list of object file paths, so we can locate the object files referenced by the map file
                        VCToolChain.ExportObjectFilePaths(LinkEnvironment, Path.ChangeExtension(MAPFilePath.FullName, ".objpaths"));
                    }
                }

                // Add the additional arguments specified by the environment.
                Arguments.Add(LinkEnvironment.Config.AdditionalArguments);
			}

            // Create a response file for the linker, unless we're generating IntelliSense data
            FileReference ResponseFileName = GetResponseFileName(LinkEnvironment, OutputFile);
            if (!ProjectFileGenerator.bGenerateProjectFiles)
            {
                ResponseFile.Create(ResponseFileName, Arguments);
            }

            // Create an action that invokes the linker.
            Action LinkAction = ActionGraph.Add(ActionType.Link);
			LinkAction.CommandDescription = "Link";
			LinkAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory.FullName;
			LinkAction.CommandPath = bIsBuildingLibrary ? EnvVars.LibraryManagerPath.FullName : EnvVars.LinkerPath.FullName;
            LinkAction.CommandArguments = String.Format("@\"{0}\"", ResponseFileName);
            LinkAction.ProducedItems.AddRange(ProducedItems);
			LinkAction.PrerequisiteItems.AddRange(PrerequisiteItems);
			LinkAction.StatusDescription = Path.GetFileName(OutputFile.AbsolutePath);

			// Tell the action that we're building an import library here and it should conditionally be
			// ignored as a prerequisite for other actions
			LinkAction.bProducesImportLibrary = bBuildImportLibraryOnly || LinkEnvironment.Config.bIsBuildingDLL;

			// Only execute linking on the local PC.
			LinkAction.bCanExecuteRemotely = false;

			Log.TraceVerbose("     Linking: " + LinkAction.StatusDescription);
			Log.TraceVerbose("     Command: " + LinkAction.CommandArguments);

			return OutputFile;
		}


		/// <summary>
		/// Gets the default include paths for the given platform.
		/// </summary>
		public static string GetVCIncludePaths(CPPTargetPlatform Platform)
		{
			Debug.Assert(Platform == CPPTargetPlatform.UWP64 || Platform == CPPTargetPlatform.UWP32);

			// Make sure we've got the environment variables set up for this target
			VCEnvironment.SetEnvironment(Platform, false);

			// Also add any include paths from the INCLUDE environment variable.  MSVC is not necessarily running with an environment that
			// matches what UBT extracted from the vcvars*.bat using SetEnvironmentVariablesFromBatchFile().  We'll use the variables we
			// extracted to populate the project file's list of include paths
			// @todo projectfiles: Should we only do this for VC++ platforms?
			var IncludePaths = Environment.GetEnvironmentVariable("INCLUDE");
			if (!String.IsNullOrEmpty(IncludePaths) && !IncludePaths.EndsWith(";"))
			{
				IncludePaths += ";";
			}

			return IncludePaths;
		}

		/** Formats compiler output from Clang so that it is clickable in Visual Studio */
		protected static void ClangCompilerOutputFormatter(object sender, DataReceivedEventArgs e)
		{
			var Output = e.Data;
			if (Output == null)
			{
				return;
			}

			// @todo clang: Convert relative includes to absolute files so they'll be clickable
			Log.TraceInformation(Output);
		}

	};
	// @ATG_CHANGE : END
}
