// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConcertMessageData.h"
#include "Styling/AppStyle.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class IConcertClient;

namespace UE::ConcertClientSharedSlate
{
	/** Knows how to display FConcertClientInfo. */
	class CONCERTSHAREDSLATE_API SClientName : public SCompoundWidget
	{
	public:

		SLATE_BEGIN_ARGS(SClientName)
			: _DisplayAsLocalClient(false)
			, _DisplayAvatarColor(true)
			, _Font(FAppStyle::Get().GetFontStyle("BoldFont"))
		{}
			/** The client info to display. */
			SLATE_ATTRIBUTE(TOptional<FConcertClientInfo>, ClientInfo)
			
			/** Whether visually indicate that this is a local client (appends "(me)" if true). */
			SLATE_ATTRIBUTE(bool, DisplayAsLocalClient)
			/** Whether to show a square image in front of the name. */
			SLATE_ATTRIBUTE(bool, DisplayAvatarColor)
			
			/** Used for highlighting in the text */
			SLATE_ATTRIBUTE(FText, HighlightText)
			/** The font to use for the name */
			SLATE_ARGUMENT(FSlateFontInfo, Font)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);

		/** @return The display that would be used given the settings. */
		static FText GetDisplayText(const FConcertClientInfo& Info, bool bDisplayAsLocalClient);

	private:

		/** The client info to display. */
		TAttribute<TOptional<FConcertClientInfo>> ClientInfoAttribute;
		/** Whether visually indicate that this is a local client (appends "(me)" if true). */
		TAttribute<bool> DisplayAsLocalClientAttribute;

		/** Gets the display name. */
		FText GetClientDisplayName() const;
		/** Gets the avatar color */
		FSlateColor GetAvatarColor() const;
	};
}
