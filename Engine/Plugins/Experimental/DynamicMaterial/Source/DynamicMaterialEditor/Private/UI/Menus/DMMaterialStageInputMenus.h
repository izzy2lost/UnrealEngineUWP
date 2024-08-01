// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Templates/SubclassOf.h"

class FMenuBuilder;
class SDMMaterialSlotEditor;
class SDMMaterialStage;
class SWidget;
class UDMMaterialSlot;
class UDMMaterialStage;
class UDMMaterialStageExpression;
class UDMMaterialStageGradient;
class UDMMaterialStageThroughput;
class UDMMaterialValue;
class UToolMenu;
enum class EDMMaterialPropertyType : uint8;
enum class EDMValueType : uint8;
struct FDMMaterialStageConnector;

class FDMMaterialStageInputMenus final
{
public:
	/** Generate right click menu for changing inputs */
	static TSharedRef<SWidget> MakeStageChangeInputMenu(UDMMaterialStageThroughput* InThroughput, const int32 InInputIndex, 
		const int32 InInputChannel);

private:
	static FName GetStageChangeInputMenuName();

	static bool CanAcceptSubChannels(UDMMaterialStageThroughput* const InThroughput, const int32 InInputIndex, const EDMValueType InOutputType);

	static bool CanAcceptSubChannels(UDMMaterialStageThroughput* const InThroughput, const int32 InInputIndex, 
		const FDMMaterialStageConnector& InOutputConnector);

	/** Previous Stage */
	static void GenerateChangeInputMenu_PreviousStages(FMenuBuilder& InChildMenuBuilder, UDMMaterialStageThroughput* InThroughput, 
		const int32 InInputIndex, const int32 InInputChannel);

	static void GenerateChangeInputMenu_PreviousStage_Outputs(FMenuBuilder& InChildMenuBuilder, UDMMaterialStageThroughput* InThroughput,
		const int32 InInputIndex, const int32 InInputChannel, EDMMaterialPropertyType InMaterialProperty);

	static void GenerateChangeInputMenu_PreviousStage_Output_Channels(FMenuBuilder& InChildMenuBuilder, UDMMaterialStageThroughput* InThroughput,
		const int32 InInputIndex, const int32 InInputChannel, EDMMaterialPropertyType InMaterialProperty, const int32 InOutputChannel);

	static void GenerateChangeInputMenu_PreviousStage_Outputs_Impl(FMenuBuilder& InChildMenuBuilder, const FText& InMenuName,
		UDMMaterialStage* InStage, UDMMaterialStageThroughput* InThroughput, int32 InInputIndex, int32 InInputChannel,
		EDMMaterialPropertyType InSlotProperty);

	static void GenerateChangeInputMenu_PreviousStage_Output_Channels_Impl(FMenuBuilder& InChildMenuBuilder, const FText& InMenuName,
		UDMMaterialStage* InStage, UDMMaterialStageThroughput* InThroughput, int32 InInputIndex, int32 InInputChannel,
		EDMMaterialPropertyType InSlotProperty, int32 InOutputIndex);

	static void GenerateChangeInputMenu_PreviousStage_Output_Channel_Impl(FMenuBuilder& InChildMenuBuilder, const FText& InMenuName,
		UDMMaterialStage* InStage, UDMMaterialStageThroughput* InThroughput, int32 InInputIndex, int32 InInputChannel,
		EDMMaterialPropertyType InSlotProperty, int32 InOutputIndex, int32 InOutputChannel);

	static void GenerateChangeInputMenu_PreviousStage_Output_Channel_Channels_Impl(FMenuBuilder& InChildMenuBuilder, const FText& InMenuName,
		UDMMaterialStage* InStage, UDMMaterialStageThroughput* InThroughput, int32 InInputIndex, int32 InInputChannel,
		EDMMaterialPropertyType InSlotProperty, int32 InOutputIndex);

