// Copyright Epic Games, Inc. All Rights Reserved.

#include "RemoteControlPropertyIdRegistry.h"

#include "IRemoteControlModule.h"
#include "Materials/MaterialInterface.h"
#include "RCVirtualProperty.h"
#include "RemoteControlPreset.h"
#include "UObject/Field.h"

#if !WITH_EDITOR
#include "Backends/CborStructDeserializerBackend.h"
#include "Backends/CborStructSerializerBackend.h"
#include "IStructDeserializerBackend.h"
#include "StructSerializer.h"
#endif

namespace RemoteControlPropertyIdUtilities
{
	bool GetObjectRef(const TSharedPtr<FRemoteControlProperty>& Field, const ERCAccess Access, FRCObjectReference& OutObjectRef)
	{
		if (!Field.IsValid())
		{
			return false;
		}
		if (UObject* FieldBoundObject = Field->GetBoundObject())
		{
			FRCObjectReference ObjectRef;
			FString ErrorText;
			if (IRemoteControlModule::Get().ResolveObjectProperty(Access, FieldBoundObject, Field->FieldPathInfo, ObjectRef, &ErrorText))
			{
				OutObjectRef = ObjectRef;
				return true;
			}
			UE_LOG(LogRemoteControl, Warning, TEXT("Could not resolve object property \"%s\" in object \"%s\": %s"), *Field->FieldName.ToString(), *FieldBoundObject->GetPathName(), *ErrorText);
		}
		return false;
	}

	bool CopyNonUClassOwnerArray(const TObjectPtr<URCVirtualPropertySelfContainer>& InVirtualPropertySelfContainer,
		const FArrayProperty* InArrayProperty,
		const TSharedPtr<FRemoteControlProperty>& InTargetRCProperty,
		FProperty* InProperty,
		FRCFieldPathInfo InSegment,
		const int32 InIndexSegment,
		const uint8* InPropertyContainer,
		const bool bCheckByteEnumComparison,
		uint8** OutPtrContainer)
	{
		FScriptArrayHelper ArrayHelper(InArrayProperty, InPropertyContainer);
		if (const TSharedPtr<FRemoteControlField> TargetRCField = StaticCastSharedPtr<FRemoteControlField>(InTargetRCProperty);
			!TargetRCField->FieldPathInfo.Segments.IsEmpty())
		{
			if (!OutPtrContainer)
            {
            	return false;
            }

			const int32 Index = InSegment.Segments[InIndexSegment].ArrayIndex;
			*OutPtrContainer = ArrayHelper.GetElementPtr(Index);
			if (InIndexSegment < InSegment.GetSegmentCount() - 2)
			{
				return false;
			}
			if (const FArrayProperty* ArrayRealProperty = CastField<FArrayProperty>(InProperty))
			{
				return InVirtualPropertySelfContainer->CopyCompleteValue(ArrayRealProperty->Inner, *OutPtrContainer, bCheckByteEnumComparison);
			}
		}
		return false;
	}
	
	bool CopyNonUClassOwnerMap(const TObjectPtr<URCVirtualPropertySelfContainer>& InVirtualPropertySelfContainer,
		const FMapProperty* InMapProperty,
		const TSharedPtr<FRemoteControlProperty>& InTargetRCProperty,
		FProperty* InProperty,
		FRCFieldPathInfo InSegment,
		const int32 InIndexSegment,
		const uint8* InPropertyContainer,
		const bool bCheckByteEnumComparison,
		uint8** OutPtrContainer)
	{
		FScriptMapHelper MapHelper(InMapProperty, InPropertyContainer);
		if (const TSharedPtr<FRemoteControlField> TargetRCField = StaticCastSharedPtr<FRemoteControlField>(InTargetRCProperty);
			!TargetRCField->FieldPathInfo.Segments.IsEmpty())
		{
			if (!OutPtrContainer)
			{
				return false;
			}

			const int32 Index = InSegment.Segments[InIndexSegment].ArrayIndex;
			*OutPtrContainer = MapHelper.GetValuePtr(Index);
			if (InIndexSegment < InSegment.GetSegmentCount() - 2)
			{
				return false;
			}
			if (const FMapProperty* MapRealProperty = CastField<FMapProperty>(InProperty))
			{
				return InVirtualPropertySelfContainer->CopyCompleteValue(MapRealProperty->ValueProp, *OutPtrContainer, bCheckByteEnumComparison);
			}
		}
		return false;
	}

