// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleElementControllerModel.h"

#include "Algo/AllOf.h"
#include "Algo/Find.h"
#include "DMXControlConsoleFixturePatchMatrixCell.h"
#include "DMXControlConsoleRawFader.h"
#include "IDMXControlConsoleFaderGroupElement.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleElementControllerModel"

namespace UE::DMX::Private
{
	FDMXControlConsoleElementControllerModel::FDMXControlConsoleElementControllerModel(const TWeakObjectPtr<UDMXControlConsoleElementController> InWeakElementController)
		: WeakElementController(InWeakElementController)
	{}

	UDMXControlConsoleElementController* FDMXControlConsoleElementControllerModel::GetElementController() const
	{
		return WeakElementController.Get();
	}

	UDMXControlConsoleFaderBase* FDMXControlConsoleElementControllerModel::GetFirstAvailableFader() const
	{
		if (!WeakElementController.IsValid() || WeakElementController->GetElements().IsEmpty())
		{
			return nullptr;
		}

		const TArray<UDMXControlConsoleFaderBase*> Faders = WeakElementController->GetFaders();
		if (Faders.IsEmpty())
		{
			return nullptr;
		}

		return Faders[0];
	}

	UDMXControlConsoleFixturePatchMatrixCell* FDMXControlConsoleElementControllerModel::GetMatrixCellElement() const
	{
		if (!WeakElementController.IsValid() || !HasSingleElement())
		{
			return nullptr;
		}

		const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> Elements = WeakElementController->GetElements();
		const TScriptInterface<IDMXControlConsoleFaderGroupElement>* ElementPtr =
			Algo::FindByPredicate(Elements,
				[](const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element)
				{
					return Element && IsValid(Cast<UDMXControlConsoleFixturePatchMatrixCell>(Element.GetObject()));
				});

		UDMXControlConsoleFixturePatchMatrixCell* MatrixCellElement = ElementPtr ? Cast<UDMXControlConsoleFixturePatchMatrixCell>(ElementPtr->GetObject()) : nullptr;
		return MatrixCellElement;
	}

	FString FDMXControlConsoleElementControllerModel::GetRelativeControllerName() const
	{
		if (!WeakElementController.IsValid())
		{
			return FString();
		}

		if (!HasSingleElement())
		{
			return WeakElementController->GetControllerName();
		}

		const UDMXControlConsoleFaderBase* FirstFader = GetFirstAvailableFader();
		if (!FirstFader)
		{
			return WeakElementController->GetControllerName();
		}

		return FirstFader->GetFaderName();
	}

	float FDMXControlConsoleElementControllerModel::GetRelativeValue() const
	{
		if (!WeakElementController.IsValid())
		{
			return 0.f;
		}

		if (!HasUniformDataType())
		{
			return WeakElementController->GetValue();
		}

		const UDMXControlConsoleFaderBase* FirstFader = GetFirstAvailableFader();
		if (!FirstFader)
		{
			return WeakElementController->GetValue();
		}

		return static_cast<float>(FirstFader->GetValue());
	}

	float FDMXControlConsoleElementControllerModel::GetRelativeMinValue() const
	{
		if (!WeakElementController.IsValid())
		{
			return 0.f;
		}

		if (!HasUniformDataType())
		{
			return WeakElementController->GetMinValue();
		}

		const UDMXControlConsoleFaderBase* FirstFader = GetFirstAvailableFader();
		if (!FirstFader)
		{
			return WeakElementController->GetMinValue();
		}

		return static_cast<float>(FirstFader->GetMinValue());
	}

	float FDMXControlConsoleElementControllerModel::GetRelativeMaxValue() const
	{
		if (!WeakElementController.IsValid())
		{
			return 0.f;
		}

		if (!HasUniformDataType())
		{
			return WeakElementController->GetMaxValue();
		}

		const UDMXControlConsoleFaderBase* FirstFader = GetFirstAvailableFader();
		if (!FirstFader)
		{
			return WeakElementController->GetMaxValue();
		}

		return static_cast<float>(FirstFader->GetMaxValue());
	}

	bool FDMXControlConsoleElementControllerModel::HasSingleElement() const
	{
		return WeakElementController.IsValid() && WeakElementController->GetElements().Num() == 1;
	}

	bool FDMXControlConsoleElementControllerModel::HasUniformDataType() const
	{
		if (!WeakElementController.IsValid() || WeakElementController->GetElements().IsEmpty())
		{
			return false;
		}

		const TArray<UDMXControlConsoleFaderBase*> Faders = WeakElementController->GetFaders();
		if (Faders.IsEmpty())
		{
			return false;
		}

		const UDMXControlConsoleFaderBase* FirstFader = Faders[0];
		const bool bHasUniformDataType = Algo::AllOf(Faders, [FirstFader](const UDMXControlConsoleFaderBase* Fader)
			{
				return Fader && Fader->GetDataType() == FirstFader->GetDataType();
			});

		return bHasUniformDataType;
	}

	bool FDMXControlConsoleElementControllerModel::HasOnlyRawFaders() const
	{
		if (!WeakElementController.IsValid())
		{
			return false;
		}

		const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>>& Elements = WeakElementController->GetElements();
		return Algo::AllOf(Elements, [](const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element)
			{
				return Element && IsValid(Cast<UDMXControlConsoleRawFader>(Element.GetObject()));
			});
	}

	bool FDMXControlConsoleElementControllerModel::IsMuted() const
	{
		return WeakElementController.IsValid() && WeakElementController->IsMuted();
	}

	bool FDMXControlConsoleElementControllerModel::IsLocked() const
	{
		return WeakElementController.IsValid() && WeakElementController->IsLocked();
	}
}

#undef LOCTEXT_NAMESPACE 
