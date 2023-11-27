// Copyright Epic Games, Inc. All Rights Reserved.

#include "Controllers/DMXControlConsoleElementController.h"

#include "Algo/AnyOf.h"
#include "Algo/Transform.h"
#include "DMXControlConsoleFaderBase.h"
#include "DMXControlConsoleFaderGroup.h"
#include "Oscillators/DMXControlConsoleFloatOscillator.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleElementController"

void UDMXControlConsoleElementController::Possess(const TScriptInterface<IDMXControlConsoleFaderGroupElement>& InElement)
{
	if (!InElement)
	{
		return;
	}

	UDMXControlConsoleElementController* OldController = InElement->GetElementController();
	if (OldController == this)
	{
		return;
	}
	 
	if (OldController)
	{
		OldController->UnPossess(InElement);
	}

	Elements.AddUnique(InElement);

	SyncElements();
}

void UDMXControlConsoleElementController::Possess(TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> InElements)
{
	InElements.RemoveAll([this](const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element)
		{
			if (Element)
			{
				const UDMXControlConsoleElementController* OldController = Element->GetElementController();
				return OldController == this;
			}
			return true;
		});

	for (const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element : InElements)
	{
		if (!Element)
		{
			continue;
		}

		UDMXControlConsoleElementController* OldController = Element->GetElementController();
		if (OldController)
		{
			OldController->UnPossess(Element);
		}
	}
	
	Elements.Append(InElements);
	SyncElements();
}

void UDMXControlConsoleElementController::UnPossess(const TScriptInterface<IDMXControlConsoleFaderGroupElement>& InElement)
{
	if (InElement)
	{
		Elements.Remove(InElement);
	}
}

void UDMXControlConsoleElementController::ClearElements()
{
	Elements.Reset();
}

UDMXControlConsoleFaderGroup& UDMXControlConsoleElementController::GetOwnerFaderGroupChecked() const
{
	UDMXControlConsoleFaderGroup* Outer = Cast<UDMXControlConsoleFaderGroup>(GetOuter());
	checkf(Outer, TEXT("Invalid outer for '%s', cannot get controller owner correctly."), *GetName());

	return *Outer;
}

int32 UDMXControlConsoleElementController::GetIndex() const
{
	const UDMXControlConsoleFaderGroup& OwnerFaderGroup = GetOwnerFaderGroupChecked();

	const TArray<UDMXControlConsoleElementController*> Controllers = OwnerFaderGroup.GetElementControllers();
	const int32 Index = Controllers.IndexOfByKey(this);
	return Index;
}

TArray<UDMXControlConsoleFaderBase*> UDMXControlConsoleElementController::GetFaders() const
{
	TArray<UDMXControlConsoleFaderBase*> Faders;
	Algo::TransformIf(Elements, Faders,
		[](const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element)
		{
			return IsValid(Cast<UDMXControlConsoleFaderBase>(Element.GetObject()));
		},
		[](const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element)
		{
			return Cast<UDMXControlConsoleFaderBase>(Element.GetObject());
		});

	return Faders;
}

FString UDMXControlConsoleElementController::GenerateControllerNameByElementsNames() const
{
	FString NewName = TEXT("");
	for (const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element : Elements)
	{
		if (!Element)
		{
			continue;
		}

		const UDMXControlConsoleFaderBase* Fader = Cast<UDMXControlConsoleFaderBase>(Element.GetObject());
		const FString ElementName = Fader ? Fader->GetFaderName() : FString();
		if (NewName.Contains(ElementName))
		{
			continue;
		}

		NewName.Append(ElementName);
		if (Elements.Last() != Element)
		{
			NewName.Append("_");
		}
	}

	return NewName;
}

void UDMXControlConsoleElementController::SetControllerName(const FString& NewName)
{
	ControllerName = NewName;
}

void UDMXControlConsoleElementController::SetValue(float NewValue)
{
	Value = FMath::Clamp(NewValue, MinValue, MaxValue);

	const TArray<UDMXControlConsoleFaderBase*> Faders = GetFaders();
	for (TWeakObjectPtr<UDMXControlConsoleFaderBase> Fader : Faders)
	{
		if (!Fader.IsValid())
		{
			continue;
		}

		const uint8 NumChannels = static_cast<uint8>(Fader->GetDataType()) + 1;
		const uint32 ValueRange = static_cast<uint32>(FMath::Pow(2.f, 8.f * NumChannels) - 1);
		const uint32 NewFaderValue = static_cast<uint32>(FMath::RoundToInt(ValueRange * Value));
		Fader->SetValue(NewFaderValue);
	}
}

void UDMXControlConsoleElementController::SetMinValue(float NewMinValue)
{
	MinValue = FMath::Clamp(NewMinValue, 0.f, MaxValue);
	Value = FMath::Clamp(Value, MinValue, MaxValue);

	const TArray<UDMXControlConsoleFaderBase*> Faders = GetFaders();
	for (TWeakObjectPtr<UDMXControlConsoleFaderBase> Fader : Faders)
	{
		if (!Fader.IsValid())
		{
			continue;
		}

		const uint8 NumChannels = static_cast<uint8>(Fader->GetDataType()) + 1;
		const uint32 ValueRange = static_cast<uint32>(FMath::Pow(2.f, 8.f * NumChannels) - 1);
		const uint32 NewFaderMinValue = static_cast<uint32>(FMath::RoundToInt((ValueRange * MinValue)));
		Fader->SetMinValue(NewFaderMinValue);
	}
}

