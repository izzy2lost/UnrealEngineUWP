// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleFixturePatchMatrixCell.h"

#include "Algo/Find.h"
#include "Algo/Sort.h"
#include "Controllers/DMXControlConsoleMatrixCellController.h"
#include "DMXAttribute.h"
#include "DMXProtocolTypes.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFixturePatchCellAttributeFader.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityFixtureType.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleFixturePatchMatrixCell"

UDMXControlConsoleFaderGroup& UDMXControlConsoleFixturePatchMatrixCell::GetOwnerFaderGroupChecked() const
{
	UDMXControlConsoleFaderGroup* Outer = Cast<UDMXControlConsoleFaderGroup>(GetOuter());
	checkf(Outer, TEXT("Invalid outer for '%s', cannot get fader owner correctly."), *GetName());

	return *Outer;
}

UDMXControlConsoleElementController* UDMXControlConsoleFixturePatchMatrixCell::GetElementController()
{
	const UDMXControlConsoleFaderGroup& OwnerFaderGroup = GetOwnerFaderGroupChecked();
	return OwnerFaderGroup.GetControllerByElement(this);
}

int32 UDMXControlConsoleFixturePatchMatrixCell::GetIndex() const
{
	const UDMXControlConsoleFaderGroup* Outer = Cast<UDMXControlConsoleFaderGroup>(GetOuter());
	if (!ensureMsgf(Outer, TEXT("Invalid outer for '%s', cannot get fader index correctly."), *GetName()))
	{
		return INDEX_NONE;
	}

	const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>>& Elements = Outer->GetElements();
	const int32 Index = Elements.IndexOfByKey(this);
	return Index;
}

int32 UDMXControlConsoleFixturePatchMatrixCell::GetUniverseID() const
{
	if (!CellAttributeFaders.IsEmpty())
	{
		return CellAttributeFaders[0]->GetUniverseID();
	}

	return 1;
}

int32 UDMXControlConsoleFixturePatchMatrixCell::GetStartingAddress() const
{
	if (!CellAttributeFaders.IsEmpty())
	{
		return CellAttributeFaders[0]->GetStartingAddress();
	}

	return 1;
}

int32 UDMXControlConsoleFixturePatchMatrixCell::GetEndingAddress() const
{
	if (!CellAttributeFaders.IsEmpty())
	{
		return CellAttributeFaders.Last()->GetStartingAddress();
	}

	return 1;
}

#if WITH_EDITOR
void UDMXControlConsoleFixturePatchMatrixCell::SetIsMatchingFilter(bool bMatches)
{
	bIsMatchingFilter = HasVisibleInEditorCellAttributeFaders();
}
#endif // WITH_EDITOR

void UDMXControlConsoleFixturePatchMatrixCell::Destroy() 
{
	UDMXControlConsoleFaderGroup* Outer = Cast<UDMXControlConsoleFaderGroup>(GetOuter());
	if (!ensureMsgf(Outer, TEXT("Invalid outer for '%s', cannot destroy fader correctly."), *GetName()))
	{
		return;
	}

#if WITH_EDITOR
	Outer->PreEditChange(UDMXControlConsoleFaderGroup::StaticClass()->FindPropertyByName(UDMXControlConsoleFaderGroup::GetElementsPropertyName()));
#endif // WITH_EDITOR

	Outer->DeleteElement(this);

#if WITH_EDITOR
	Outer->PostEditChange();
#endif // WITH_EDITOR
}

UDMXControlConsoleFixturePatchCellAttributeFader* UDMXControlConsoleFixturePatchMatrixCell::AddFixturePatchCellAttributeFader(const FDMXFixtureCellAttribute& CellAttribute, const int32 InUniverseID, const int32 StartingChannel)
{
	UDMXControlConsoleFixturePatchCellAttributeFader* CellAttributeFader = NewObject<UDMXControlConsoleFixturePatchCellAttributeFader>(this, NAME_None, RF_Transactional);
	CellAttributeFader->SetPropertiesFromFixtureCellAttribute(CellAttribute, InUniverseID, StartingChannel);
	CellAttributeFaders.Add(CellAttributeFader);

	UDMXControlConsoleFaderGroup& OwnerFaderGroup = GetOwnerFaderGroupChecked();
	OwnerFaderGroup.OnElementAdded.Broadcast(CellAttributeFader);

	return CellAttributeFader;
}

