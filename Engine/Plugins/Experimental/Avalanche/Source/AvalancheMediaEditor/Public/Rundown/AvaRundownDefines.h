// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Internationalization/Text.h"
#include "Templates/SharedPointer.h"

class IAvaRundownInstancedPageView;
class IAvaRundownPageView;
class IAvaRundownTemplatePageView;
struct FAvaRundownPage;

typedef TWeakPtr<IAvaRundownPageView> FAvaRundownPageViewWeak;
typedef TSharedPtr<IAvaRundownPageView> FAvaRundownPageViewPtr;
typedef TSharedRef<IAvaRundownPageView> FAvaRundownPageViewRef;
typedef TWeakPtr<IAvaRundownTemplatePageView> FAvaRundownTemplatePageViewWeak;
typedef TSharedPtr<IAvaRundownTemplatePageView> FAvaRundownTemplatePageViewPtr;
typedef TSharedRef<IAvaRundownTemplatePageView> FAvaRundownTemplatePageViewRef;
typedef TWeakPtr<IAvaRundownInstancedPageView> FAvaRundownInstancedPageViewWeak;
typedef TSharedPtr<IAvaRundownInstancedPageView> FAvaRundownInstancedPageViewPtr;
typedef TSharedRef<IAvaRundownInstancedPageView> FAvaRundownInstancedPageViewRef;

namespace UE::AvaRundown
{
	enum {InvalidPageId	= -1};
	
	struct AVALANCHEMEDIAEDITOR_API FEditorMetrics
	{
		static constexpr float ColumnLeftOffset = 5.f;
		static const FNumberFormattingOptions PageIdFormattingOptions;
	};
	
	enum class EPageEvent : uint8
	{
		/** Called after the Selection has Changed */ 
		SelectionChanged,

		/** Called to sync the Selection of current Selected Items in the Caller */
		SelectionRequest,

		/** Called to Request a Rename of the Selected Items */
		RenameRequest,

		/** Called to Request Renumbering Page Id of Selected Items */
		RenumberRequest,

		/** Called to Request Reimporting Assets of Selected Items */
		ReimportRequest,
	};
}
