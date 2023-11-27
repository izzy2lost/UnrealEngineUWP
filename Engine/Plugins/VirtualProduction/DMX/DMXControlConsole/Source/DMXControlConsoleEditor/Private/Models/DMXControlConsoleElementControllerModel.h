// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Controllers/DMXControlConsoleElementController.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"

class UDMXControlConsoleElementController;
class UDMXControlConsoleFaderBase;
class UDMXControlConsoleFixturePatchMatrixCell;


namespace UE::DMX::Private
{
	/** Model for an element controller in a fader group */
	class FDMXControlConsoleElementControllerModel
		: public TSharedFromThis<FDMXControlConsoleElementControllerModel>
	{
	public:
		/** Constructor */
		FDMXControlConsoleElementControllerModel(const TWeakObjectPtr<UDMXControlConsoleElementController> InWeakElementController);

		/** Gets the Element Controller this model is based on */
		UDMXControlConsoleElementController* GetElementController() const;

		/** Gets the first available Fader in the Element Controller, if valid */
		UDMXControlConsoleFaderBase* GetFirstAvailableFader() const;

		/** Gets the Matrix Cell Element in the Controller, if valid */
		UDMXControlConsoleFixturePatchMatrixCell* GetMatrixCellElement() const;

		/** Gets the name of the Element Controller, relative to the contained Elements */
		FString GetRelativeControllerName() const;

		/** Gets the value of the Element Controller, relative to the contained Elements */
		float GetRelativeValue() const;

		/** Gets the min value of the Element Controller, relative to the contained Elements */
		float GetRelativeMinValue() const;

		/** Gets the max value of the Element Controller, relative to the contained Elements */
		float GetRelativeMaxValue() const;

		/** True if the Controller has just one Element */
		bool HasSingleElement() const;

		/** True if the Controller has Elements with the same data type */
		bool HasUniformDataType() const;

		/** True if all Elements in the controller are raw faders */
		bool HasOnlyRawFaders() const;

		/** True if all the Elements in the Controller are muted */
		bool IsMuted() const;

		/** True if all the Elements in the Controller are locked */
		bool IsLocked() const;

	private:
		/** Weak reference to the Element Controller this model is based on */
		TWeakObjectPtr<UDMXControlConsoleElementController> WeakElementController;
	};
}
