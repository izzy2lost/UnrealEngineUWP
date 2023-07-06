// Copyright Epic Games, Inc. All Rights Reserved.

#include "AddOuterSource.h"

#include "StreamEditor/Model/DisplayUtils.h"

#include "GameFramework/Actor.h"

#define LOCTEXT_NAMESPACE "FAddOuterSource"

namespace UE::MultiUserReplicationEditor
{
	bool FAddOuterSource::TrySetObjectIfValid(UObject* InObject)
	{
		if (InObject
			// Allow anything in actors but nothing above: persistent level should not be added.
			&& InObject->IsInA(AActor::StaticClass()) && !InObject->IsA<AActor>())
		{
			Object = InObject;
			return true;
		}
		return false;
	}

	ConcertSharedSlate::FSourceDisplayInfo FAddOuterSource::GetDisplayInfo() const
	{
		if (!ensure(Object.IsValid()))
		{
			return {{ LOCTEXT("Invalid", "Invalid") }};
		}
		
		UObject* Outer = Object->GetOuter();
		check(Outer);
		
		const FText Label = [Outer]()
		{
			if (Outer->IsA<AActor>())
			{
				return FText::Format(LOCTEXT("Add.OwningActor", "Add Actor {0}"), FText::FromString(DisplayUtils::GetObjectDisplayString(*Outer)));
			}
			if (UActorComponent* AsComponent = Cast<UActorComponent>(Outer))
			{
				return FText::Format(LOCTEXT("Add.Component", "Add Component {0} of {1}"),
					FText::FromString(DisplayUtils::GetObjectDisplayString(*Outer)),
					FText::FromString(DisplayUtils::GetObjectDisplayString(*AsComponent->GetOwner()))
				);
			}
			return LOCTEXT("Add.Outer", "Add Outer");
		}();
		return {
			{ Label, LOCTEXT("Tooltip", "Adds the owning object"), DisplayUtils::GetObjectIcon(*Outer) },
			ConcertSharedSlate::ESourceType::AddOnClick
		};
	}

	uint32 FAddOuterSource::GetNumSelectableItems() const
	{
		return Object.IsValid() ? 1 : 0;
	}

	void FAddOuterSource::EnumerateSelectableItems(TFunctionRef<EBreakBehavior(const FSelectableObjectInfo& SelectableOption)> Delegate) const
	{
		if (Object.IsValid())
		{
			Delegate(FSelectableObjectInfo{ *Object->GetOuter() });
		}
	}
}

#undef LOCTEXT_NAMESPACE