	bool CopyNonUClassOwnerSet(const TObjectPtr<URCVirtualPropertySelfContainer>& InVirtualPropertySelfContainer,
		const FSetProperty* InSetProperty,
		const TSharedPtr<FRemoteControlProperty>& InTargetRCProperty,
		FProperty* InProperty,
		FRCFieldPathInfo InSegment,
		const int32 InIndexSegment,
		const uint8* InPropertyContainer,
		const bool bCheckByteEnumComparison,
		uint8** OutPtrContainer)
	{
		FScriptSetHelper SetHelper(InSetProperty, InPropertyContainer);
		if (const TSharedPtr<FRemoteControlField> TargetRCField = StaticCastSharedPtr<FRemoteControlField>(InTargetRCProperty);
			!TargetRCField->FieldPathInfo.Segments.IsEmpty())
		{
			if (!OutPtrContainer)
			{
				return false;
			}

			const int32 Index = InSegment.Segments[InIndexSegment].ArrayIndex;
			*OutPtrContainer = SetHelper.GetElementPtr(Index);
			if (InIndexSegment < InSegment.GetSegmentCount() - 2)
			{
				return false;
			}
			if (const FSetProperty* SetRealProperty = CastField<FSetProperty>(InProperty))
			{
				return InVirtualPropertySelfContainer->CopyCompleteValue(SetRealProperty->ElementProp, *OutPtrContainer, bCheckByteEnumComparison);
			}
		}
		return false;
	}
}

FRCPropertyIdWrapper::FRCPropertyIdWrapper(const TSharedRef<FRemoteControlProperty>& InRCProperty)
{
	EntityId = InRCProperty->GetId();
	
	PropertyId = InRCProperty->PropertyId;
	if (FProperty* Property = InRCProperty->GetProperty())
	{
		if (UObject* BoundObject = InRCProperty->GetBoundObject())
		{
			UpdateTypes(InRCProperty, BoundObject, Property);
		}
	}
}

const FGuid& FRCPropertyIdWrapper::GetEntityId() const
{
	return EntityId;
}

FName FRCPropertyIdWrapper::GetPropertyId() const
{
	return PropertyId;
}

void FRCPropertyIdWrapper::SetPropertyId(FName InNewPropertyId)
{
	PropertyId = InNewPropertyId;
}

const FName& FRCPropertyIdWrapper::GetSuperType() const
{
	return SuperType;
}

const FName& FRCPropertyIdWrapper::GetSubType() const
{
	return SubType;
}

const UClass* FRCPropertyIdWrapper::GetClassToCreate() const
{
	return ClassToCreate;
}

bool FRCPropertyIdWrapper::IsValid() const
{
	return EntityId.IsValid() && (!SuperType.IsNone() || !SubType.IsNone());
}

bool FRCPropertyIdWrapper::IsValidPropertyId() const
{
	return PropertyId != NAME_None;
}

uint32 GetTypeHash(const FRCPropertyIdWrapper& Wrapper)
{
	return GetTypeHash(Wrapper.EntityId);
}

bool FRCPropertyIdWrapper::operator==(const FGuid& WrappedId) const
{
	if (!IsValid())
	{
		return false;
	}
	return EntityId == WrappedId;
}

bool FRCPropertyIdWrapper::operator==(const FRCPropertyIdWrapper& Other) const
{
	if (IsValid() && Other.IsValid())
	{
		return EntityId == Other.EntityId;
	}
	return false;
}

