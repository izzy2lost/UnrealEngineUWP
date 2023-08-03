// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/PropertyChainUtils.h"

#include "Misc/ScopeExit.h"
#include "Serialization/ArchiveSerializedPropertyChain.h"
#include "UObject/UnrealType.h"

namespace UE::ConcertSyncCore::PropertyChain
{
	namespace Private
	{
		EBreakBehavior VisitPropertyRecursive(FArchiveSerializedPropertyChain& Chain, FProperty& Property, TFunctionRef<EBreakBehavior(const FArchiveSerializedPropertyChain& Chain, const FProperty&LeafProperty)> ProcessProperty);
		
		EBreakBehavior VisitStructPropertyRecursive(
			FArchiveSerializedPropertyChain& Chain,
			FStructProperty& StructProperty,
			TFunctionRef<EBreakBehavior(const FArchiveSerializedPropertyChain& Chain, const FProperty&LeafProperty)> ProcessProperty
			)
		{
			// If the struct is in a container, then the container should be pushed, not the inner property.
			FProperty& PropertyToPush = IsInnerContainerProperty(StructProperty)
				? *StructProperty.GetOwner<FProperty>()
				: StructProperty;
			Chain.PushProperty(&PropertyToPush, PropertyToPush.IsEditorOnlyProperty());
			ON_SCOPE_EXIT { Chain.PopProperty(&PropertyToPush, PropertyToPush.IsEditorOnlyProperty()); };
			
			for (TFieldIterator<FProperty> FieldIt(StructProperty.Struct); FieldIt; ++FieldIt)
			{
				if (VisitPropertyRecursive(Chain, **FieldIt, ProcessProperty) == EBreakBehavior::Break)
				{
					return EBreakBehavior::Break;
				}
			}
			
			return EBreakBehavior::Continue;
		}
		
		EBreakBehavior VisitPropertyRecursive(
			FArchiveSerializedPropertyChain& Chain,
			FProperty& Property,
			TFunctionRef<EBreakBehavior(const FArchiveSerializedPropertyChain& Chain, const FProperty&LeafProperty)> ProcessProperty
			)
		{
			if (!IsReplicatableProperty(Property))
			{
				return EBreakBehavior::Continue;
			}

			if (ProcessProperty(Chain, Property) == EBreakBehavior::Break)
			{
				return EBreakBehavior::Break;
			}
			
			if (FStructProperty* StructProperty = CastField<FStructProperty>(&Property))
			{
				// Handle FConcertPropertyChain::InternalContainerPropertyValueName case
				// If the struct defines a custom serialize function, it does not make sense to list any uproperties we find.
				// The serialize function can (and often will) skip certain properties so exposing them to a user for selection makes no sense.
				// If this struct property is supposed to be replicated, then it is all the Serialize function serializes or nothing.
				return IsNativeStructProperty(*StructProperty)
					? EBreakBehavior::Continue
					: VisitStructPropertyRecursive(Chain, *StructProperty, ProcessProperty);
			}

			auto HandleContainer = [&Chain, &Property, &ProcessProperty](FProperty& Inner)
			{
				// Handle FConcertPropertyChain::InternalContainerPropertyValueName case
				if (IsPrimitiveProperty(Inner)
					|| IsNativeStructProperty(Inner))
				{
					Chain.PushProperty(&Property, Property.IsEditorOnlyProperty());
					ON_SCOPE_EXIT{ Chain.PopProperty(&Property, Property.IsEditorOnlyProperty()); };
					return ProcessProperty(Chain, Inner);
				}
				
				FStructProperty* InnerStructProperty = CastField<FStructProperty>(&Inner);
				return InnerStructProperty && IsReplicatableProperty(Inner)
					? VisitStructPropertyRecursive(Chain, *InnerStructProperty, ProcessProperty)
					: EBreakBehavior::Continue;
			};

			if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(&Property))
			{
				return HandleContainer(*ArrayProperty->Inner);
			}

			if (FSetProperty* SetProperty = CastField<FSetProperty>(&Property))
			{
				
				return HandleContainer(*SetProperty->ElementProp);
			}

