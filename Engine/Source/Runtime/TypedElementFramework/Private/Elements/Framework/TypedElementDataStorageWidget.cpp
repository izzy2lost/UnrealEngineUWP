// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Framework/TypedElementDataStorageWidget.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"

STedsWidget::STedsWidget()
	: UiRowHandle(TypedElementDataStorage::InvalidRowHandle)
{

}

void STedsWidget::Construct(const FArguments& InArgs)
{
	UiRowHandle = InArgs._UiRowHandle;

	ChildSlot
	[
		InArgs._Content.Widget
	];
}

void STedsWidget::SetContent(const TSharedRef< SWidget >& InContent)
{
	ChildSlot
	[
		InContent
	];
}