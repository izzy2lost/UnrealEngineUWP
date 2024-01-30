// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Internationalization/Text.h"
#include "Templates/SharedPointer.h"

class IAvaInstancedPageView;
class IAvaTemplatePageView;
class IAvaPageView;
struct FAvalanchePage;

typedef TWeakPtr<IAvaPageView> FAvaPageViewWeak;
typedef TSharedPtr<IAvaPageView> FAvaPageViewPtr;
typedef TSharedRef<IAvaPageView> FAvaPageViewRef;
typedef TWeakPtr<IAvaTemplatePageView> FAvaTemplatePageViewWeak;
typedef TSharedPtr<IAvaTemplatePageView> FAvaTemplatePageViewPtr;
typedef TSharedRef<IAvaTemplatePageView> FAvaTemplatePageViewRef;
typedef TWeakPtr<IAvaInstancedPageView> FAvaInstancedPageViewWeak;
typedef TSharedPtr<IAvaInstancedPageView> FAvaInstancedPageViewPtr;
typedef TSharedRef<IAvaInstancedPageView> FAvaInstancedPageViewRef;

namespace UE::AvalanchePlaylist
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
