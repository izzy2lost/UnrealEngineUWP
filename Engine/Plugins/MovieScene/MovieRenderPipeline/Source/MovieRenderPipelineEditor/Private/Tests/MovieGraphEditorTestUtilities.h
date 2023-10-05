// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UMovieGraphConfig;

namespace UE::MovieGraph::Private::Tests
{
	UMovieGraphConfig* CreateNewMovieGraphConfig(const FName InName = "GraphTestConfig");
	UMovieGraphConfig* CreateDefaultMovieGraphConfig();
	void OpenGraphConfigInEditor(UMovieGraphConfig* InGraphConfig);
	TArray<UClass*> GetAllDerivedClasses(UClass* BaseClass, bool bRecursive);
	TArray<UClass*> GetNativeClasses(UClass* BaseClass, bool bRecursive);
	TArray<UClass*> GetBlueprintClasses(UClass* BaseClass, bool bRecursive);
}