void UDMXControlConsoleElementController::SetMaxValue(float NewMaxValue)
{
	MaxValue = FMath::Clamp(NewMaxValue, MinValue, 1.f);
	Value = FMath::Clamp(Value, MinValue, MaxValue);

	const TArray<UDMXControlConsoleFaderBase*> Faders = GetFaders();
	for (TWeakObjectPtr<UDMXControlConsoleFaderBase> Fader : Faders)
	{
		if (!Fader.IsValid())
		{
			continue;
		}

		const uint8 NumChannels = static_cast<uint8>(Fader->GetDataType()) + 1;
		const uint32 ValueRange = static_cast<uint32>(FMath::Pow(2.f, 8.f * NumChannels) - 1);
		const uint32 NewFaderMaxValue = static_cast<uint32>(FMath::RoundToInt(ValueRange * MaxValue));
		Fader->SetMaxValue(NewFaderMaxValue);
	}
}

void UDMXControlConsoleElementController::SetMute(bool bMute)
{
	bIsMuted = bMute;
}

void UDMXControlConsoleElementController::ToggleMute()
{
	SetMute(!bIsMuted);
}

void UDMXControlConsoleElementController::SetLock(bool bLock)
{
	bIsLocked = bLock;

	const TArray<UDMXControlConsoleFaderBase*> Faders = GetFaders();
	for (TWeakObjectPtr<UDMXControlConsoleFaderBase> Fader : Faders)
	{
		if (!Fader.IsValid())
		{
			continue;
		}

		Fader->Modify();
		Fader->SetLock(bIsLocked);
	}
}

void UDMXControlConsoleElementController::ToggleLock()
{
	SetLock(!bIsLocked);
}

#if WITH_EDITOR
bool UDMXControlConsoleElementController::IsActive() const
{
	return GetOwnerFaderGroupChecked().IsActive();
}
#endif // WITH_EDITOR

#if WITH_EDITOR
bool UDMXControlConsoleElementController::IsMatchingFilter() const
{
	const bool bIsAnyElementMatchingFilter = Algo::AnyOf(Elements, 
		[](const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element)
		{
			return Element && Element->IsMatchingFilter();
		});

	return bIsAnyElementMatchingFilter;
}
#endif // WITH_EDITOR

void UDMXControlConsoleElementController::Destroy()
{
	UDMXControlConsoleFaderGroup& OwnerFaderGroup = GetOwnerFaderGroupChecked();

#if WITH_EDITOR
	OwnerFaderGroup.PreEditChange(UDMXControlConsoleFaderGroup::StaticClass()->FindPropertyByName(UDMXControlConsoleFaderGroup::GetElementControllersPropertyName()));
#endif // WITH_EDITOR

	OwnerFaderGroup.DeleteElementController(this);

#if WITH_EDITOR
	OwnerFaderGroup.PostEditChange();
#endif // WITH_EDITOR
}

void UDMXControlConsoleElementController::PostInitProperties()
{
	Super::PostInitProperties();

	ControllerName = GetName();
}

#if WITH_EDITOR
void UDMXControlConsoleElementController::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UDMXControlConsoleElementController, FloatOscillatorClass))
	{
		if (UClass* StrongFloatOscillatorClass = FloatOscillatorClass.Get())
		{
			FloatOscillator = NewObject<UDMXControlConsoleFloatOscillator>(this, StrongFloatOscillatorClass, NAME_None, RF_Transactional | RF_Public);
		}
		else
		{
			FloatOscillator = nullptr;
		}
	}
}
#endif // WITH_EDITOR

void UDMXControlConsoleElementController::Tick(float DeltaTime)
{
	if (FloatOscillator)
	{
		const float ValueRange = MaxValue - MinValue;
		const float NewValue = FMath::Clamp(MinValue + FloatOscillator->GetNormalizedValue(DeltaTime) * ValueRange, MinValue, MaxValue);
		SetValue(NewValue);
	}
}

bool UDMXControlConsoleElementController::IsTickable() const
{
	return
		!bIsLocked &&
		FloatOscillator != nullptr;
}

TStatId UDMXControlConsoleElementController::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDMXControlConsoleElementController, STATGROUP_Tickables);
}

ETickableTickType UDMXControlConsoleElementController::GetTickableTickType() const
{
	return ETickableTickType::Conditional;
}

void UDMXControlConsoleElementController::SyncElements() const
{
	const TArray<UDMXControlConsoleFaderBase*> Faders = GetFaders();
	for (TWeakObjectPtr<UDMXControlConsoleFaderBase> Fader : Faders)
	{
		if (!Fader.IsValid())
		{
			continue;
		}

		const uint8 NumChannels = static_cast<uint8>(Fader->GetDataType()) + 1;
		const uint32 ValueRange = static_cast<uint32>(FMath::Pow(2.f, 8.f * NumChannels) - 1);

		const uint32 NewFaderValue = static_cast<uint32>(FMath::RoundToInt(ValueRange * Value));
		const uint32 NewFaderMinValue = static_cast<uint32>(FMath::RoundToInt(ValueRange * MinValue));
		const uint32 NewFaderMaxValue = static_cast<uint32>(FMath::RoundToInt(ValueRange * MaxValue));

		Fader->Modify();
		Fader->SetValue(NewFaderValue);
		Fader->SetMinValue(NewFaderMinValue);
		Fader->SetMaxValue(NewFaderMaxValue);
		Fader->SetLock(bIsLocked);
	}
}

#undef LOCTEXT_NAMESPACE
