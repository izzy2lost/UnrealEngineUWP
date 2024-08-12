// Copyright Epic Games, Inc. All Rights Reserved.

#include "Entries/AnimNextVariableEntry.h"

#include "AnimNextRigVMAssetEditorData.h"
#include "UncookedOnlyUtils.h"
#include "Module/AnimNextModule.h"
#include "Variables/AnimNextUniversalObjectLocatorBindingData.h"

const FLazyName IAnimNextRigVMVariableInterface::ValueName("Value");

UAnimNextVariableEntry::UAnimNextVariableEntry()
{
	// TEMP: This should be selectable
	Binding.BindingData.InitializeAs<FAnimNextUniversalObjectLocatorBindingData>();
}

FAnimNextParamType UAnimNextVariableEntry::GetExportType() const
{
	return GetType();
}

FName UAnimNextVariableEntry::GetExportName() const
{
	return GetVariableName();
}

EAnimNextExportAccessSpecifier UAnimNextVariableEntry::GetExportAccessSpecifier() const
{
	return Access;
}

void UAnimNextVariableEntry::SetExportAccessSpecifier(EAnimNextExportAccessSpecifier InAccessSpecifier, bool bSetupUndoRedo)
{
	if(bSetupUndoRedo)
	{
		Modify();
	};

	Access = InAccessSpecifier;

	BroadcastModified(EAnimNextEditorDataNotifType::EntryAccessSpecifierChanged);
}

FAnimNextParamType UAnimNextVariableEntry::GetType() const
{
	return Type;
}

FName UAnimNextVariableEntry::GetEntryName() const
{
	return ParameterName;
}

bool UAnimNextVariableEntry::SetType(const FAnimNextParamType& InType, bool bSetupUndoRedo)
{
	if(bSetupUndoRedo)
	{
		Modify();
	}

	Type = InType;

	DefaultValue.Reset();
	DefaultValue.AddProperties({ FPropertyBagPropertyDesc(IAnimNextRigVMVariableInterface::ValueName, Type.GetContainerType(), Type.GetValueType(), Type.GetValueTypeObject()) });

	BroadcastModified(EAnimNextEditorDataNotifType::VariableTypeChanged);

	return true;
}

bool UAnimNextVariableEntry::SetDefaultValue(const FString& InDefaultValue, bool bSetupUndoRedo)
{
	if(bSetupUndoRedo)
	{
		Modify();
	}
	
	return DefaultValue.SetValueSerializedString(IAnimNextRigVMVariableInterface::ValueName, InDefaultValue) == EPropertyBagResult::Success;
}

FName UAnimNextVariableEntry::GetVariableName() const
{
	return ParameterName;
}

void UAnimNextVariableEntry::SetVariableName(FName InName, bool bSetupUndoRedo)
{
	SetEntryName(InName, bSetupUndoRedo);
}

const FInstancedPropertyBag& UAnimNextVariableEntry::GetPropertyBag() const
{
	return DefaultValue;
}

const FAnimNextVariableBinding& UAnimNextVariableEntry::GetBinding() const
{
	return Binding;
}

void UAnimNextVariableEntry::SetEntryName(FName InName, bool bSetupUndoRedo)
{
	if(bSetupUndoRedo)
	{
		Modify();
	}

	ParameterName = InName;
	BroadcastModified(EAnimNextEditorDataNotifType::EntryRenamed);
}

FText UAnimNextVariableEntry::GetDisplayName() const
{
	return FText::FromName(ParameterName);
}

FText UAnimNextVariableEntry::GetDisplayNameTooltip() const
{
	return FText::FromString(Comment);
}

void UAnimNextVariableEntry::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar.UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);
}

void UAnimNextVariableEntry::PostLoad()
{
	Super::PostLoad();
	if(GetLinkerCustomVersion(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::AnimNextModuleRefactor)
	{
		// Add a property for this type
		DefaultValue.Reset();
		DefaultValue.AddProperties({ FPropertyBagPropertyDesc(IAnimNextRigVMVariableInterface::ValueName, Type.GetContainerType(), Type.GetValueType(), Type.GetValueTypeObject()) });

		// Copy any default value from the module's defaults, if found
		UAnimNextRigVMAsset* Asset = GetTypedOuter<UAnimNextRigVMAsset>();
		FString FullName = Asset->GetPathName() + TEXT(":") + GetVariableName().ToString();
		const FPropertyBagPropertyDesc* OldDesc = Asset->VariableDefaults.GetPropertyBagStruct() ? Asset->VariableDefaults.GetPropertyBagStruct()->FindPropertyDescByName(*FullName) : nullptr;
		const FPropertyBagPropertyDesc* NewDesc = DefaultValue.GetPropertyBagStruct() ? DefaultValue.GetPropertyBagStruct()->FindPropertyDescByName(IAnimNextRigVMVariableInterface::ValueName) : nullptr;
		if(OldDesc && NewDesc)
		{
			const void* OldValue = OldDesc->CachedProperty->ContainerPtrToValuePtr<void>(Asset->VariableDefaults.GetValue().GetMemory());
			void* NewValue = NewDesc->CachedProperty->ContainerPtrToValuePtr<void>(DefaultValue.GetMutableValue().GetMemory());
			NewDesc->CachedProperty->CopyCompleteValue(NewValue, OldValue);
		}
	}
}