void FRCPropertyIdWrapper::UpdateTypes(const TSharedRef<FRemoteControlProperty>& InRCProperty, UObject* InOwner, FProperty* InProperty)
{
	SuperType = InProperty->GetClass()->GetFName();
	SubType = NAME_None;
	ClassToCreate = nullptr;
	if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(InProperty))
	{
		if (EnumProperty->GetEnum())
		{
			SubType = EnumProperty->GetEnum()->GetFName();
		}
	}
	else if (const FByteProperty* ByteProperty = CastField<FByteProperty>(InProperty))
	{
		if (ByteProperty->Enum)
		{
			SuperType = NAME_EnumProperty;
			SubType = ByteProperty->Enum->GetFName();
		}
	}
	else if (const FStructProperty* StructProperty = CastField<FStructProperty>(InProperty))
	{
		if (StructProperty->Struct->GetFName() == NAME_LinearColor)
		{
			SubType = NAME_Color;
		}
		else
		{
			SubType = StructProperty->Struct->GetFName();
		}
	}
	else if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(InProperty))
	{
		// ContainerPtrToValuePtr will crash if the GetOwner<UClass> is nullptr so we check before.
		if (ObjectProperty->GetOwner<UClass>())
		{
			if (const uint8* PropertyValuePtr = ObjectProperty->ContainerPtrToValuePtr<uint8>(InOwner))
			{
				if (const TObjectPtr<UObject> Object = ObjectProperty->GetPropertyValue(PropertyValuePtr))
				{
					if (Object && Object->IsValidLowLevel())
					{
						if (Object->IsA(UMaterialInterface::StaticClass()))
						{
							SubType = UMaterialInterface::StaticClass()->GetFName();
							ClassToCreate = UMaterialInterface::StaticClass();
						}
						else
						{
							SubType = Object->GetClass()->GetFName();
							ClassToCreate = Object->GetClass();
						}
					}
				}
			}
		}
	}
	else if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(InProperty))
	{
		SuperType = ArrayProperty->Inner->GetClass()->GetFName();

		// ContainerPtrToValuePtr will crash if the GetOwner<UClass> is nullptr so we check before.
		if (ArrayProperty->GetOwner<UClass>())
		{
			if (const uint8* PropertyValuePtr = ArrayProperty->ContainerPtrToValuePtr<uint8>(InOwner))
			{
				if (const FObjectProperty* InnerObjectProperty = CastField<FObjectProperty>(ArrayProperty->Inner))
				{
					FScriptArrayHelper ArrayHelper(ArrayProperty, PropertyValuePtr);
					
					if (const TSharedRef<FRemoteControlField> TargetRCField = StaticCastSharedRef<FRemoteControlField>(InRCProperty); !TargetRCField->FieldPathInfo.Segments.IsEmpty())
					{
						const int32 ArrayIndex = TargetRCField->FieldPathInfo.Segments[0].ArrayIndex;
						if (ArrayIndex != INDEX_NONE)
						{
							const uint8* ObjPtrContainer = ArrayHelper.GetRawPtr(ArrayIndex);
							if (const UObject* CurrentObject = InnerObjectProperty->GetObjectPropertyValue(ObjPtrContainer))
							{
								if (CurrentObject->IsA(UMaterialInterface::StaticClass()))
								{
									SubType = UMaterialInterface::StaticClass()->GetFName();
									ClassToCreate = UMaterialInterface::StaticClass();
								}
								else
								{
									SubType = CurrentObject->GetClass()->GetFName();
									ClassToCreate = CurrentObject->GetClass();
								}
							}
						}
					}
				}
			}
		}
		else if (const FStructProperty* InnerStructProperty = CastField<FStructProperty>(ArrayProperty->Inner))
		{
			if (InnerStructProperty->Struct->GetFName() == NAME_LinearColor)
			{
				SubType = NAME_Color;
			}
			else
			{
				SubType = InnerStructProperty->Struct->GetFName();
			}
		}
	}
}

void URemoteControlPropertyIdRegistry::Initialize()
{
	URemoteControlPreset* SourcePreset = GetSourcePreset();
	if (!SourcePreset)
	{
		return;
	}

	IdentifiedFields.Reset();
	for (TWeakPtr<FRemoteControlEntity> RCEntity : SourcePreset->GetExposedEntities())
	{
		if (const TSharedPtr<FRemoteControlField> RCField = StaticCastSharedPtr<FRemoteControlField>(RCEntity.Pin()))
		{
			AddIdentifiedField(RCField.ToSharedRef());
		}
	}
}

