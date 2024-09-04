// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Common/TypedElementHandles.h"

#include "TedsStylingFactory.generated.h"

struct FSlateBrush;
struct FSlateColor;
class ISlateStyle;

UCLASS()
class UTedsStylingFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~UTedsStylingFactory() override = default;

	void RegisterTables(ITypedElementDataStorageInterface& DataStorage) override;
	virtual void RegisterQueries(ITypedElementDataStorageInterface& DataStorage) override;

	static void RegisterAllKnownStyles();
	
private:
	static void RegisterBrush(ITypedElementDataStorageInterface* DataStorage, const FName& StyleName, const FSlateBrush* Brush, const ISlateStyle& OwnerStyle);
	static void RegisterColor(ITypedElementDataStorageInterface* DataStorage, const FName& StyleName, const FSlateColor& Color, const ISlateStyle& OwnerStyle);
	static UE::Editor::DataStorage::RowHandle AddOrGetStyleRow(ITypedElementDataStorageInterface* DataStorage, const FName& StyleName, const ISlateStyle& OwnerStyle);
};
