// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

namespace UE::MultiUserClient
{
	enum class EApplyPresetFlags : uint8;
}

class FMenuBuilder;

namespace UE::MultiUserClient
{
	class FPresetManager;

	/**
	 * A combo button displayed to the right of the object search bar.
	 * Its menu allows the user to save and load replication presets.
	 */
	class SPresetComboButton : public SCompoundWidget
	{
	public:

		SLATE_BEGIN_ARGS(SPresetComboButton) {}
			
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, FPresetManager& InPresetManager UE_LIFETIMEBOUND);

	private:

		/** Used to save & load preset. */
		FPresetManager* PresetManager = nullptr;

		struct FPresetOptions
		{
			/** Clients not mentioned by the preset will get their content wiped. */
			bool bResetAllOtherClients = true;
		} Options;

		/** Creates the Save & Load options for the menu. */
		TSharedRef<SWidget> CreateMenuContent();
		void CreateSaveMenuContent(FMenuBuilder& MenuBuilder);
		void CreateLoadMenuContent(FMenuBuilder& MenuBuilder);

		/** Saves the current session content. */
		void SavePresetAs();
		/** Handles loading the preset. */
		void LoadPreset(const FAssetData& AssetData);

		EApplyPresetFlags BuildFlags() const;
	};
}