URemoteControlPreset* URemoteControlPropertyIdRegistry::GetSourcePreset() const
{
	return GetTypedOuter<URemoteControlPreset>();
}

void URemoteControlPropertyIdRegistry::PerformChainReaction(const FRemoteControlPropertyIdArgs& InArgs)
{
	URemoteControlPreset* SourcePreset = GetSourcePreset();
	if (!IsValid(SourcePreset))
	{
		return;
	}

	TSet<FGuid> TargetProperties;

	Algo::TransformIf(IdentifiedFields, TargetProperties,
		[InArgs](const FRCPropertyIdWrapper& Wrapper)
		{
			return Wrapper.IsValid()
			&& Wrapper.GetPropertyId() == InArgs.PropertyId
			&& Wrapper.GetSuperType() == InArgs.SuperType
			&& Wrapper.GetSubType() == InArgs.SubType;
		},
		[](const FRCPropertyIdWrapper& Wrapper)
		{
			return Wrapper.GetEntityId();
		}
		);

	for (const FGuid& TargetProperty : TargetProperties)
	{
		if (TSharedPtr<FRemoteControlProperty> TargetRCProperty = SourcePreset->GetExposedEntity<FRemoteControlProperty>(TargetProperty).Pin())
		{
			if (UObject* BoundObject = TargetRCProperty->GetBoundObject())
			{
				bool bCopyComplete = false;

				if (FProperty* Property = TargetRCProperty->GetProperty())
				{
#if WITH_EDITOR
					BoundObject->PreEditChange(Property);
					
					BoundObject->Modify();
#endif // WITH_EDITOR
					// Note : For all other object types except materials.
					if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
					{
						if (uint8* PropertyValuePtr = ObjectProperty->ContainerPtrToValuePtr<uint8>(BoundObject))
						{
							UObject* CurrentObject = ObjectProperty->GetObjectPropertyValue(PropertyValuePtr);
							if (CurrentObject && CurrentObject->IsA(InArgs.SourceClass))
							{
								ObjectProperty->SetObjectPropertyValue(PropertyValuePtr, InArgs.SourceObject);
								bCopyComplete = true;
							}
						}
					}
					// Note : Specialization for Materials.
					else if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
					{
						if (ArrayProperty->GetOwner<UClass>())
						{
							if (uint8* PropertyValuePtr = ArrayProperty->ContainerPtrToValuePtr<uint8>(BoundObject))
							{
								if (FObjectProperty* InnerObjectProperty = CastField<FObjectProperty>(ArrayProperty->Inner))
								{
									FScriptArrayHelper ArrayHelper(ArrayProperty, PropertyValuePtr);
									if (TSharedPtr<FRemoteControlField> TargetRCField = StaticCastSharedPtr<FRemoteControlField>(TargetRCProperty); !TargetRCField->FieldPathInfo.Segments.IsEmpty())
									{
										const int32 MaterialIndex = TargetRCField->FieldPathInfo.Segments[0].ArrayIndex;
										uint8* ObjPtrContainer = ArrayHelper.GetRawPtr(MaterialIndex);
										UObject* CurrentObject = InnerObjectProperty->GetObjectPropertyValue(ObjPtrContainer);
										if (CurrentObject && CurrentObject->IsA(InArgs.SourceClass))
										{
											InnerObjectProperty->SetObjectPropertyValue(ObjPtrContainer, InArgs.SourceObject);
											bCopyComplete = true;
										}
									}
								}
							}
						}
					}
					// Note : For primitive types not in container and that has a valid UClass Owner.
					// ContainerPtrToValuePtr will crash if the GetOwner<UClass> is nullptr so we check before.
					if (!bCopyComplete && Property->GetOwner<UClass>() != nullptr &&
						!Property->IsA<FArrayProperty>() && !Property->IsA<FSetProperty>() && !Property->IsA<FMapProperty>())
					{
						if (uint8* PropertyValuePtr = Property->ContainerPtrToValuePtr<uint8>(BoundObject))
						{
							if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
							{
								// FLinearColor are treated as FColor to avoid creating 2 different widget for them and avoid confusion
								// Instead of the normal CopyCompleteValue we use this for FLinearColor
								if (StructProperty->Struct->GetFName() == NAME_LinearColor)
								{
									FColor ColorValue;
									InArgs.VirtualProperty->GetValueColor(ColorValue);
									const FLinearColor RealValue(ColorValue);
									Property->CopyCompleteValue(PropertyValuePtr, &RealValue);
									bCopyComplete = true;
								}
							}
							// We do this to avoid copying it above and here
							if (bCopyComplete == false)
							{
								bCopyComplete = InArgs.VirtualProperty->CopyCompleteValue(Property, PropertyValuePtr, true);
							}
						}
					}
					//Note : For all the other cases, Container and UStruct Owner.
					else if (!bCopyComplete)
					{
#if !WITH_EDITOR
						StructMemoryContainer = nullptr;
#endif // !WITH_EDITOR
						bCopyComplete = CopyNonUClassOwnerProperty(InArgs, TargetRCProperty, Property, BoundObject);
					}
					if (bCopyComplete)
					{
#if WITH_EDITOR
						TargetRCProperty->FieldPathInfo.Resolve(BoundObject);
						FPropertyChangedEvent PropertyEvent = TargetRCProperty->FieldPathInfo.ToPropertyChangedEvent(EPropertyChangeType::ValueSet);
						FEditPropertyChain EditPropertyChain;
						TargetRCProperty->FieldPathInfo.ToEditPropertyChain(EditPropertyChain);
						if (EditPropertyChain.IsEmpty())
						{
							BoundObject->PostEditChangeProperty(PropertyEvent);
						}
						else
						{
							FPropertyChangedChainEvent ChainEvent(EditPropertyChain, PropertyEvent);
							TArray<TMap<FString, int32>> ArrayIndicesPerObject;
							{
								TMap<FString, int32> ArrayIndices;
								ArrayIndices.Reserve(TargetRCProperty->FieldPathInfo.Segments.Num());
								for (const FRCFieldPathSegment& Segment : TargetRCProperty->FieldPathInfo.Segments)
								{
									ArrayIndices.Add(Segment.Name.ToString(), Segment.ArrayIndex);
								}
								ArrayIndicesPerObject.Add(MoveTemp(ArrayIndices));
							}
							ChainEvent.ObjectIteratorIndex = 0;
							ChainEvent.SetArrayIndexPerObject(ArrayIndicesPerObject);
							BoundObject->PostEditChangeChainProperty(ChainEvent);
						}
						
#else // NOTE: During runtime use Serialization API to update the property properly.
						TArray<uint8> Buffer;
						FMemoryWriter Writer(Buffer);
						FCborStructSerializerBackend WriterBackend(Writer, EStructSerializerBackendFlags::Default);
						FStructSerializerPolicies Policies;
						Policies.MapSerialization = EStructSerializerMapPolicies::Array;

						// We do this at runtime because otherwise Property Owned by UStruct won't update.
						// So during the copy we take the StructContainer and use that to serialize the property.
						if (StructMemoryContainer != nullptr)
						{
							FStructSerializer::SerializeElement(StructMemoryContainer, Property, INDEX_NONE, WriterBackend, Policies);
						}
						else
						{
							FStructSerializer::SerializeElement(BoundObject, Property, INDEX_NONE, WriterBackend, Policies);
						}
						// Deserialization
						FMemoryReader Reader(Buffer);
						FCborStructDeserializerBackend ReaderBackend(Reader);
						FRCObjectReference TargetObjectRef;
						if (!RemoteControlPropertyIdUtilities::GetObjectRef(TargetRCProperty, ERCAccess::WRITE_ACCESS, TargetObjectRef))
						{
							continue;
						}
						if (!IRemoteControlModule::Get().SetObjectProperties(TargetObjectRef, ReaderBackend, ERCPayloadType::Cbor, Buffer))
						{
							continue;
						}
#endif // WITH_EDITOR
					}
				}
			}
		}
	}
}

