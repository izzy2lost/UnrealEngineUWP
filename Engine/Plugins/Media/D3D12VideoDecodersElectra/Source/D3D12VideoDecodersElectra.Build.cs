// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class D3D12VideoDecodersElectra : ModuleRules
	{
		public D3D12VideoDecodersElectra(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
                    "RHI",
					"ElectraCodecFactory",
					"ElectraDecoders"
                });
		}
	}
}