void UDMXControlConsoleFixturePatchMatrixCell::DeleteCellAttributeFader(UDMXControlConsoleFixturePatchCellAttributeFader* CellAttributeFader)
{
	if (!ensureMsgf(CellAttributeFader, TEXT("Invalid fader, cannot delete from '%s'."), *GetName()))
	{
		return;
	}

	if (!ensureMsgf(CellAttributeFaders.Contains(CellAttributeFader), TEXT("'%s' matrix cell is not owner of '%s'. Cannot delete fader correctly."), *GetName(), *CellAttributeFader->GetName()))
	{
		return;
	}

	CellAttributeFaders.Remove(CellAttributeFader);

	UDMXControlConsoleFaderGroup& OwnerFaderGroup = GetOwnerFaderGroupChecked();
	OwnerFaderGroup.OnElementRemoved.Broadcast(CellAttributeFader);
}

void UDMXControlConsoleFixturePatchMatrixCell::SetPropertiesFromCell(const FDMXCell& Cell, const int32 InUniverseID, const int32 StartingChannel)
{
	// Order of initialization matters
	CellID = Cell.CellID;
	CellX = Cell.Coordinate.X;
	CellY = Cell.Coordinate.Y;

	const FString XToString = FString::FromInt(CellX);
	const FString YToString = FString::FromInt(CellY);

	UDMXControlConsoleFaderGroup& FaderGroup = GetOwnerFaderGroupChecked();
	UDMXEntityFixturePatch* FixturePatch = FaderGroup.GetFixturePatch();
	if (!FixturePatch)
	{
		return;
	}

	FDMXFixtureMatrix FixtureMatrix;
	if (!FixturePatch->GetMatrixProperties(FixtureMatrix))
	{
		return;
	}

	if (!UDMXEntityFixturePatch::GetOnFixturePatchChanged().IsBoundToObject(this))
	{
		UDMXEntityFixturePatch::GetOnFixturePatchChanged().AddUObject(this, &UDMXControlConsoleFixturePatchMatrixCell::OnFixturePatchChanged);
	}

	const TArray<FDMXFixtureCellAttribute> CellAttributes = FixtureMatrix.CellAttributes;
	TMap<FDMXAttributeName, int32> AttributeToChannelMap;
	FixturePatch->GetMatrixCellChannelsRelative(Cell.Coordinate, AttributeToChannelMap);

	for (const FDMXFixtureCellAttribute& CellAttribute : CellAttributes)
	{
		const FDMXAttributeName& AttributeName = CellAttribute.Attribute;
		const int32 RelativeChannel = AttributeToChannelMap.FindRef(AttributeName) - 1;
		const int32 AbsoluteChannel = StartingChannel + RelativeChannel;

		UDMXControlConsoleFixturePatchCellAttributeFader* CellAttributeFader = AddFixturePatchCellAttributeFader(CellAttribute, InUniverseID, AbsoluteChannel);
		const FString& ControllerName = CellAttributeFader ? CellAttributeFader->GetFaderName() : "";
		CreateMatrixCellController(CellAttributeFader, ControllerName);
	}

	SortElementsByStartingAddress();
}

UDMXControlConsoleMatrixCellController* UDMXControlConsoleFixturePatchMatrixCell::CreateMatrixCellController(const TScriptInterface<IDMXControlConsoleFaderGroupElement>& InElement, const FString& ControllerName)
{
	if (!InElement)
	{
		return nullptr;
	}

	const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> ElementAsArray = { InElement };
	UDMXControlConsoleMatrixCellController* MatrixCellController = CreateMatrixCellController(ElementAsArray, ControllerName);
	return MatrixCellController;
}

