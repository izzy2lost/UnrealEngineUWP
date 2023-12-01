// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "UObject/WeakObjectPtr.h"

enum class ECheckBoxState : uint8;
struct EVisibility;
class FReply;
class IPropertyUtilities;
class UDMXControlConsoleEditorModel;


namespace UE::DMX::Private
{
	/** Details Customization for DMX DMX Control Console */
	class FDMXControlConsoleFaderGroupDetails
		: public IDetailCustomization
	{
	public:
		/** Constructor */
		FDMXControlConsoleFaderGroupDetails(const TWeakObjectPtr<UDMXControlConsoleEditorModel> InWeakEditorModel);

		/** Makes an instance of this Details Customization */
		static TSharedRef<IDetailCustomization> MakeInstance(const TWeakObjectPtr<UDMXControlConsoleEditorModel> InWeakEditorModel);

		//~ Begin of IDetailCustomization interface
		virtual void CustomizeDetails(IDetailLayoutBuilder& InDetailLayout) override;
		//~ End of IDetailCustomization interface

	private:
		/** True if at least one selected Fader Group have any Fixture Patch bound */
		bool DoSelectedFaderGroupsHaveAnyFixturePatches() const;

		/** Called when Clear button is clicked */
		FReply OnClearButtonClicked();

		/** Called to toggle the lock state of selected Fader Groups */
		void OnLockToggleChanged(ECheckBoxState CheckState);

		/** Gets the current lock state of selected Fader Groups */
		ECheckBoxState IsLockChecked() const;

		/** Gets current selected FaderGroup Fixture Patch name */
		FText GetFixturePatchText() const;

		/** Gets visibility attribute of the Editor Color Property */
		EVisibility GetEditorColorVisibility() const;

		/** Gets visibility attribute of the Clear button */
		EVisibility GetClearButtonVisibility() const;

		/** Weak reference to the Control Console editor model */
		TWeakObjectPtr<UDMXControlConsoleEditorModel> WeakEditorModel;
	};
}
