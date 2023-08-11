// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/MovieGraphDataTypes.h"
#include "Graph/MovieGraphDefaultRenderer.h"
#include "Graph/MovieGraphSequenceDataSource.h"
#include "Graph/MovieGraphPipeline.h"

FMovieGraphInitConfig::FMovieGraphInitConfig()
{
	RendererClass = UMovieGraphDefaultRenderer::StaticClass();
	DataSourceClass = UMovieGraphSequenceDataSource::StaticClass();
	bRenderViewport = false;
}

UMovieGraphPipeline* UMovieGraphTimeStepBase::GetOwningGraph() const
{ 
	return GetTypedOuter<UMovieGraphPipeline>();
}

UMovieGraphPipeline* UMovieGraphRendererBase::GetOwningGraph() const
{ 
	return GetTypedOuter<UMovieGraphPipeline>();
}

UMovieGraphPipeline* UMovieGraphDataSourceBase::GetOwningGraph() const
{
	return GetTypedOuter<UMovieGraphPipeline>();
}