UDMXControlConsoleMatrixCellController* UDMXControlConsoleFixturePatchMatrixCell::CreateMatrixCellController(const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> InElements, const FString& ControllerName)
{
	if (InElements.IsEmpty())
	{
		return nullptr;
	}

	UDMXControlConsoleMatrixCellController* MatrixCellController = NewObject<UDMXControlConsoleMatrixCellController>(this, NAME_None, RF_Transactional);
	MatrixCellController->Possess(InElements);

	const FString NewName = ControllerName.IsEmpty() ? FString::FromInt(MatrixCellControllers.Num() + 1) : ControllerName;
	MatrixCellController->SetControllerName(NewName);

	MatrixCellControllers.Add(MatrixCellController);
	return MatrixCellController;
}

void UDMXControlConsoleFixturePatchMatrixCell::DeleteMatrixCellController(UDMXControlConsoleMatrixCellController* MatrixCellController)
{
	if (!ensureMsgf(MatrixCellController, TEXT("Invalid matrix cell controller, cannot delete from '%s'."), *GetName()))
	{
		return;
	}

	if (!ensureMsgf(MatrixCellControllers.Contains(MatrixCellController), TEXT("'%s' matrix cell is not owner of '%s'. Cannot delete controller correctly."), *GetName(), *MatrixCellController->GetControllerName()))
	{
		return;
	}

	MatrixCellControllers.Remove(MatrixCellController);
}

UDMXControlConsoleMatrixCellController* UDMXControlConsoleFixturePatchMatrixCell::GetControllerByElement(const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element) const
{
	if (Element)
	{
		const TObjectPtr<UDMXControlConsoleMatrixCellController>* ControllerPtr = Algo::FindByPredicate(MatrixCellControllers, [Element](const UDMXControlConsoleMatrixCellController* Controller)
			{
				return Controller && Controller->GetElements().Contains(Element);
			});

		return ControllerPtr ? ControllerPtr->Get() : nullptr;
	}

	return nullptr;
}

void UDMXControlConsoleFixturePatchMatrixCell::SortElementsByStartingAddress() const
{
	const auto SortElementsByStartingAddressLambda = [](const TScriptInterface<IDMXControlConsoleFaderGroupElement>& ItemA, const TScriptInterface<IDMXControlConsoleFaderGroupElement>& ItemB)
		{
			const int32 StartingAddressA = ItemA->GetStartingAddress();
			const int32 StartingAddressB = ItemB->GetStartingAddress();

			return StartingAddressA < StartingAddressB;
		};

	Algo::Sort(CellAttributeFaders, SortElementsByStartingAddressLambda);

	const auto SortControllersByStartingAddressLambda = [SortElementsByStartingAddressLambda](const UDMXControlConsoleMatrixCellController* ItemA, const UDMXControlConsoleMatrixCellController* ItemB)
		{
			const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>>& ElementsA = ItemA->GetElements();
			const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>>& ElementsB = ItemB->GetElements();

			Algo::Sort(ElementsA, SortElementsByStartingAddressLambda);
			Algo::Sort(ElementsB, SortElementsByStartingAddressLambda);

			if (ElementsA.IsEmpty() || ElementsB.IsEmpty())
			{
				return false;
			}

			const TScriptInterface<IDMXControlConsoleFaderGroupElement> ElementA = ElementsA[0];
			const TScriptInterface<IDMXControlConsoleFaderGroupElement> ElementB = ElementsB[0];

			const int32 StartingAddressA = ElementA->GetStartingAddress();
			const int32 StartingAddressB = ElementB->GetStartingAddress();

			return StartingAddressA < StartingAddressB;
		};

	Algo::Sort(MatrixCellControllers, SortControllersByStartingAddressLambda);
}

#if WITH_EDITOR
bool UDMXControlConsoleFixturePatchMatrixCell::HasVisibleInEditorCellAttributeFaders() const
{
	for (const UDMXControlConsoleFaderBase* CellAttributeFader : CellAttributeFaders)
	{
		if (CellAttributeFader && CellAttributeFader->IsMatchingFilter())
		{
			return true;
		}
	}

	return false;
}
#endif // WITH_EDITOR

