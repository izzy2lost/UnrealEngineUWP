// Created by ushell to make blueprint projects work end to end.
// Placed in Source/ because UAT cleans Intermediate/Source/
using UnrealBuildTool;
public class AutoRTFMTestsClientTarget : TargetRules {
public AutoRTFMTestsClientTarget(TargetInfo Target) : base(Target) {
Type = TargetType.Client;
ExtraModuleNames.AddRange(new string[] {"UnrealGame"} ); }}
