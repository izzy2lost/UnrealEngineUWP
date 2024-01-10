// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieGraphFormatTokenCustomization.h"

#include "Graph/Nodes/MovieGraphFileOutputNode.h"
#include "MoviePipelineUtils.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Layout/WidgetPath.h"
#include "Widgets/SMoviePipelineFormatTokenAutoCompleteBox.h"

void FMovieGraphFormatTokenCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	CustomizedObject = DetailBuilder.GetSelectedObjects()[0];
	check(CustomizedObject.IsValid() && !CustomizedObject.IsStale());

	// This should work just fine for UMovieGraphFileOutputNode and UMovieGraphCommandLineEncoderNode as long as the property names stay in sync
	OutputFormatPropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMovieGraphFileOutputNode, FileNameFormat));
	const bool bShowChildren = false;
	FText InitialText;
	OutputFormatPropertyHandle->GetValueAsDisplayText(InitialText);

	DetailBuilder.EditDefaultProperty(OutputFormatPropertyHandle)->CustomWidget(bShowChildren)
		.NameContent()
		[
			OutputFormatPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(200.0f)
		[
			SNew(SMoviePipelineFormatTokenAutoCompleteBox)
			.Text(this, &FMovieGraphFormatTokenCustomization::GetText)
			.Suggestions(this, &FMovieGraphFormatTokenCustomization::GetSuggestions)
			.OnTextChanged(this, &FMovieGraphFormatTokenCustomization::OnTextChanged)
		];
}

TArray<FString> FMovieGraphFormatTokenCustomization::GetSuggestions() const
{
	TArray<FString> Suggestions;
	FMoviePipelineFormatArgs FormatArgs;
	GetFormatArguments(FormatArgs);

	for (const TPair<FString, FString>& KVP : FormatArgs.FilenameArguments)
	{
		Suggestions.Add(KVP.Key);
	}

	return Suggestions;
}

void FMovieGraphFormatTokenCustomization::OnTextChanged(const FText& InValue) const
{
	OutputFormatPropertyHandle->SetValue(InValue.ToString());
}

FText FMovieGraphFormatTokenCustomization::GetText() const
{
	FText DisplayText;
	OutputFormatPropertyHandle->GetValueAsDisplayText(DisplayText);
	return DisplayText;
}

void FMovieGraphFormatTokenCustomization::GetFormatArguments(FMoviePipelineFormatArgs& InOutFormatArgs)
{
	static const FString LevelName = TEXT("Level Name");
	static const FString SequenceName = TEXT("Sequence Name");
	static const FString JobAuthor = TEXT("Job Author");
	static const FString JobName = TEXT("Job Name");
	static const FString JobComment = TEXT("Job Comment");
	static constexpr double FrameRate = 0.0;
	static const FString FramePlaceholderNumber = TEXT("0");

	MoviePipeline::GetOutputStateFormatArgs(
		InOutFormatArgs.FilenameArguments, InOutFormatArgs.FileMetadata,
		FramePlaceholderNumber, FramePlaceholderNumber,
		FramePlaceholderNumber, FramePlaceholderNumber,
		TEXT("CameraName"), TEXT("ShotName"));

	InOutFormatArgs.FilenameArguments.Add(TEXT("level_name"), LevelName);
	InOutFormatArgs.FilenameArguments.Add(TEXT("sequence_name"), SequenceName);
	InOutFormatArgs.FilenameArguments.Add(TEXT("job_author"), JobAuthor);
	InOutFormatArgs.FilenameArguments.Add(TEXT("job_name"), JobName);
	InOutFormatArgs.FilenameArguments.Add(TEXT("frame_rate"), FString::SanitizeFloat(FrameRate));

	static const FDateTime CurrentTime = FDateTime::Now();
	static constexpr int32 DummyVersionNumber = 1;
	UE::MoviePipeline::GetSharedFormatArguments(
		InOutFormatArgs.FilenameArguments, InOutFormatArgs.FileMetadata, CurrentTime, DummyVersionNumber, InOutFormatArgs.InJob);
}