#if WITH_EDITOR
void UDMXControlConsoleFixturePatchMatrixCell::ShowAllFadersInEditor()
{
	for (UDMXControlConsoleFaderBase* CellAttributeFader : CellAttributeFaders)
	{
		if (!CellAttributeFader)
		{
			continue;
		}

		CellAttributeFader->SetIsMatchingFilter(true);
	}
}
#endif // WITH_EDITOR

void UDMXControlConsoleFixturePatchMatrixCell::PostLoad()
{
	Super::PostLoad();

	UDMXControlConsoleFaderGroup& FaderGroup = GetOwnerFaderGroupChecked();
	UDMXEntityFixturePatch* FixturePatch = FaderGroup.GetFixturePatch();
	if (!FixturePatch)
	{
		UpdateMatrixCellControllers();
		return;
	}

	if (!UDMXEntityFixturePatch::GetOnFixturePatchChanged().IsBoundToObject(this))
	{
		UDMXEntityFixturePatch::GetOnFixturePatchChanged().AddUObject(this, &UDMXControlConsoleFixturePatchMatrixCell::OnFixturePatchChanged);
	}

	UpdateFixturePatchCellAttributeFaders(FixturePatch);
}

void UDMXControlConsoleFixturePatchMatrixCell::OnFixturePatchChanged(const UDMXEntityFixturePatch* InFixturePatch)
{
	UDMXControlConsoleFaderGroup& FaderGroup = GetOwnerFaderGroupChecked();
	UDMXEntityFixturePatch* MyFixturePatch = FaderGroup.GetFixturePatch();
	if (!MyFixturePatch ||
		MyFixturePatch != InFixturePatch)
	{
		return;
	}

	UpdateFixturePatchCellAttributeFaders(MyFixturePatch);
}

