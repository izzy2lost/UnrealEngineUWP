// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class Facebook : ModuleRules
{
	public Facebook(ReadOnlyTargetRules Target) : base(Target)
    {
		Type = ModuleType.External;

		// Additional Frameworks and Libraries for Android found in OnlineSubsystemFacebook_UPL.xml
        if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			bEnableObjCAutomaticReferenceCounting = true;

			PrivateDependencyModuleNames.Add("Swift");

			PublicWeakFrameworks.Add("AuthenticationServices");
			PublicWeakFrameworks.Add("SafariServices");			
			PublicWeakFrameworks.Add("Accelerate");
			
			// Dependency from other Facebook kits
			PublicAdditionalFrameworks.Add(
				new Framework(
					"FBAEMKit",
					$"IOS/FBAEMKit.xcframework",
					Framework.FrameworkMode.LinkAndCopy
				)
			);

			PublicAdditionalFrameworks.Add(
				new Framework(
					"FBSDKCoreKit_Basics",
					$"IOS/FBSDKCoreKit_Basics.xcframework",
					Framework.FrameworkMode.LinkAndCopy
				)
			);

			// Access to Facebook core
			PublicAdditionalFrameworks.Add(
				new Framework(
					"FBSDKCoreKit",
					$"IOS/FBSDKCoreKit.xcframework",
					Framework.FrameworkMode.LinkAndCopy
				)
			);

			// Access to Facebook login
			PublicAdditionalFrameworks.Add(
				new Framework(
					"FBSDKLoginKit",
					$"IOS/FBSDKLoginKit.xcframework",
					Framework.FrameworkMode.LinkAndCopy
				)
			);

			// Access to Facebook sharing
			PublicAdditionalFrameworks.Add(
				new Framework(
					"FBSDKShareKit",
					$"IOS/FBSDKShareKit.xcframework",
					Framework.FrameworkMode.LinkAndCopy
				)
			);
		}
	}
}

