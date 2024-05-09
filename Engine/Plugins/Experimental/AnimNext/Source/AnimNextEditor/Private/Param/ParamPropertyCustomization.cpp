// Copyright Epic Games, Inc. All Rights Reserved.

#include "ParamPropertyCustomization.h"

#include "DetailWidgetRow.h"
#include "EdGraphSchema_K2.h"
#include "EditorUtils.h"
#include "PropertyHandle.h"
#include "SParameterPickerCombo.h"
#include "Modules/ModuleManager.h"
#include "Param/AnimNextEditorParam.h"
#include "Param/AnimNextParam.h"
#include "Param/AnimNextParamInstanceIdentifier.h"
#include "Param/ParamCompatibility.h"
#include "Param/ParamUtils.h"
#include "HAL/PlatformApplicationMisc.h"
#include "RigVMCore/RigVMRegistry.h"

#define LOCTEXT_NAMESPACE "ParamPropertyCustomization"

namespace UE::AnimNext::Editor
{

void FParamPropertyCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& InCustomizationUtils)
{
	auto GetMetadataProperty = [](const FProperty* InProperty)
	{
		if (const FProperty* OuterProperty = InProperty->GetOwner<FProperty>())
		{
			if (OuterProperty->IsA<FArrayProperty>()
				|| OuterProperty->IsA<FSetProperty>()
				|| OuterProperty->IsA<FMapProperty>())
			{
				return OuterProperty;
			}
		}

		return InProperty;
	};

	PropertyHandle = InPropertyHandle;
	ParamStruct = CastFieldChecked<FStructProperty>(PropertyHandle->GetProperty())->Struct;

	const FProperty* Property = GetMetadataProperty(InPropertyHandle->GetProperty());
	const FString ParamTypeString = Property->GetMetaData("AllowedParamType");
	FAnimNextParamType FilterType = FAnimNextParamType::FromString(ParamTypeString);

	FParameterPickerArgs PickerArgs;
	PickerArgs.bMultiSelect = false;
	PickerArgs.OnFilterParameterType = FOnFilterParameterType::CreateLambda([FilterType](const FAnimNextParamType& InParamType)
	{
		if(FilterType.IsValid())
		{
			if(!FParamUtils::GetCompatibility(FilterType, InParamType).IsCompatible())
			{
				return EFilterParameterResult::Exclude;
			}
		}

		if(InParamType.IsValid())
		{
			const FRigVMTemplateArgumentType RigVMType = InParamType.ToRigVMTemplateArgument();
			if(!RigVMType.IsValid() || FRigVMRegistry::Get().GetTypeIndex(RigVMType) == INDEX_NONE)
			{
				return EFilterParameterResult::Exclude;
			}
		}

		return EFilterParameterResult::Include;
	});
	PickerArgs.NewParameterType = FilterType;
	PickerArgs.OnInstanceIdChanged = FOnInstanceIdChanged::CreateSPLambda(this, [this](const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InInstanceId)
	{
		if(PropertyHandle)
		{
			PropertyHandle->NotifyPreChange();
			if(ParamStruct == FAnimNextEditorParam::StaticStruct())
			{
				PropertyHandle->EnumerateRawData([&InInstanceId](void* RawData, const int32 DataIndex, const int32 NumDatas)
				{
					FAnimNextEditorParam& Param = *static_cast<FAnimNextEditorParam*>(RawData);
					Param.InstanceId = InInstanceId;
					return true;
				});
			}
			else if(ParamStruct == FAnimNextParam::StaticStruct())
			{
				FAnimNextParam ScheduleParam(NAME_None, FAnimNextParamType(), InInstanceId);
				PropertyHandle->EnumerateRawData([&ScheduleParam](void* RawData, const int32 DataIndex, const int32 NumDatas)
				{
					FAnimNextParam& Param = *static_cast<FAnimNextParam*>(RawData);
					Param.InstanceId = ScheduleParam.InstanceId;
					return true;
				});
			}
			PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
			PropertyHandle->NotifyFinishedChangingProperties();
			Refresh();
		}
	});

	PickerArgs.OnParameterPicked = FOnParameterPicked::CreateSPLambda(this, [this](const FParameterBindingReference& InParameterBinding)
	{
		if(PropertyHandle)
		{
			PropertyHandle->NotifyPreChange();

			if(ParamStruct == FAnimNextEditorParam::StaticStruct())
			{
				FAnimNextEditorParam ParamValue(*InParameterBinding.Parameter.ToString(), InParameterBinding.Type, InParameterBinding.InstanceId);
				PropertyHandle->EnumerateRawData([&ParamValue](void* RawData, const int32 DataIndex, const int32 NumDatas)
				{
					FAnimNextEditorParam& Param = *static_cast<FAnimNextEditorParam*>(RawData);
					Param = ParamValue;
					return true;
				});
				CachedParam = ParamValue;
			}
			else if(ParamStruct == FAnimNextParam::StaticStruct())
			{
				FAnimNextParam ParamValue(*InParameterBinding.Parameter.ToString(), InParameterBinding.Type, InParameterBinding.InstanceId);
				PropertyHandle->EnumerateRawData([&ParamValue](void* RawData, const int32 DataIndex, const int32 NumDatas)
				{
					FAnimNextParam& Param = *static_cast<FAnimNextParam*>(RawData);
					Param = ParamValue;
					return true;
				});
				CachedParam = FAnimNextEditorParam(ParamValue);
			}
			PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
			PropertyHandle->NotifyFinishedChangingProperties();
		}
	});

	if(ParamStruct == FAnimNextEditorParam::StaticStruct())
	{
		TOptional<TInstancedStruct<FAnimNextParamInstanceIdentifier>> CommonInstanceId;
		PropertyHandle->EnumerateConstRawData([&CommonInstanceId](const void* RawData, const int32 DataIndex, const int32 NumDatas)
		{
			const FAnimNextEditorParam& Param = *static_cast<const FAnimNextEditorParam*>(RawData);
			if(!CommonInstanceId.IsSet())
			{
				CommonInstanceId = Param.InstanceId;
			}
			else if(CommonInstanceId.GetValue() != Param.InstanceId)
			{
				// No common scope, so use a null instance
				CommonInstanceId = TInstancedStruct<FAnimNextParamInstanceIdentifier>();
				return false;
			}
			return true;
		});

		PickerArgs.InstanceId = CommonInstanceId.IsSet() ? CommonInstanceId.GetValue() : TInstancedStruct<FAnimNextParamInstanceIdentifier>();
	}
	else if(ParamStruct == FAnimNextParam::StaticStruct())
	{
		struct FInstanceId
		{
			FInstanceId(FName InInstanceId, const UScriptStruct* InInstanceIdType)
				: InstanceId(InInstanceId)
				, InstanceIdType(InInstanceIdType)
			{}

			FName InstanceId;
			const UScriptStruct* InstanceIdType;
		};
		
		TOptional<FInstanceId> CommonInstanceId;
		PropertyHandle->EnumerateConstRawData([&CommonInstanceId](const void* RawData, const int32 DataIndex, const int32 NumDatas)
		{
			const FAnimNextParam& Param = *static_cast<const FAnimNextParam*>(RawData);
			if(!CommonInstanceId.IsSet())
			{
				if(!Param.InstanceId.IsNone() && Param.InstanceIdType != nullptr)
				{
					CommonInstanceId = FInstanceId(Param.InstanceId, Param.InstanceIdType);
				}
			}
			else if(CommonInstanceId.GetValue().InstanceId != Param.InstanceId && CommonInstanceId.GetValue().InstanceIdType != Param.InstanceIdType)
			{
				// No common instance ID, so use NAME_None
				CommonInstanceId = FInstanceId(NAME_None, FAnimNextParamInstanceIdentifier::StaticStruct());
				return false;
			}
			return true;
		});

		if(CommonInstanceId.IsSet())
		{
			PickerArgs.InstanceId.InitializeAsScriptStruct(CommonInstanceId.GetValue().InstanceIdType);
			PickerArgs.InstanceId.GetMutable().FromName(CommonInstanceId.GetValue().InstanceId);
		}
		else
		{
			PickerArgs.InstanceId = TInstancedStruct<FAnimNextParamInstanceIdentifier>();
		}
	}

	FUIAction CopyAction, PasteAction;
	PropertyHandle->CreateDefaultPropertyCopyPasteActions(CopyAction, PasteAction);

	DefaultCopyAction = CopyAction.ExecuteAction;
	CopyAction.ExecuteAction = FExecuteAction::CreateSP(this, &FParamPropertyCustomization::HandleCopy);

	InHeaderRow
	.CopyAction(CopyAction)
	.PasteAction(PasteAction)
	.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SParameterPickerCombo)
			.PickerArgs(PickerArgs)
			.OnGetParameterName_Lambda([this]()
			{
				return CachedParam.Name;
			})
			.OnGetParameterType_Lambda([this]()
			{
				return CachedParam.Type;
			})
			.OnGetParameterInstanceId_Lambda([this]()
			{
				return CachedParam.InstanceId;
			})
		]
	];

	Refresh();
}

void FParamPropertyCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}

void FParamPropertyCustomization::Refresh()
{
	CachedParam = FAnimNextEditorParam();

	if(ParamStruct == FAnimNextEditorParam::StaticStruct())
	{
		PropertyHandle->EnumerateConstRawData([this](const void* RawData, const int32 DataIndex, const int32 NumDatas)
		{
			if(NumDatas == 1 && RawData != nullptr)
			{
				CachedParam = *static_cast<const FAnimNextEditorParam*>(RawData);
			}
			return false;
		});
	}
	else if(ParamStruct == FAnimNextParam::StaticStruct())
	{
		PropertyHandle->EnumerateConstRawData([this](const void* RawData, const int32 DataIndex, const int32 NumDatas)
		{
			if(NumDatas == 1 && RawData != nullptr)
			{
				CachedParam = FAnimNextEditorParam(*static_cast<const FAnimNextParam*>(RawData));
			}
			return false;
		});
	}
}

void FParamPropertyCustomization::HandleCopy()
{
	DefaultCopyAction.Execute();

	// We always copy-paste as a FAnimNextEditorParam
	if(ParamStruct == FAnimNextParam::StaticStruct())
	{
		// Grab the clipboard text
		FString ImportClipboardString;
		FPlatformApplicationMisc::ClipboardPaste(ImportClipboardString);

		// Import as FAnimNextParam
		FAnimNextParam ImportedParam;
		FAnimNextParam::StaticStruct()->ImportText(*ImportClipboardString, &ImportedParam, nullptr, PPF_None, nullptr, FAnimNextParam::StaticStruct()->GetName());

		// Re-export as FAnimNextEditorParam
		FAnimNextEditorParam ParamToExport(ImportedParam);
		FString ExportClipboardString;
		FAnimNextEditorParam::StaticStruct()->ExportText(ExportClipboardString, &ParamToExport, nullptr, nullptr, PPF_None, nullptr);
		FPlatformApplicationMisc::ClipboardCopy(*ExportClipboardString);
	}
}

}

#undef LOCTEXT_NAMESPACE