			if (FMapProperty* MapProperty = CastField<FMapProperty>(&Property))
			{
				return IsReplicatableProperty(*MapProperty->KeyProp)
					? HandleContainer(*MapProperty->ValueProp)
					: EBreakBehavior::Continue;
			}
			
			return EBreakBehavior::Continue;
		}
	}
	
	void ForEachReplicatableProperty(
		UStruct& Class,
		TFunctionRef<EBreakBehavior(const FArchiveSerializedPropertyChain& Chain, const FProperty& LeafProperty)> ProcessProperty)
	{
		FArchiveSerializedPropertyChain Chain;
		for (TFieldIterator<FProperty> FieldIt(&Class); FieldIt; ++FieldIt)
		{
			if (Private::VisitPropertyRecursive(Chain, **FieldIt, ProcessProperty) == EBreakBehavior::Break)
			{
				break;
			}
		}
	}

	void ForEachReplicatableConcertProperty(
		UStruct& Class,
		TFunctionRef<EBreakBehavior(FConcertPropertyChain&& PropertyChain)> ProcessProperty)
	{
		ForEachReplicatableProperty(Class, [&ProcessProperty](const FArchiveSerializedPropertyChain& Chain, const FProperty& LeafProperty)
		{
			return ProcessProperty(FConcertPropertyChain(&Chain, LeafProperty));
		});
	}

	void BulkConstructConcertChainsFromPaths(UStruct& Class, uint32 NumPaths, TFunctionRef<bool(const FArchiveSerializedPropertyChain& Chain, const FProperty& LeafProperty)> MatchesPath)
	{
		if (NumPaths == 0)
		{
			return;
		}
		
		ForEachReplicatableProperty(Class, [&NumPaths, &MatchesPath](const FArchiveSerializedPropertyChain& Chain, const FProperty& LeafProperty) mutable
		{
			if (MatchesPath(Chain, LeafProperty))
			{
				--NumPaths;
				return NumPaths == 0
					? EBreakBehavior::Break
					: EBreakBehavior::Continue;
			}
			return EBreakBehavior::Continue;
		});
	}
	
	bool DoPathAndChainsMatch(const TArray<FName>& Path, const FArchiveSerializedPropertyChain& Chain, const FProperty& LeafProperty)
	{
		if (Chain.GetNumProperties() + 1 != Path.Num())
		{
			return false;
		}
		
		for (int32 i = 0; i < Path.Num() - 1; ++i)
		{
			if (Path[i] != Chain.GetPropertyFromRoot(0)->GetFName())
			{
				return false;
			}
		}

		return Path[Path.Num() - 1] == LeafProperty.GetFName();
	}
	
	bool IsReplicatableProperty(const FProperty& LeafProperty)
	{
		return !LeafProperty.HasAnyPropertyFlags(
			// Replicating delegates makes no sense
			CPF_BlueprintAssignable
			// It does not make sense to serialize the reference of an instanced subobject. Instead the subobject should be added to the list of replicated properties.
			| CPF_InstancedReference /* for object ptrs */ | CPF_ContainsInstancedReference /* for containers of object ptrs */
			);
	}

	bool IsInnerContainerProperty(const FProperty& Property)
	{
		const FProperty* ParentProperty = Property.GetOwner<FProperty>();
		const bool bIsInnerContainerProperty = ParentProperty
			&& (ParentProperty->IsA(FArrayProperty::StaticClass()) || ParentProperty->IsA(FSetProperty::StaticClass()) || ParentProperty->IsA(FMapProperty::StaticClass()));
		return bIsInnerContainerProperty;
	}
	
	bool IsPropertyEligibleForMarkingAsInternal(const FProperty& Property)
	{
		return IsInnerContainerProperty(Property)
			&& (IsPrimitiveProperty(Property) || IsNativeStructProperty(Property));
	}

	bool IsPrimitiveProperty(const FProperty& Property)
	{
		return Property.IsA(FNumericProperty::StaticClass())
			|| Property.IsA(FBoolProperty::StaticClass())
			|| Property.IsA(FEnumProperty::StaticClass());
	}
	
	bool IsNativeStructProperty(const FProperty& Property)
	{
		return Property.IsA(FStructProperty::StaticClass())
			&& CastField<FStructProperty>(&Property)->Struct->GetCppStructOps()->HasSerializer();
	}
}
