// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ClientInfoDelegate.h"
#include "Styling/AppStyle.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class IConcertClient;
class SScrollBox;
class SWidgetSwitcher;

struct FConcertSessionClientInfo;
struct FGuid;

namespace UE::ConcertClientSharedSlate
{
	/** Aligns client widgets from left to right. If there is not enough space, a horizontal scroll bar cuts of the list. */
	class CONCERTSHAREDSLATE_API SHorizontalClientList : public SCompoundWidget
	{
	public:

		DECLARE_DELEGATE_RetVal_TwoParams(bool, FSortPredicate, const FConcertSessionClientInfo& Left, const FConcertSessionClientInfo& FConcertSessionClientInfo);
		static bool SortLocalClientFirstThenAlphabetical(const FConcertSessionClientInfo& Left, const FConcertSessionClientInfo& Right, ConcertSharedSlate::FIsLocalClient IsLocalClientDelegate);

		/** @return The display string a SHorizontalClientList would display with the given state. Returns unset optional if EmptyListSlot would be shown. */
		static TOptional<FString> GetDisplayString(
			const TConstArrayView<FGuid>& Clients,
			const ConcertSharedSlate::FGetOptionalClientInfo& GetClientInfoDelegate,
			const FSortPredicate& SortPredicate,
			const ConcertSharedSlate::FIsLocalClient& IsLocalClientDelegate = {}
			);
		
		SLATE_BEGIN_ARGS(SHorizontalClientList)
			: _Font(FAppStyle::Get().GetFontStyle("NormalFont"))
		{}
			/** Decides whether the given client should be displayed as if it was a local client. */
			SLATE_EVENT(ConcertSharedSlate::FIsLocalClient, IsLocalClient)
			/** Used to get client display info for remote clients. */
			SLATE_EVENT(ConcertSharedSlate::FGetOptionalClientInfo, GetClientInfo)
			
			/** Whether to show a square image in front of the name. */
			SLATE_ATTRIBUTE(bool, DisplayAvatarColor)
			
			/** Used for highlighting in the text */
			SLATE_ATTRIBUTE(FText, HighlightText)
			/** The font to use for the names */
			SLATE_ARGUMENT(FSlateFontInfo, Font)
			
			/** Defaults to placing the local client first (if contained) and sorting alphabetically otherwise. */
			SLATE_EVENT(FSortPredicate, SortPredicate)

			/** Tooltip text to display when the list is non-empty. */
			SLATE_ATTRIBUTE(FText, ListToolTipText)
			
			/** The widget to display when the list is empty */
			SLATE_NAMED_SLOT(FArguments, EmptyListSlot)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);

		/** Refreshes the list. */
		void RefreshList(const TConstArrayView<FGuid>& Clients);

	private:
		
		/** Decides whether the given client should be displayed as if it was a local client. */
		ConcertSharedSlate::FIsLocalClient IsLocalClientDelegate;
		/** Used to get client display info for remote clients. */
		ConcertSharedSlate::FGetOptionalClientInfo GetClientInfoDelegate;
		/** Sorts the client list */
		FSortPredicate SortPredicateDelegate;

		/** Whether the square in front of the client name should be displayed. */
		TAttribute<bool> DisplayAvatarColorAttribute;
		/** Used for highlighting in the text */
		TAttribute<FText> HighlightTextAttribute;
		
		/** The font to use for the names */
		FSlateFontInfo NameFont;

		/** Displays the ScrollBox when there are clients and the EmptyListSlot otherwise. */
		TSharedPtr<SWidgetSwitcher> WidgetSwitcher;
		/** Contains the children. */
		TSharedPtr<SScrollBox> ScrollBox;
	};
}