void URemoteControlPropertyIdRegistry::AddIdentifiedField(const TSharedRef<FRemoteControlField>& InFieldToIdentify)
{
	if (InFieldToIdentify->FieldType == EExposedFieldType::Property)
	{
		if (const TSharedPtr<FRemoteControlProperty> RCProperty = StaticCastSharedRef<FRemoteControlProperty>(InFieldToIdentify))
		{
			FRCPropertyIdWrapper Wrapper{ RCProperty.ToSharedRef()};
			if (Wrapper.IsValid())
			{
				IdentifiedFields.Add(MoveTemp(Wrapper));
			}
		}
	}
}

bool URemoteControlPropertyIdRegistry::IsEmpty() const
{
	return IdentifiedFields.IsEmpty();
}

void URemoteControlPropertyIdRegistry::UpdateIdentifiedField(const TSharedRef<FRemoteControlField>& InFieldToIdentify)
{
	if (InFieldToIdentify->FieldType == EExposedFieldType::Property)
	{
		const uint32 Hash = GetTypeHash(InFieldToIdentify->GetId());
		if (FRCPropertyIdWrapper* Wrapper = IdentifiedFields.FindByHash(Hash, InFieldToIdentify->GetId()))
		{
			Wrapper->SetPropertyId(InFieldToIdentify->PropertyId);
			OnPropertyIdUpdated().Broadcast();
		}
	}
}

