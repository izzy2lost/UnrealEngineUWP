// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Nodes/MovieGraphOutputSettingNode.h"

#include "Graph/MovieGraphProjectSettings.h"
#include "Styling/AppStyle.h"

UMovieGraphOutputSettingNode::UMovieGraphOutputSettingNode()
	: OutputResolution(FMovieGraphNamedResolution(FMovieGraphNamedResolution::DefaultResolutionName))
	, OutputFrameRate(FFrameRate(24, 1))
	, bOverwriteExistingOutput(true)
	, ZeroPadFrameNumbers(4)
	, FrameNumberOffset(0)
	, bAutoVersion(true)
	, VersionNumber(1)
{
	FileNameFormat = TEXT("{sequence_name}.{frame_number}");
	OutputDirectory.Path = TEXT("{project_dir}/Saved/MovieRenders/");
}

void UMovieGraphOutputSettingNode::GetFormatResolveArgs(FMovieGraphResolveArgs& OutMergedFormatArgs, const FMovieGraphRenderDataIdentifier& InRenderDataIdentifier) const
{
	const FString ResolvedProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	OutMergedFormatArgs.FilenameArguments.Add(TEXT("project_dir"), ResolvedProjectDir);
	OutMergedFormatArgs.FileMetadata.Add(TEXT("unreal/project_dir"), ResolvedProjectDir);

	FIntPoint OutputResolutionAsIntPoint = GetSyncedOutputResolution();
	
	// Resolution Arguments
	{
		FString Resolution = FString::Printf(TEXT("%d_%d"), OutputResolutionAsIntPoint.X, OutputResolutionAsIntPoint.Y);
		OutMergedFormatArgs.FilenameArguments.Add(TEXT("output_resolution"), Resolution);
		OutMergedFormatArgs.FilenameArguments.Add(TEXT("output_width"), FString::FromInt(OutputResolutionAsIntPoint.X));
		OutMergedFormatArgs.FilenameArguments.Add(TEXT("output_height"), FString::FromInt(OutputResolutionAsIntPoint.Y));
	}

	// We don't resolve the version here because that's handled on a per-file/shot basis
}

#if WITH_EDITOR
FText UMovieGraphOutputSettingNode::GetNodeTitle(const bool bGetDescriptive) const
{
	static const FText OutputSettingsNodeName = NSLOCTEXT("MoviePipelineGraph", "NodeName_OutputSettings", "Output Settings");
	return OutputSettingsNodeName;
}

FText UMovieGraphOutputSettingNode::GetMenuCategory() const 
{
	return NSLOCTEXT("MoviePipelineGraph", "Settings_Category", "Settings");
}

FLinearColor UMovieGraphOutputSettingNode::GetNodeTitleColor() const 
{
	static const FLinearColor OutputSettingsColor = FLinearColor(0.854f, 0.509f, 0.039f);
	return OutputSettingsColor;
}

FSlateIcon UMovieGraphOutputSettingNode::GetIconAndTint(FLinearColor& OutColor) const 
{
	static const FSlateIcon SettingsIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Settings");

	OutColor = FLinearColor::White;
	return SettingsIcon;
}
#endif // WITH_EDITOR

FIntPoint UMovieGraphOutputSettingNode::GetSyncedOutputResolution() const
{
	// Try to find a matching entry from Project Settings to stay in sync
	const UMovieGraphProjectSettings* MovieGraphProjectSettings = GetDefault<UMovieGraphProjectSettings>();
	if (ensureAlwaysMsgf(MovieGraphProjectSettings, TEXT("%hs: Failed to find UMovieGraphProjectSettings!"), __FUNCTION__))
	{
		const FMovieGraphNamedResolution* FoundResolution = MovieGraphProjectSettings->FindNamedResolutionForOption(OutputResolution.ProfileName);
		if (FoundResolution)
		{
			return FoundResolution->Resolution;
		}
	}

	// Otherwise return what we have saved locally
	return OutputResolution.Resolution;
}
