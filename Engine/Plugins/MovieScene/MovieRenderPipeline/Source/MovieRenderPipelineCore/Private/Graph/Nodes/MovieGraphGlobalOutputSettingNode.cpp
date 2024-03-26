// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Nodes/MovieGraphGlobalOutputSettingNode.h"
#include "Graph/MovieGraphProjectSettings.h"
#include "Graph/MovieGraphBlueprintLibrary.h"
#include "Styling/AppStyle.h"
#include "Algo/Find.h"

UMovieGraphGlobalOutputSettingNode::UMovieGraphGlobalOutputSettingNode()
	: OutputFrameRate(FFrameRate(24, 1))
	, bOverwriteExistingOutput(true)
	, ZeroPadFrameNumbers(4)
	, FrameNumberOffset(0)
	, HandleFrameCount(0)
	, CustomPlaybackRangeStartFrame(0)
	, CustomPlaybackRangeEndFrame(0)
	, bFlushDiskWritesPerShot(false)
{
	// We prefer a 1080p resolution by default, but users may not have a preset that matches that. So we'll 
	// look for a matching resolution if we can find one, otherwise we go to Custom set to 1920x1080.
	const FMovieGraphNamedResolution* FoundResolution = nullptr;
	if (const UMovieGraphProjectSettings* MovieGraphProjectSettings =
		GetDefault<UMovieGraphProjectSettings>())
	{
		FoundResolution = Algo::FindByPredicate(
			MovieGraphProjectSettings->DefaultNamedResolutions,
			[](const FMovieGraphNamedResolution& Other)
			{
				return Other.Resolution == FIntPoint(1920, 1080);
			});
	}

	// If we found one that was 1080p, regardless of name, use it.
	if (FoundResolution)
	{
		OutputResolution = *FoundResolution;
	}
	else
	{
		// We didn't find one that was 1080p, just force a custom resolution.
		OutputResolution = UMovieGraphBlueprintLibrary::NamedResolutionFromSize(1920, 1080);
	}
	
	OutputDirectory.Path = TEXT("{project_dir}/Saved/MovieRenders/");
}

void UMovieGraphGlobalOutputSettingNode::GetFormatResolveArgs(FMovieGraphResolveArgs& OutMergedFormatArgs, const FMovieGraphRenderDataIdentifier& InRenderDataIdentifier) const
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
FText UMovieGraphGlobalOutputSettingNode::GetNodeTitle(const bool bGetDescriptive) const
{
	static const FText OutputSettingsNodeName = NSLOCTEXT("MoviePipelineGraph", "NodeName_GlobalOutputSettings", "Global Output Settings");
	return OutputSettingsNodeName;
}

FText UMovieGraphGlobalOutputSettingNode::GetMenuCategory() const 
{
	return NSLOCTEXT("MoviePipelineGraph", "Settings_Category", "Settings");
}

FLinearColor UMovieGraphGlobalOutputSettingNode::GetNodeTitleColor() const 
{
	static const FLinearColor OutputSettingsColor = FLinearColor(0.854f, 0.509f, 0.039f);
	return OutputSettingsColor;
}

FSlateIcon UMovieGraphGlobalOutputSettingNode::GetIconAndTint(FLinearColor& OutColor) const 
{
	static const FSlateIcon SettingsIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Settings");

	OutColor = FLinearColor::White;
	return SettingsIcon;
}

EMovieGraphBranchRestriction UMovieGraphGlobalOutputSettingNode::GetBranchRestriction() const
{
	return EMovieGraphBranchRestriction::Globals;
}
#endif // WITH_EDITOR

FIntPoint UMovieGraphGlobalOutputSettingNode::GetSyncedOutputResolution() const
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