void URemoteControlPropertyIdRegistry::RemoveIdentifiedField(const FGuid& InEntityId)
{
	const uint32 Hash = GetTypeHash(InEntityId);

	if (IdentifiedFields.ContainsByHash(Hash, InEntityId))
	{
		IdentifiedFields.RemoveByHash(Hash, InEntityId);
	}
}

const UClass* URemoteControlPropertyIdRegistry::GetClassByEntityId(const FGuid& InEntityId) const
{
	const uint32 Hash = GetTypeHash(InEntityId);

	if (const FRCPropertyIdWrapper* Wrapper = IdentifiedFields.FindByHash(Hash, InEntityId))
	{
		return Wrapper->GetClassToCreate();
	}
	return nullptr;
}

TSet<FName> URemoteControlPropertyIdRegistry::GetFieldIdsNameList()
{
	TSet<FName> OutIds;
	for (FRCPropertyIdWrapper Field : IdentifiedFields)
	{
		if (Field.IsValidPropertyId())
		{
			OutIds.Add(Field.GetPropertyId());
		}
	}
	return OutIds;
}

TSet<FGuid> URemoteControlPropertyIdRegistry::GetEntityIdsForPropertyId(const FName& InPropertyId) const
{
	TSet<FGuid> OutEntityIds;
	if (InPropertyId == NAME_None)
	{
		return OutEntityIds;
	}
	for (FRCPropertyIdWrapper Wrappers : IdentifiedFields)
	{
		if (Wrappers.GetPropertyId() == InPropertyId)
		{
			OutEntityIds.Add(Wrappers.GetEntityId());
		}
	}
	return OutEntityIds;
}

TSet<FGuid> URemoteControlPropertyIdRegistry::GetEntityIdsList()
{
	TSet<FGuid> OutIds;
	for (FRCPropertyIdWrapper Field : IdentifiedFields)
	{
		if (Field.IsValidPropertyId())
		{
			OutIds.Add(Field.GetEntityId());
		}
	}
	return OutIds;
}

bool URemoteControlPropertyIdRegistry::CopyNonUClassOwnerProperty(const FRemoteControlPropertyIdArgs& InArgs, TSharedPtr<FRemoteControlProperty> TargetRCProperty, FProperty* InProperty, UObject* BoundObject)
{
	bool bCheckByteEnumComparison = false;
	if (InArgs.VirtualProperty->GetProperty())
	{
		bCheckByteEnumComparison = InArgs.VirtualProperty->GetProperty()->IsA<FEnumProperty>();
	}
	TargetRCProperty->FieldPathInfo.Resolve(BoundObject);
	return TryCopyNonUClassOwnerProperty(InArgs.VirtualProperty, TargetRCProperty, InProperty, BoundObject, TargetRCProperty->FieldPathInfo, 0, nullptr, bCheckByteEnumComparison);
}