	static void GenerateChangeInputMenu_PreviousStage_Output_Impl(FMenuBuilder& InChildMenuBuilder, const FText& InMenuName,
		UDMMaterialStage* InStage, UDMMaterialStageThroughput* InThroughput, int32 InInputIndex, int32 InInputChannel,
		EDMMaterialPropertyType InSlotProperty, int32 InOutputIndex, bool bInAcceptsWholeChannel, bool bInAcceptsSubChannels);

	/** New local value */
	static void GenerateChangeInputMenu_NewLocalValues(FMenuBuilder& InChildMenuBuilder, UDMMaterialStageThroughput* InThroughput, 
		const int32 InInputIndex, const int32 InInputChannel);

	/** Global values */
	static void GenerateChangeInputMenu_GlobalValues(FMenuBuilder& InChildMenuBuilder, UDMMaterialStageThroughput* InThroughput, 
		const int32 InInputIndex, const int32 InInputChannel);

	static void GenerateChangeInputMenu_GlobalValue_Channels(FMenuBuilder& InChildMenuBuilder, UDMMaterialStageThroughput* InThroughput, 
		const int32 InInputIndex, const int32 InInputChannel, const int32 InValueIndex);

	static void GenerateChangeInputMenu_GlobalValue_Channels_Impl(FMenuBuilder& InChildMenuBuilder, const FText& InMenuName,
		UDMMaterialStage* InStage, UDMMaterialStageThroughput* InThroughput, int32 InInputIndex, int32 InInputChannel,
		int32 InValueIndex, UDMMaterialValue* InValue);

	static void GenerateChangeInputMenu_GlobalValue_Channel_Impl(FMenuBuilder& InChildMenuBuilder, const FText& InMenuName,
		UDMMaterialStage* InStage, UDMMaterialStageThroughput* InThroughput, int32 InInputIndex, int32 InInputChannel,
		int32 InValueIndex, UDMMaterialValue* InValue, int32 InOutputChannel);

	static void GenerateChangeInputMenu_GlobalValue_Value_Channels_Impl(FMenuBuilder& InChildMenuBuilder, const FText& InMenuName,
		UDMMaterialStage* InStage, UDMMaterialStageThroughput* InThroughput, int32 InInputIndex, int32 InInputChannel,
		int32 InValueIndex, UDMMaterialValue* InValue);

	static void GenerateChangeInputMenu_GlobalValue_Impl(FMenuBuilder& InChildMenuBuilder, const FText& InMenuName,
		UDMMaterialStage* InStage, UDMMaterialStageThroughput* InThroughput, int32 InInputIndex, int32 InInputChannel,
		int32 InValueIndex, UDMMaterialValue* InValue, bool bInAcceptsWholeChannel, bool bInAcceptsSubChannels);

	/** New global value */
	static void GenerateChangeInputMenu_NewGlobalValues(FMenuBuilder& InChildMenuBuilder, UDMMaterialStageThroughput* InThroughput, 
		const int32 InInputIndex, const int32 InInputChannel);

	/** Other slots */
	static void GenerateChangeInputMenu_Slots(FMenuBuilder& InChildMenuBuilder, UDMMaterialStageThroughput* InThroughput, 
		const int32 InInputIndex, const int32 InInputChannel);

	static void GenerateChangeInputMenu_Slot_Properties(FMenuBuilder& InChildMenuBuilder, UDMMaterialStageThroughput* InThroughput,
		const int32 InInputIndex, const int32 InInputChannel, UDMMaterialSlot* InSlot);

	static void GenerateChangeInputMenu_Slot_Property_Outputs(FMenuBuilder& InChildMenuBuilder, UDMMaterialStageThroughput* InThroughput,
		const int32 InInputIndex, const int32 InInputChannel, UDMMaterialSlot* InSlot, EDMMaterialPropertyType InMaterialProperty);

	static void GenerateChangeInputMenu_Slot_Properties_Impl(FMenuBuilder& InChildMenuBuilder, const FText& InMenuName,
		UDMMaterialStage* InStage, UDMMaterialStageThroughput* InThroughput, int32 InInputIndex, int32 InInputChannel,
		UDMMaterialSlot* InSlot);

