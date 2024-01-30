// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "UObject/Object.h"
#include "AvaOutlinerToolBarContext.generated.h"

class FAvaOutlinerView;

UCLASS(MinimalAPI)
class UAvaOutlinerToolBarContext : public UObject
{
	GENERATED_BODY()
	
	friend FAvaOutlinerView;
	
public:
	UAvaOutlinerToolBarContext() = default;
	
	TSharedPtr<FAvaOutlinerView> GetOutlinerView() const { return OutlinerViewWeak.Pin(); }
	
private:
	TWeakPtr<FAvaOutlinerView> OutlinerViewWeak;
};
