// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AdvancedRenamerProviderInterface.h"
#include "UObject/Interface.h"
#include "AdvancedRenamerObjectProviderInterface.generated.h"

UINTERFACE()
class ADVANCEDRENAMER_API UAdvancedRenamerObjectProvider : public UInterface
{
	GENERATED_BODY()
};

class ADVANCEDRENAMER_API IAdvancedRenamerObjectProvider 
#if CPP
	: public IAdvancedRenamerProvider
#endif
{
	GENERATED_BODY()

public:

	/** 
	 * Set as BNE so that there will be complaints when these are not implemented.
	 * A BIE will silently fail.
	 */	

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Advanced Rename Panel", meta = (DisplayName = "Num"))
	int32 BP_Num() const;
	virtual int32 BP_Num_Implementation() const PURE_VIRTUAL(IAdvancedRenamerObjectProvider::BP_Num, return 0;)

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Advanced Rename Panel", meta = (DisplayName = "IsValidIndex"))
	bool BP_IsValidIndex(int32 Index) const;
	virtual bool BP_IsValidIndex_Implementation(int32 Index) const PURE_VIRTUAL(IAdvancedRenamerObjectProvider::BP_IsValidIndex, return false;)

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Advanced Rename Panel", meta = (DisplayName = "GetHash"))
	int32 BP_GetHash(int32 Index) const;
	virtual int32 BP_GetHash_Implementation(int32 Index) const PURE_VIRTUAL(IAdvancedRenamerObjectProvider::BP_GetHash, return 0;)

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Advanced Rename Panel", meta = (DisplayName = "GetOriginalName"))
	FString BP_GetOriginalName(int32 Index) const;
	virtual FString BP_GetOriginalName_Implementation(int32 Index) const PURE_VIRTUAL(IAdvancedRenamerObjectProvider::BP_GetOriginalName, return "";)

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Advanced Rename Panel", meta = (DisplayName = "RemoveIndex"))
	bool BP_RemoveIndex(int32 Index);
	virtual bool BP_RemoveIndex_Implementation(int32 Index) PURE_VIRTUAL(IAdvancedRenamerObjectProvider::BP_RemoveIndex, return false;)

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Advanced Rename Panel", meta = (DisplayName = "CanRename"))
	bool BP_CanRename(int32 Index) const;
	virtual bool BP_CanRename_Implementation(int32 Index) const PURE_VIRTUAL(IAdvancedRenamerObjectProvider::BP_CanRename, return false;)

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Advanced Rename Panel", meta = (DisplayName = "Rename"))
	bool BP_ExecuteRename(int32 Index, const FString& NewName);
	virtual bool BP_ExecuteRename_Implementation(int32 Index, const FString& NewName) PURE_VIRTUAL(IAdvancedRenamerObjectProvider::BP_Rename, return false;)
};
