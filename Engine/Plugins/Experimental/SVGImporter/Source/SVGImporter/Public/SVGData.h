// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SVGGraphicalElements.h"
#include "UObject/Object.h"
#include "SVGData.generated.h"

class UTexture2D;

/**
 * Helper struct to provide SVGData objects with the necessary information for initialization.
 */
struct FSVGDataInitializer
{
	FSVGDataInitializer()
	{
	}
	
	FSVGDataInitializer(const FString& InSVGTextBuffer, const FString& InSourceFilename = TEXT(""))
		: SVGTextBuffer(InSVGTextBuffer)
		, SourceFilename(InSourceFilename)
	{
	}

	/** Is this element initialized? */
	bool IsInitialized() const
	{
		return !SVGTextBuffer.IsEmpty();
	}

	/** The SVG Text Buffer */
	FString SVGTextBuffer = TEXT("");

	/** The SVG Filename. Can be empty if Text Buffer is not coming from a file */
	FString SourceFilename = TEXT("");

	/** The SVG Elements already parsed from the SVG Text Buffer */
	TArray<TSharedRef<FSVGBaseElement>> Elements;
};

UCLASS(Blueprintable)
class SVGIMPORTER_API USVGData : public UObject
{
	GENERATED_BODY()

public:
	void Reset()
	{
		SVGFileContent = TEXT("");
		Shapes.Empty();
	}

	/** Initialize this SVGData with the information provided by the initializer */
	void Initialize(const FSVGDataInitializer& InInitializer);

#if WITH_EDITOR
	/** Generate a texture from the SVG information */
	void GenerateSVGTexture();

	/** Get the SVG source file path, if available */
	const FString& GetSourceFilePath() const { return SourceFilePath; }
#endif

	/** Create the shapes composing this SVG Data, starting from the information provided by SVG parsing */
	void CreateShapes(const TArray<TSharedRef<FSVGBaseElement>>& InSVGElements);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SVG Data")
	TObjectPtr<UTexture2D> SVGTexture;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SVG Data")
	FString SVGFileContent;

	UPROPERTY(BlueprintReadOnly, Category = "SVG Data")
	TArray<FSVGShape> Shapes;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category = "Source Asset")
	FString SourceFilePath = TEXT("");
#endif
};