	static void GenerateChangeInputMenu_Slot_Property_Outputs_Impl(FMenuBuilder& InChildMenuBuilder, const FText& InMenuName,
		UDMMaterialStage* InStage, UDMMaterialStageThroughput* InThroughput, int32 InInputIndex, int32 InInputChannel,
		UDMMaterialSlot* InSlot, EDMMaterialPropertyType InSlotProperty);

	static void GenerateChangeInputMenu_Slot_Property_Output_Channels_Impl(FMenuBuilder& InChildMenuBuilder, const FText& InMenuName,
		UDMMaterialStage* InStage, UDMMaterialStageThroughput* InThroughput, int32 InInputIndex, int32 InInputChannel,
		UDMMaterialSlot* InSlot, EDMMaterialPropertyType InSlotProperty, int32 InOutputIndex);

	static void GenerateChangeInputMenu_Slot_Property_Output_Channel_Impl(FMenuBuilder& InChildMenuBuilder, const FText& InMenuName,
		UDMMaterialStage* InStage, UDMMaterialStageThroughput* InThroughput, int32 InInputIndex, int32 InInputChannel,
		UDMMaterialSlot* InSlot, EDMMaterialPropertyType InSlotProperty, int32 InOutputIndex, int32 InOutputChannel);

	static void GenerateChangeInputMenu_Slot_Property_Output_Channel_Channels_Impl(FMenuBuilder& InChildMenuBuilder, const FText& InMenuName,
		UDMMaterialStage* InStage, UDMMaterialStageThroughput* InThroughput, int32 InInputIndex, int32 InInputChannel,
		UDMMaterialSlot* InSlot, EDMMaterialPropertyType InSlotProperty, int32 InOutputIndex);

	static void GenerateChangeInputMenu_Slot_Property_Output_Impl(FMenuBuilder& InChildMenuBuilder, const FText& InMenuName,
		UDMMaterialStage* InStage, UDMMaterialStageThroughput* InThroughput, int32 InInputIndex, int32 InInputChannel,
		UDMMaterialSlot* InSlot, EDMMaterialPropertyType InSlotProperty, int32 InOutputIndex, bool bInAcceptsWholeChannel,
		bool bInAcceptsSubChannels);

	static void GenerateChangeInputMenu_Slot_Property_Output_Channels(FMenuBuilder& InChildMenuBuilder, UDMMaterialStageThroughput* InThroughput,
		const int32 InInputIndex, const int32 InInputChannel, UDMMaterialSlot* InSlot, EDMMaterialPropertyType InMaterialProperty, 
		const int32 InOutputChannel);

	/** Expressions */
	static void GenerateChangeInputMenu_Expressions(FMenuBuilder& InChildMenuBuilder, UDMMaterialStageThroughput* InThroughput, 
		const int32 InInputIndex, const int32 InInputChannel);

	static void GenerateChangeInputMenu_Expression(FMenuBuilder& InChildSubMenuBuilder, UDMMaterialStageExpression* InExpressionCDO, 
		UDMMaterialStageThroughput* InThroughput, const int32 InInputIndex, const int32 InInputChannel);

	static void GenerateChangeInputMenu_Expression_Outputs(FMenuBuilder& InChildMenuBuilder, UDMMaterialStageThroughput* InThroughput,
		const int32 InInputIndex, const int32 InInputChannel, TSubclassOf<UDMMaterialStageExpression> InExpressionClass);

	static void GenerateChangeInputMenu_Expression_Output_Channels(FMenuBuilder& InChildMenuBuilder, UDMMaterialStageThroughput* InThroughput,
		const int32 InInputIndex, const int32 InInputChannel, TSubclassOf<UDMMaterialStageExpression> InExpressionClass, 
		const int32 InOutputChannel);

	static void GenerateChangeInputMenu_Expression_Outputs_Impl(FMenuBuilder& InChildMenuBuilder, const FText& InMenuName,
		UDMMaterialStage* InStage, UDMMaterialStageThroughput* InThroughput, int32 InInputIndex, int32 InInputChannel,
		TSubclassOf<UDMMaterialStageExpression> InExpressionClass);

