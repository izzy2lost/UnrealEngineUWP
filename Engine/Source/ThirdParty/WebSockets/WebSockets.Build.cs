// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class WebSockets : ModuleRules
{

    public WebSockets(TargetInfo Target)
	{
		Type = ModuleType.External;
            string WebsocketPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "WebSockets/libwebsockets/";
// @ATG_CHANGE : BEGIN UWP support
		    if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.UWP64)
// @ATG_CHANGE : END
            {
                PublicIncludePaths.Add(WebsocketPath + "include/");
// @ATG_CHANGE : BEGIN UWP support
				if (WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2015)
				{
					PublicLibraryPaths.Add(WebsocketPath + "lib/x64/2015/");
				}
				else
				{
					PublicLibraryPaths.Add(WebsocketPath + "lib/x64/2013/");
				}
// @ATG_CHANGE : END
			    PublicAdditionalLibraries.Add("websockets_static.lib");
			    PublicAdditionalLibraries.Add("ZLIB.lib");
		    }
            else if ( Target.Platform == UnrealTargetPlatform.Mac)
            {
                  PublicIncludePaths.Add(WebsocketPath + "include/");
		          PublicAdditionalLibraries.Add(WebsocketPath + "lib/Mac/libwebsockets.a");
            }
     }
}