bool URemoteControlPropertyIdRegistry::TryCopyNonUClassOwnerProperty(const TObjectPtr<URCVirtualPropertySelfContainer>& InVirtualPropertySelfContainer, const TSharedPtr<FRemoteControlProperty>& InTargetRCProperty, FProperty* InProperty, UObject* InBoundObject, FRCFieldPathInfo InSegment, const int32 InIndexSegment, uint8* InPropertyContainer, const bool bCheckByteEnumComparison)
{
	if (InIndexSegment >= InSegment.GetSegmentCount())
	{
		return false;
	}

	if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(InSegment.Segments[InIndexSegment].ResolvedData.Field))
	{
		if (ArrayProperty->GetOwner<UClass>())
		{
			if (const uint8* PropertyValuePtr = ArrayProperty->ContainerPtrToValuePtr<uint8>(InBoundObject))
			{
				uint8* PtrContainer = nullptr;
				if (RemoteControlPropertyIdUtilities::CopyNonUClassOwnerArray(InVirtualPropertySelfContainer, ArrayProperty, InTargetRCProperty, InProperty, InSegment, InIndexSegment, PropertyValuePtr, bCheckByteEnumComparison, &PtrContainer))
				{
					return true;
				}
				return TryCopyNonUClassOwnerProperty(InVirtualPropertySelfContainer, InTargetRCProperty, InProperty, InBoundObject, InSegment, InIndexSegment + 1, PtrContainer, bCheckByteEnumComparison);
			}
		}
		else if (InPropertyContainer)
		{
			uint8* PtrContainer = nullptr;
			if (RemoteControlPropertyIdUtilities::CopyNonUClassOwnerArray(InVirtualPropertySelfContainer, ArrayProperty, InTargetRCProperty, InProperty, InSegment, InIndexSegment, InPropertyContainer, bCheckByteEnumComparison, &PtrContainer))
			{
				return true;
			}
			return TryCopyNonUClassOwnerProperty(InVirtualPropertySelfContainer, InTargetRCProperty, InProperty, InBoundObject, InSegment, InIndexSegment + 1, PtrContainer, bCheckByteEnumComparison);
		}
	}
	else if (const FSetProperty* SetProperty = CastField<FSetProperty>(InSegment.Segments[InIndexSegment].ResolvedData.Field))
	{
		if (SetProperty->GetOwner<UClass>())
		{
			if (const uint8* PropertyValuePtr = SetProperty->ContainerPtrToValuePtr<uint8>(InBoundObject))
			{
				uint8* PtrContainer = nullptr;
				if (RemoteControlPropertyIdUtilities::CopyNonUClassOwnerSet(InVirtualPropertySelfContainer, SetProperty, InTargetRCProperty, InProperty, InSegment, InIndexSegment, PropertyValuePtr, bCheckByteEnumComparison, &PtrContainer))
				{
					return true;
				}
				return TryCopyNonUClassOwnerProperty(InVirtualPropertySelfContainer, InTargetRCProperty, InProperty, InBoundObject, InSegment, InIndexSegment + 1, PtrContainer, bCheckByteEnumComparison);
			}
		}
		else if (InPropertyContainer)
		{
			uint8* PtrContainer = nullptr;
			if (RemoteControlPropertyIdUtilities::CopyNonUClassOwnerSet(InVirtualPropertySelfContainer, SetProperty, InTargetRCProperty, InProperty, InSegment, InIndexSegment, InPropertyContainer, bCheckByteEnumComparison, &PtrContainer))
			{
				return true;
			}
			return TryCopyNonUClassOwnerProperty(InVirtualPropertySelfContainer, InTargetRCProperty, InProperty, InBoundObject, InSegment, InIndexSegment + 1, PtrContainer, bCheckByteEnumComparison);
		}
	}
	else if (const FMapProperty* MapProperty = CastField<FMapProperty>(InSegment.Segments[InIndexSegment].ResolvedData.Field))
	{
		if (MapProperty->GetOwner<UClass>())
		{
			if (const uint8* PropertyValuePtr = MapProperty->ContainerPtrToValuePtr<uint8>(InBoundObject))
			{
				uint8* PtrContainer = nullptr;
				if (RemoteControlPropertyIdUtilities::CopyNonUClassOwnerMap(InVirtualPropertySelfContainer, MapProperty, InTargetRCProperty, InProperty, InSegment, InIndexSegment, PropertyValuePtr, bCheckByteEnumComparison, &PtrContainer))
				{
					return true;
				}
				return TryCopyNonUClassOwnerProperty(InVirtualPropertySelfContainer, InTargetRCProperty, InProperty, InBoundObject, InSegment, InIndexSegment + 1, PtrContainer, bCheckByteEnumComparison);
			}
		}
		else if (InPropertyContainer)
		{
			uint8* PtrContainer = nullptr;
			if (RemoteControlPropertyIdUtilities::CopyNonUClassOwnerMap(InVirtualPropertySelfContainer, MapProperty, InTargetRCProperty, InProperty, InSegment, InIndexSegment, InPropertyContainer, bCheckByteEnumComparison, &PtrContainer))
			{
				return true;
			}
			return TryCopyNonUClassOwnerProperty(InVirtualPropertySelfContainer, InTargetRCProperty, InProperty, InBoundObject, InSegment, InIndexSegment + 1, PtrContainer, bCheckByteEnumComparison);
		}
	}
	else if (const FStructProperty* StructProperty = CastField<FStructProperty>(InSegment.Segments[InIndexSegment].ResolvedData.Field))
	{
		uint8* InnerStructPtrContainer;
		if (InPropertyContainer != nullptr)
		{
			InnerStructPtrContainer = StructProperty->ContainerPtrToValuePtr<uint8>(InPropertyContainer);
		}
		else
		{
			InnerStructPtrContainer = StructProperty->ContainerPtrToValuePtr<uint8>(InBoundObject);
		}
		if (uint8* PtrContainer = InProperty->ContainerPtrToValuePtr<uint8>(InnerStructPtrContainer))
		{
			if (InIndexSegment < InSegment.GetSegmentCount() - 2)
			{
				return TryCopyNonUClassOwnerProperty(InVirtualPropertySelfContainer, InTargetRCProperty, InProperty, InBoundObject, InSegment, InIndexSegment + 1, InnerStructPtrContainer, bCheckByteEnumComparison);
			}

			// FLinearColor are treated as FColor to avoid creating 2 different widget for them and avoid confusion
			// Instead of the normal CopyCompleteValue we use this for FLinearColor
			if (const FStructProperty* StructRealProperty = CastField<FStructProperty>(InProperty))
			{
#if !WITH_EDITOR
				StructMemoryContainer = InnerStructPtrContainer;
#endif // !WITH_EDITOR
				if (StructRealProperty->Struct->GetFName() == NAME_LinearColor)
				{
					FColor ColorValue;
					InVirtualPropertySelfContainer->GetValueColor(ColorValue);
					const FLinearColor RealValue(ColorValue);
					InProperty->CopyCompleteValue(PtrContainer, &RealValue);
					return true;
				}
			}

			if (InProperty->IsA<FArrayProperty>() || InProperty->IsA<FSetProperty>() || InProperty->IsA<FMapProperty>())
			{
				return TryCopyNonUClassOwnerProperty(InVirtualPropertySelfContainer, InTargetRCProperty, InProperty, InBoundObject, InSegment, InIndexSegment + 1, InnerStructPtrContainer, bCheckByteEnumComparison);
			}
			return InVirtualPropertySelfContainer->CopyCompleteValue(InProperty, PtrContainer, bCheckByteEnumComparison);
		}
	}

	return false;
}
