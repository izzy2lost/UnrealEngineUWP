// Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class UWPSDK : ModuleRules
{
	public UWPSDK(TargetInfo Target)
	{
		Type = ModuleType.External;

		PublicAdditionalLibraries.AddRange(
			new string[] {
				"dxgi.lib",
				"dxguid.lib",
				"d3dcompiler.lib",
				"xapobase.lib",
			}
			);

		////@todo.WinRT: Add a profiling configuration type?
		//// By default, use standard d3d11
		PublicAdditionalLibraries.Add("d3d11.lib");
		//// Include this (d3d11i) to enable PIX captures
		////PublicAdditionalLibraries.Add("d3d11i.lib");
	}
}

