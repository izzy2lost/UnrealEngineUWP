// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TedsRowPickingMode.h"
#include "Widgets/SCompoundWidget.h"

class TEDSPROPERTYEDITOR_API SPropertyMenuTedsRowPicker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPropertyMenuTedsRowPicker)
		: _AllowClear(true)
		, _ElementFilter()
	{}
		SLATE_ARGUMENT(bool, AllowClear)
		SLATE_ARGUMENT(TypedElementDataStorage::FQueryDescription, QueryFilter)
		SLATE_ARGUMENT(FOnShouldFilterTedsRow, ElementFilter)
		SLATE_EVENT(FOnTedsRowSelected, OnSet)
		SLATE_EVENT(FSimpleDelegate, OnClose)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:

	void OnClear();

	void OnElementSelected(TypedElementDataStorage::RowHandle RowHandle);

	void SetValue(TypedElementDataStorage::RowHandle RowHandle);

private:
	bool bAllowClear;

	TypedElementDataStorage::FQueryDescription QueryFilter;

	FOnShouldFilterTedsRow ElementFilter;

	FOnTedsRowSelected OnSet;

	FSimpleDelegate OnClose;

	FSimpleDelegate OnUseSelected;
};