	static void GenerateChangeInputMenu_Expression_Output_Channels_Impl(FMenuBuilder& InChildMenuBuilder, const FText& InMenuName,
		UDMMaterialStage* InStage, UDMMaterialStageThroughput* InThroughput, int32 InInputIndex, int32 InInputChannel,
		TSubclassOf<UDMMaterialStageExpression> InExpressionClass, int32 InOutputIndex);

	static void GenerateChangeInputMenu_Expression_Output_Channel_Impl(FMenuBuilder& InChildMenuBuilder, const FText& InMenuName,
		UDMMaterialStage* InStage, UDMMaterialStageThroughput* InThroughput, int32 InInputIndex, int32 InInputChannel,
		TSubclassOf<UDMMaterialStageExpression> InExpressionClass, int32 InOutputIndex, int32 InOutputChannel);

	static void GenerateChangeInputMenu_Expression_Output_Channel_Channels_Impl(FMenuBuilder& InChildMenuBuilder, const FText& InMenuName,
		UDMMaterialStage* InStage, UDMMaterialStageThroughput* InThroughput, int32 InInputIndex, int32 InInputChannel,
		TSubclassOf<UDMMaterialStageExpression> InExpressionClass, int32 InOutputIndex);

	static void GenerateChangeInputMenu_Expression_Output_Impl(FMenuBuilder& InChildMenuBuilder, const FText& InMenuName,
		UDMMaterialStage* InStage, UDMMaterialStageThroughput* InThroughput, int32 InInputIndex, int32 InInputChannel,
		TSubclassOf<UDMMaterialStageExpression> InExpressionClass, int32 InOutputIndex, bool bInAcceptsWholeChannel, bool bInAcceptsSubChannels);

	/** UV */
	static void GenerateChangeInputMenu_UV(FMenuBuilder& InChildMenuBuilder, UDMMaterialStageThroughput* InThroughput, const int32 InInputIndex,
		const int32 InInputChannel);

	static void GenerateChangeInputMenu_UV_Channels_Impl(FMenuBuilder& InChildMenuBuilder, const FText& InMenuName,
		UDMMaterialStage* InStage, UDMMaterialStageThroughput* InThroughput, int32 InInputIndex, int32 InInputChannel);

	static void GenerateChangeInputMenu_UV_Channel_Impl(FMenuBuilder& InChildMenuBuilder, const FText& InMenuName,
		UDMMaterialStage* InStage, UDMMaterialStageThroughput* InThroughput, int32 InInputIndex, int32 InInputChannel, int32 InOutputChannel);

	static void GenerateChangeInputMenu_UV_UV_Channels_Impl(FMenuBuilder& InChildMenuBuilder, const FText& InMenuName,
		UDMMaterialStage* InStage, UDMMaterialStageThroughput* InThroughput, int32 InInputIndex, int32 InInputChannel);

	static void GenerateChangeInputMenu_UV_Impl(FMenuBuilder& InChildMenuBuilder, const FText& InMenuName,
		UDMMaterialStage* InStage, UDMMaterialStageThroughput* InThroughput, int32 InInputIndex, int32 InInputChannel,
		bool bInAcceptsWholeChannel, bool bInAcceptsSubChannels);

	static void GenerateChangeInputMenu_UV_Channels(FMenuBuilder& InChildMenuBuilder, UDMMaterialStageThroughput* InThroughput,
		const int32 InInputIndex, const int32 InInputChannel);

	/** Gradients */
	static void GenerateChangeInputMenu_Gradients(FMenuBuilder& InChildMenuBuilder, UDMMaterialStageThroughput* InThroughput, 
		const int32 InInputIndex, const int32 InInputChannel);

	static void GenerateChangeInputMenu_Gradient(FMenuBuilder& InChildSubMenuBuilder, UDMMaterialStageGradient* InGradientCDO, 
		UDMMaterialStageThroughput* InThroughput, const int32 InInputIndex, const int32 InInputChannel);
};
