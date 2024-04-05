// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeEditorPropertyBindings.h"
#include "StateTreePropertyBindingCompiler.h"
#include "Misc/EnumerateRange.h"
#include "PropertyPathHelpers.h"
#include "StateTreeNodeBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeEditorPropertyBindings)

#define LOCTEXT_NAMESPACE "StateTreeEditor"

UStateTreeEditorPropertyBindingsOwner::UStateTreeEditorPropertyBindingsOwner(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

//////////////////////////////////////////////////////////////////////////

void FStateTreeEditorPropertyBindings::AddPropertyBinding(const FStateTreePropertyPath& SourcePath, const FStateTreePropertyPath& TargetPath)
{
	RemovePropertyBindings(TargetPath);
	PropertyBindings.Add(FStateTreePropertyPathBinding(SourcePath, TargetPath));
}

void FStateTreeEditorPropertyBindings::RemovePropertyBindings(const FStateTreePropertyPath& TargetPath)
{
	PropertyBindings.RemoveAll([&TargetPath](const FStateTreePropertyPathBinding& Binding)
		{
			return Binding.GetTargetPath() == TargetPath;
		});
}

void FStateTreeEditorPropertyBindings::CopyBindings(const FGuid FromStructID, const FGuid ToStructID)
{
	// Copy all bindings that target "FromStructID" and retarget them to "ToStructID".
	TArray<FStateTreePropertyPathBinding> NewBindings;
	for (const FStateTreePropertyPathBinding& Binding : PropertyBindings)
	{
		if (Binding.GetTargetPath().GetStructID() == FromStructID)
		{
			NewBindings.Emplace(Binding.GetSourcePath(), FStateTreePropertyPath(ToStructID, Binding.GetTargetPath().GetSegments()));
		}
	}

	PropertyBindings.Append(NewBindings);
}

bool FStateTreeEditorPropertyBindings::HasPropertyBinding(const FStateTreePropertyPath& TargetPath) const
{
	return PropertyBindings.ContainsByPredicate([&TargetPath](const FStateTreePropertyPathBinding& Binding)
		{
			return Binding.GetTargetPath() == TargetPath;
		});
}

const FStateTreePropertyPath* FStateTreeEditorPropertyBindings::GetPropertyBindingSource(const FStateTreePropertyPath& TargetPath) const
{
	const FStateTreePropertyPathBinding* Binding = PropertyBindings.FindByPredicate([&TargetPath](const FStateTreePropertyPathBinding& Binding)
		{
			return Binding.GetTargetPath() == TargetPath;
		});
	return Binding ? &Binding->GetSourcePath() : nullptr;
}

void FStateTreeEditorPropertyBindings::GetPropertyBindingsFor(const FGuid StructID, TArray<FStateTreePropertyPathBinding>& OutBindings) const
{
	OutBindings = PropertyBindings.FilterByPredicate([StructID](const FStateTreePropertyPathBinding& Binding)
		{
			return Binding.GetSourcePath().GetStructID().IsValid()
				&& Binding.GetTargetPath().GetStructID() == StructID;
		});
}

void FStateTreeEditorPropertyBindings::RemoveUnusedBindings(const TMap<FGuid, const FStateTreeDataView>& ValidStructs)
{
	PropertyBindings.RemoveAll([ValidStructs](FStateTreePropertyPathBinding& Binding)
		{
			// Remove binding if it's target struct has been removed
			if (!ValidStructs.Contains(Binding.GetTargetPath().GetStructID()))
			{
				// Remove
				return true;
			}

			// Target path should always have at least one segment (copy bind directly on a target struct/object). 
			if (Binding.GetTargetPath().IsPathEmpty())
			{
				return true;
			}

			// Remove binding if path containing instanced indirections (e.g. instance struct or object) cannot be resolved.
			// TODO: Try to use core redirect to find new name.
			{
				const FStateTreeDataView* SourceValue = ValidStructs.Find(Binding.GetSourcePath().GetStructID());
				if (SourceValue && SourceValue->IsValid())
				{
					FString Error;
					TArray<FStateTreePropertyPathIndirection> Indirections;
					if (!Binding.GetSourcePath().ResolveIndirectionsWithValue(*SourceValue, Indirections, &Error))
					{
						UE_LOG(LogStateTree, Verbose, TEXT("Removing binding to %s because binding source path cannot be resolved: %s"),
							*Binding.GetSourcePath().ToString(), *Error); // Error contains the target path.

						// Remove
						return true;
					}
				}
			}
			
			{
				const FStateTreeDataView TargetValue = ValidStructs.FindChecked(Binding.GetTargetPath().GetStructID());
				FString Error;
				TArray<FStateTreePropertyPathIndirection> Indirections;
				if (!Binding.GetTargetPath().ResolveIndirectionsWithValue(TargetValue, Indirections, &Error))
				{
					UE_LOG(LogStateTree, Verbose, TEXT("Removing binding to %s because binding target path cannot be resolved: %s"),
						*Binding.GetSourcePath().ToString(), *Error); // Error contains the target path.

					// Remove
					return true;
				}
			}

			return false;
		});
}

bool FStateTreeEditorPropertyBindings::ContainsAnyStruct(const TSet<const UStruct*>& Structs)
{
	auto PathContainsStruct = [&Structs](const FStateTreePropertyPath& PropertyPath)
	{
		for (const FStateTreePropertyPathSegment& Segment : PropertyPath.GetSegments())
		{
			if (Structs.Contains(Segment.GetInstanceStruct()))
			{
				return true;
			}
		}
		return false;
	};
	
	for (FStateTreePropertyPathBinding& PropertyPathBinding : PropertyBindings)
	{
		if (PathContainsStruct(PropertyPathBinding.GetSourcePath()))
		{
			return true;
		}
		if (PathContainsStruct(PropertyPathBinding.GetTargetPath()))
		{
			return true;
		}
	}
	
	return false;
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
void FStateTreeEditorPropertyBindings::AddPropertyBinding(const FStateTreeEditorPropertyPath& SourcePath, const FStateTreeEditorPropertyPath& TargetPath)
{
	AddPropertyBinding(UE::StateTree::Private::ConvertEditorPath(SourcePath), UE::StateTree::Private::ConvertEditorPath(TargetPath));
}

void FStateTreeEditorPropertyBindings::RemovePropertyBindings(const FStateTreeEditorPropertyPath& TargetPath)
{
	RemovePropertyBindings(UE::StateTree::Private::ConvertEditorPath(TargetPath));
}

bool FStateTreeEditorPropertyBindings::HasPropertyBinding(const FStateTreeEditorPropertyPath& TargetPath) const
{
	return HasPropertyBinding(UE::StateTree::Private::ConvertEditorPath(TargetPath));
}

const FStateTreeEditorPropertyPath* FStateTreeEditorPropertyBindings::GetPropertyBindingSource(const FStateTreeEditorPropertyPath& TargetPath) const
{
	static FStateTreeEditorPropertyPath Dummy;
	const FStateTreePropertyPath* SourcePath = GetPropertyBindingSource(UE::StateTree::Private::ConvertEditorPath(TargetPath));
	if (SourcePath != nullptr)
	{
		Dummy = UE::StateTree::Private::ConvertEditorPath(*SourcePath);
		return &Dummy;
	}
	return nullptr;
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

void FStateTreeEditorPropertyBindings::RemoveUnusedBindings(const TMap<FGuid, const UStruct*>& ValidStructs)
{
	PropertyBindings.RemoveAll([ValidStructs](const FStateTreePropertyPathBinding& Binding)
		{
			// Remove binding if it's target struct has been removed
			if (!ValidStructs.Contains(Binding.GetTargetPath().GetStructID()))
			{
				return true;
			}

			// Target path should always have at least one segment (copy bind directly on a target struct/object). 
			if (Binding.GetTargetPath().IsPathEmpty())
			{
				return true;
			}
		
			// Remove binding if path containing instanced indirections (e.g. instance struct or object) cannot be resolved.
			// TODO: Try to use core redirect to find new name.
			const UStruct* Struct = ValidStructs.FindChecked(Binding.GetTargetPath().GetStructID());
			TArray<FStateTreePropertyPathIndirection> Indirections;
			if (!Binding.GetTargetPath().ResolveIndirections(Struct, Indirections))
			{
				// Remove
				return true;
			}

			return false;
		});
}


//////////////////////////////////////////////////////////////////////////

FStateTreeBindingLookup::FStateTreeBindingLookup(const IStateTreeEditorPropertyBindingsOwner* InBindingOwner)
	: BindingOwner(InBindingOwner)
{
}

const FStateTreePropertyPath* FStateTreeBindingLookup::GetPropertyBindingSource(const FStateTreePropertyPath& InTargetPath) const
{
	check(BindingOwner);
	const FStateTreeEditorPropertyBindings* EditorBindings = BindingOwner->GetPropertyEditorBindings();
	check(EditorBindings);

	return EditorBindings->GetPropertyBindingSource(InTargetPath);
}

FText FStateTreeBindingLookup::GetPropertyPathDisplayName(const FStateTreePropertyPath& InPath, EStateTreeNodeFormatting Formatting) const
{
	check(BindingOwner);
	const FStateTreeEditorPropertyBindings* EditorBindings = BindingOwner->GetPropertyEditorBindings();
	check(EditorBindings);

	FString Result;

	FStateTreeBindableStructDesc Struct;
	if (BindingOwner->GetStructByID(InPath.GetStructID(), Struct))
	{
		Result = Struct.Name.ToString();
	}

	if (!InPath.IsPathEmpty())
	{
		Result += TEXT(".") + InPath.ToString();
	}

	return FText::FromString(Result);
}

FText FStateTreeBindingLookup::GetBindingSourceDisplayName(const FStateTreePropertyPath& InTargetPath, EStateTreeNodeFormatting Formatting) const
{
	check(BindingOwner);
	const FStateTreeEditorPropertyBindings* EditorBindings = BindingOwner->GetPropertyEditorBindings();
	check(EditorBindings);

	// Check if the target property is bound, if so, return binding description.
	if (const FStateTreePropertyPath* SourcePath = GetPropertyBindingSource(InTargetPath))
	{
		return GetPropertyPathDisplayName(*SourcePath, Formatting);
	}

	// Check if it's bound to context data.
	const UStruct* TargetStruct = nullptr;
	const FProperty* TargetProperty = nullptr;
	EStateTreePropertyUsage Usage = EStateTreePropertyUsage::Invalid;
	
	FStateTreeBindableStructDesc TargetStructDesc;
	if (BindingOwner->GetStructByID(InTargetPath.GetStructID(), TargetStructDesc))
	{
		TArray<FStateTreePropertyPathIndirection> Indirection;
		if (InTargetPath.ResolveIndirections(TargetStructDesc.Struct, Indirection)
			&& Indirection.Num() > 0)
		{
			const FStateTreePropertyPathIndirection& Leaf = Indirection.Last(); 
			TargetProperty = Leaf.GetProperty();
			if (TargetProperty)
			{
				Usage = UE::StateTree::GetUsageFromMetaData(TargetProperty);
			}
			if (const FStructProperty* StructProperty = CastField<FStructProperty>(TargetProperty))
			{
				TargetStruct = StructProperty->Struct;
			}
			if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(TargetProperty))
			{
				TargetStruct = ObjectProperty->PropertyClass;
			}
		}
	}

	if (Usage == EStateTreePropertyUsage::Context)
	{
		if (TargetStruct)
		{
			const FStateTreeBindableStructDesc Desc = BindingOwner->FindContextData(TargetStruct, TargetProperty->GetName());
			if (Desc.IsValid())
			{
				// Connected
				return FText::FromName(Desc.Name);
			}
		}
		return LOCTEXT("Unlinked", "???");
	}

	// Not a binding nor context data.
	return FText::GetEmpty();
}

const FProperty* FStateTreeBindingLookup::GetPropertyPathLeafProperty(const FStateTreePropertyPath& InPath) const
{
	check(BindingOwner);
	const FStateTreeEditorPropertyBindings* EditorBindings = BindingOwner->GetPropertyEditorBindings();
	check(EditorBindings);

	const FProperty* Result = nullptr;
	FStateTreeBindableStructDesc Struct;
	if (BindingOwner->GetStructByID(InPath.GetStructID(), Struct))
	{
		TArray<FStateTreePropertyPathIndirection> Indirection;
		if (InPath.ResolveIndirections(Struct.Struct, Indirection) && Indirection.Num() > 0)
		{
			return Indirection.Last().GetProperty();
		}
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE