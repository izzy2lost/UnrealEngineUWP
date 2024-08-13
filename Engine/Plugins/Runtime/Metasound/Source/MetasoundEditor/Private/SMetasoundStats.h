// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Templates/SharedPointer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"


// Forward Declarations
class IMetaSoundDocumentInterface;
class STextBlock;
class UMetaSoundBuilderBase;
class UMetaSoundSource;

struct FMetaSoundPageSettings;


namespace Metasound::Editor
{
	/** Widget for displaying page stats of a previewing MetaSound. */
	class SPageStats : public SVerticalBox
	{
	public:
		SLATE_BEGIN_ARGS(SPageStats) { };
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);
		void Update(const FMetaSoundPageSettings* PageSettings);

	private:
		TSharedPtr<STextBlock> PageTextWidget;
	};


	/** Widget for displaying render stats of a previewing MetaSound. */
	class SRenderStats : public SVerticalBox
	{
	public:
		SLATE_BEGIN_ARGS( SRenderStats) { };
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);
		void Update(bool bIsPlaying, const UMetaSoundSource* InSource);

	private:
		TSharedPtr<STextBlock> RenderStatsCostWidget;
		TSharedPtr<STextBlock> RenderStatsCPUWidget;

		bool bPreviousIsPlaying = false;
		double MaxCPUCoreUtilization = 0;
		float MaxRelativeRenderCost = 0.f;
	};
} // namespace Metasound::Editor
