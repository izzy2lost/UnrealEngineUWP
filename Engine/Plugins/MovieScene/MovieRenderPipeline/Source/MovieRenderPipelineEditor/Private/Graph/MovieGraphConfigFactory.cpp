// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/MovieGraphConfigFactory.h"

#include "Graph/MovieGraphConfig.h"
#include "HAL/IConsoleManager.h"
#include "MovieRenderPipelineCoreModule.h"
#include "MovieRenderPipelineSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieGraphConfigFactory)

UMovieGraphConfigFactory::UMovieGraphConfigFactory()
{
	bCreateNew = true;
	bEditAfterNew = false;
	SupportedClass = UMovieGraphConfig::StaticClass();
}

UObject* UMovieGraphConfigFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	const UMovieRenderPipelineProjectSettings* ProjectSettings = GetDefault<UMovieRenderPipelineProjectSettings>();
	const TSoftObjectPtr<UMovieGraphConfig> ProjectDefaultGraph = ProjectSettings->DefaultGraph;

	// Duplicate the default graph asset provided in Project Settings
	if (const UMovieGraphConfig* DefaultGraph = ProjectDefaultGraph.LoadSynchronous())
	{
		return DuplicateObject<UMovieGraphConfig>(DefaultGraph, InParent, Name);
	}

	// If the default couldn't be loaded, try loading the default supplied by MRQ. Note that this could be the same as
	// the default above if it wasn't changed by the user; failure to load this indicates a larger issue.
	const FSoftObjectPath DefaultGraphPath(UMovieRenderPipelineProjectSettings::GetDefaultGraphPath());
	if (const UMovieGraphConfig* DefaultGraph = Cast<UMovieGraphConfig>(DefaultGraphPath.TryLoad()))
	{
		UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Could not load the default graph [%s] specified in Project Settings. Falling back to MRQ-supplied default."), *DefaultGraphPath.GetAssetPathString());
		
		return DuplicateObject<UMovieGraphConfig>(DefaultGraph, InParent, Name);
	}

	// This should never happen, but create an empty graph as a last resort (which is most likely just an Input node and an Output node).
	UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Could not load the default graph [%s] supplied by MRQ. Falling back to an empty graph."), *DefaultGraphPath.GetAssetPathString());
	
	return NewObject<UMovieGraphConfig>(InParent, Class, Name, Flags);
}

bool UMovieGraphConfigFactory::ShouldShowInNewMenu() const
{
	IConsoleVariable* RenderGraphCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("MoviePipeline.EnableRenderGraph"));
	return RenderGraphCVar && RenderGraphCVar->GetBool();
}
