// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

struct FDMXEntityFixturePatchRef;
struct FSlateColor;
template <typename OptionType> class SComboBox;
class UDMXControlConsoleEditorModel;
class UDMXControlConsoleFaderGroup;
class UDMXEntityFixturePatch;
class UDMXLibrary;


namespace UE::DMX::Private
{
	class SDMXControlConsoleEditorFaderGroupView;

	/** Combo box widget for selecting fixture patches in the Fader Group toolbar */
	class SDMXControlConsoleEditorFaderGroupComboBox
		: public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDMXControlConsoleEditorFaderGroupComboBox)
			{}

		SLATE_END_ARGS()

		/** Constructs the widget */
		void Construct(const FArguments& InArgs, const TWeakPtr<SDMXControlConsoleEditorFaderGroupView>& InFaderGroupView, UDMXControlConsoleEditorModel* InEditorModel);

	private:
		/** Gets reference to the Fader Group */
		UDMXControlConsoleFaderGroup* GetFaderGroup() const;

		/** Generates a widget for each element in the Fixture Patches Combo Box */
		TSharedRef<SWidget> GenerateFixturePatchesComboBoxWidget(const TSharedPtr<FDMXEntityFixturePatchRef> FixturePatchRef);

		/** True if the given Fixture Patch is not used by any other Fader Group */
		bool IsFixturePatchStillAvailable(const UDMXEntityFixturePatch* InFixturePatch) const;

		/** Updates the ComboBoxSource array according to the current DMX Library */
		void UpdateComboBoxSource();

		/** Called when a FixturePatchesComboBox element is selected */
		void OnComboBoxSelectionChanged(const TSharedPtr<FDMXEntityFixturePatchRef> FixturePatchRef, ESelectInfo::Type SelectInfo);

		/** Gets the fader group's editor color */
		FSlateColor GetFaderGroupEditorColor() const;

		/** Gets the fader group's fixture patch name, if valid */
		FText GetFaderGroupFixturePatchNameText() const;

		/** Reference to the current DMX Library */
		TWeakObjectPtr<UDMXLibrary> DMXLibrary;

		/** Source items for the FixturePatchesComboBox */
		TArray<TSharedPtr<FDMXEntityFixturePatchRef>> ComboBoxSource;

		/** A ComboBox for showing all active Fixture Patches in the current DMX Library */
		TSharedPtr<SComboBox<TSharedPtr<FDMXEntityFixturePatchRef>>> FixturePatchesComboBox;

		/** Weak Reference to the Fader Group view */
		TWeakPtr<SDMXControlConsoleEditorFaderGroupView> FaderGroupView;

		/** Weak reference to the Control Console editor model */
		TWeakObjectPtr<UDMXControlConsoleEditorModel> EditorModel;
	};
}
