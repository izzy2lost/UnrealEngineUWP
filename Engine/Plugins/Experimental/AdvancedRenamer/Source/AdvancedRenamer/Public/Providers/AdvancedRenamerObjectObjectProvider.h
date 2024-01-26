// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AdvancedRenamerObjectProviderInterface.h"
#include "AdvancedRenamerObjectProvider.h"
#include "Containers/UnrealString.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "AdvancedRenamerObjectObjectProvider.generated.h"

UCLASS(Abstract, BlueprintType, Blueprintable)
class ADVANCEDRENAMER_API UAdvancedRenamerObjectObjectProvider : public UObject, public IAdvancedRenamerObjectProvider
#if CPP
	, public FAdvancedRenamerObjectProvider
#endif
{
	GENERATED_BODY()

public:
	UAdvancedRenamerObjectObjectProvider() = default;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Advanced Rename Panel", meta = (DisplayName = "Set Object List"))
	void BP_SetObjectList(const TArray<UObject*>& InObjectList);
	virtual void BP_SetObjectList_Implementation(const TArray<UObject*>& InObjectList);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Advanced Rename Panel", meta = (DisplayName = "Add Object List"))
	void BP_AddObjectList(const TArray<UObject*>& InObjectList);
	virtual void BP_AddObjectList_Implementation(const TArray<UObject*>& InObjectList);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Advanced Rename Panel", meta = (DisplayName = "Add Object Data"))
	void BP_AddObjectData(UObject* InObject);
	virtual void BP_AddObjectData_Implementation(UObject* InObject);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Advanced Rename Panel", meta = (DisplayName = "Get Object"))
	UObject* BP_GetObject(int32 Index) const;
	virtual UObject* BP_GetObject_Implementation(int32 Index) const;

	virtual int32 Num() const override;
	virtual bool IsValidIndex(int32 Index) const override;
	virtual uint32 GetHash(int32 Index) const override;
	virtual FString GetOriginalName(int32 Index) const override;
	virtual bool RemoveIndex(int32 Index) override;
	virtual bool CanRename(int32 Index) const override;
	virtual bool ExecuteRename(int32 Index, const FString & NewName) override;
};
