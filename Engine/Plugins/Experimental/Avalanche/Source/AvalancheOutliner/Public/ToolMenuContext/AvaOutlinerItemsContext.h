// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaOutlinerDefines.h"
#include "Containers/Array.h"
#include "Containers/ContainersFwd.h"
#include "Templates/SharedPointer.h"
#include "UObject/Object.h"
#include "AvaOutlinerItemsContext.generated.h"

class FAvaOutlinerView;

UCLASS(MinimalAPI)
class UAvaOutlinerItemsContext : public UObject
{
	GENERATED_BODY()
	
	friend FAvaOutlinerView;
	
public:
	UAvaOutlinerItemsContext() = default;
	
	TSharedPtr<FAvaOutlinerView> GetOutlinerView() const { return OutlinerViewWeak.Pin(); }
	
	TConstArrayView<FAvaOutlinerItemWeakPtr> GetItems() const { return ItemListWeak; }
	
private:
	TWeakPtr<FAvaOutlinerView> OutlinerViewWeak;
	
	TArray<FAvaOutlinerItemWeakPtr> ItemListWeak;
};