void UDMXControlConsoleFixturePatchMatrixCell::UpdateFixturePatchCellAttributeFaders(UDMXEntityFixturePatch* InFixturePatch)
{
	if (!InFixturePatch || !InFixturePatch->GetActiveMode())
	{
		return;
	}

	FDMXFixtureMatrix FixtureMatrix;
	if (!InFixturePatch->GetMatrixProperties(FixtureMatrix))
	{
		return;
	}

	const int32 UniverseID = InFixturePatch->GetUniverseID();
	const int32 StartingChannel = InFixturePatch->GetStartingChannel();

	const TArray<FDMXFixtureCellAttribute> CellAttributes = FixtureMatrix.CellAttributes;
	const FIntPoint Coordinate = FIntPoint(CellX, CellY);
	TMap<FDMXAttributeName, int32> AttributeToChannelMap;

	InFixturePatch->GetMatrixCellChannelsRelative(Coordinate, AttributeToChannelMap);
	// Destroy all FixturePatchCellAttributeFaders which Attribute is no longer in use
	const auto IsAttributNoLongerInUseLambda = [AttributeToChannelMap](UDMXControlConsoleFaderBase* Fader)
		{
			const UDMXControlConsoleFixturePatchCellAttributeFader* CellAttributeFader = Cast<UDMXControlConsoleFixturePatchCellAttributeFader>(Fader);
			if (!CellAttributeFader)
			{
				return true;
			}

			const FDMXAttributeName& AttributeName = CellAttributeFader->GetAttributeName();
			if (!AttributeToChannelMap.Contains(AttributeName))
			{
				return true;
			}

			return false;
		};

	CellAttributeFaders.RemoveAll(IsAttributNoLongerInUseLambda);

	// Update FixturePatchCellAttributeFaders which Attributes are already in use and create CellAttributeFaders for new Attributes
	for (const FDMXFixtureCellAttribute& CellAttribute : CellAttributes)
	{
		const FDMXAttributeName& AttributeName = CellAttribute.Attribute;

		const auto IsAttributeAlreadyInUseLambda = [AttributeName](UDMXControlConsoleFaderBase* Fader)
			{
				if (!Fader)
				{
					return false;
				}

				const UDMXControlConsoleFixturePatchCellAttributeFader* CellAttributeFader = Cast<UDMXControlConsoleFixturePatchCellAttributeFader>(Fader);
				if (!CellAttributeFader)
				{
					return false;
				}

				if (CellAttributeFader->GetAttributeName() != AttributeName)
				{
					return false;
				}

				return true;
			};

		const int32 RelativeChannel = AttributeToChannelMap.FindRef(AttributeName) - 1;
		const int32 AbsoluteChannel = StartingChannel + RelativeChannel;

		TObjectPtr<UDMXControlConsoleFaderBase>* MyFader = Algo::FindByPredicate(CellAttributeFaders, IsAttributeAlreadyInUseLambda);
		if (MyFader)
		{
			UDMXControlConsoleFixturePatchCellAttributeFader* MyCellAttributeFader = Cast<UDMXControlConsoleFixturePatchCellAttributeFader>(MyFader->Get());
			if (MyCellAttributeFader)
			{
				// SetPropertiesFromFixtureCellAttribute gets the the default value from the patch and sets it. 
				// Hence here remember the current value and set it back after setting properties.
				const uint32 Value = MyCellAttributeFader->GetValue();
				MyCellAttributeFader->SetPropertiesFromFixtureCellAttribute(CellAttribute, UniverseID, AbsoluteChannel);
				MyCellAttributeFader->SetValue(Value);
			}
		}
		else
		{
			UDMXControlConsoleFixturePatchCellAttributeFader* NewCellAttributeFader = AddFixturePatchCellAttributeFader(CellAttribute, UniverseID, AbsoluteChannel);
			const FString& ControllerName = NewCellAttributeFader ? NewCellAttributeFader->GetFaderName() : "";
			CreateMatrixCellController(NewCellAttributeFader, ControllerName);
		}
	}

	const auto SortFadersByStartingAddressLambda = [](const UDMXControlConsoleFaderBase* ItemA, const UDMXControlConsoleFaderBase* ItemB)
	{
		const int32 StartingAddressA = ItemA->GetStartingAddress();
		const int32 StartingAddressB = ItemB->GetStartingAddress();

		return StartingAddressA < StartingAddressB;
	};

	Algo::Sort(CellAttributeFaders, SortFadersByStartingAddressLambda);
	UpdateMatrixCellControllers();
}

void UDMXControlConsoleFixturePatchMatrixCell::UpdateMatrixCellControllers()
{
	// Create matrix cell controllers for elements with no controller
	for (UDMXControlConsoleFaderBase* CellAttributeFader : CellAttributeFaders)
	{
		if (!CellAttributeFader)
		{
			continue;
		}

		if (GetControllerByElement(CellAttributeFader))
		{
			continue;
		}

		const FString ControllerName = CellAttributeFader->GetFaderName();

		const uint8 NumChannels = static_cast<uint8>(CellAttributeFader->GetDataType()) + 1;
		const float ValueRange = FMath::Pow(2.f, 8.f * NumChannels) - 1;
		const float ControllerValue = CellAttributeFader->GetValue() / ValueRange;
		const float ControllerMinValue = CellAttributeFader->GetMinValue() / ValueRange;
		const float ControllerMaxValue = CellAttributeFader->GetMaxValue() / ValueRange;
		
		UDMXControlConsoleMatrixCellController* NewController = CreateMatrixCellController(CellAttributeFader, ControllerName);
		if (NewController)
		{
			NewController->SetValue(ControllerValue);
			NewController->SetMinValue(ControllerValue);
			NewController->SetMaxValue(ControllerValue);
		}
	}

	// Remove all matrix cell controllers with no elements
	MatrixCellControllers.RemoveAll([](const UDMXControlConsoleMatrixCellController* MatrixCellController)
		{
			return MatrixCellController && MatrixCellController->GetElements().IsEmpty();
		});

	SortElementsByStartingAddress();
}

#undef LOCTEXT_NAMESPACE
