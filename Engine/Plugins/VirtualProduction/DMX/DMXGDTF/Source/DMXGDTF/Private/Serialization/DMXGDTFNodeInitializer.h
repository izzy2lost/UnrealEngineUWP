// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Algo/Count.h"
#include "Algo/Transform.h"
#include "DMXGDTFLog.h"
#include "GDTF/DMXGDTFFixtureType.h"
#include "GDTF/DMXModes/DMXGDTFDMXValue.h"
#include "GDTF/Geometries/DMXGDTFAddress.h"
#include "Internationalization/Regex.h"
#include "Math/Vector2D.h"
#include "Misc/Char.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/Class.h"
#include "XmlNode.h"
#include <type_traits>

namespace UE::DMX::GDTF
{
	/**
	 * Initializer for FDMXGDTF nodes.
	 *
	 * To parse non-trivial types GetAttribute accepts transforms and predicates.
	 */
	template<typename NodeType>
	class FDMXGDTFNodeInitializer
		: FNoncopyable
	{
	public:
		FDMXGDTFNodeInitializer(TSharedRef<NodeType> InUserObject, const FXmlNode& InXmlNode UE_LIFETIMEBOUND)
			: UserObject(InUserObject)
			, XmlNode(InXmlNode)
		{
			checkf(XmlNode.GetTag() == InUserObject->GetXmlTag(), TEXT("Tag mismatch when trying to initialize GDTF node initializer. Expected tag '%s' but got tag '%s'."), InUserObject->GetXmlTag(), *XmlNode.GetTag());
		}
		
		/** Initializes an attribute */
		template <typename AttributeType>
		const FDMXGDTFNodeInitializer& GetAttribute(const TCHAR* InAttribute, AttributeType& OutValue) const
		{
			const FString StringValue = XmlNode.GetAttribute(InAttribute);
			if constexpr (std::is_same_v<AttributeType, FString> || std::is_same_v<AttributeType, FName>)
			{
				OutValue = *StringValue;
			}
			else if constexpr (std::is_arithmetic_v<AttributeType>)
			{
				const bool bSuccess = LexTryParseString(OutValue, *StringValue);
				if (!StringValue.IsEmpty() && !bSuccess)
				{
					UE_LOG(LogDMXGDTF, Warning, TEXT("Failed to parse numerical value from XML attribute '%s'. String was '%s'."), InAttribute, *StringValue);
				}
			}
			else if constexpr (std::is_enum_v<AttributeType> && std::is_invocable_v<decltype(&FDMXGDTFNodeInitializer::ParseUEnum<AttributeType>), FDMXGDTFNodeInitializer, const FString&, AttributeType&>)
			{
				AttributeType EnumValue;
				if (ParseUEnum<AttributeType>(StringValue, EnumValue))
				{
					OutValue = EnumValue;
				}
			}
			else if constexpr (std::is_same_v<AttributeType, FVector2D>)
			{
				OutValue = ParseVector2D(StringValue);
			}
			else if constexpr (std::is_same_v<AttributeType, FVector>)
			{
				OutValue = ParseVector(StringValue);
			}
			else if constexpr (std::is_same_v<AttributeType, FTransform>)
			{
				OutValue = ParseTransform(StringValue);
			}
			else if constexpr (std::is_same_v<AttributeType, FGuid>)
			{
				OutValue = FGuid(StringValue);
			}
			else if constexpr (std::is_same_v<AttributeType, FDMXGDTFAddress>)
			{
				OutValue = ParseDMXAddress(StringValue);
			}
			else if constexpr (std::is_same_v<AttributeType, FDMXGDTFDMXValue>)
			{
				OutValue.Value = StringValue;
			}
			else
			{
				[] <bool SupportedType = false>()
				{
					static_assert(SupportedType, "Unsupported attribute type.");
				}();
			}

			return *this;
		}

		/** Initializes an attribute, using a static for transformation. */
		template <typename AttributeType, typename UserObject, typename TransformType, typename... Args>
		const FDMXGDTFNodeInitializer& GetAttribute(const TCHAR* InAttribute, AttributeType& OutValue, TransformType Transform, Args...args) const
		{
			const FString StringValue = XmlNode.GetAttribute(InAttribute);
			if (StringValue.IsEmpty())
			{
				return *this;
			}

			OutValue = (*Transform)(*StringValue, args...);

			return *this;
		}

		/** Initializes an attribute, using a member function for transformation. */
		template <typename AttributeType, typename UserObject, typename TransformType, typename... Args>
		const FDMXGDTFNodeInitializer& GetAttribute(const TCHAR* InAttribute, AttributeType& OutValue, const UserObject* Object, TransformType Transform, Args...args) const
		{
			static_assert(std::is_member_function_pointer_v<TransformType>, "TransformType must be a member function pointer.");
			check(Object);

			const FString StringValue = XmlNode.GetAttribute(InAttribute);
			if (StringValue.IsEmpty())
			{
				return *this;
			}

			OutValue = (Object->*Transform)(*StringValue, args...);

			return *this;
		}

		/** Initializes an attribute, using a predicate for transformation. */
		template <typename AttributeType, typename PredicateType>
		const FDMXGDTFNodeInitializer& GetAttribute(const TCHAR* InAttribute, AttributeType& OutValue, PredicateType Predicate) const
		{
			const FString StringValue = XmlNode.GetAttribute(InAttribute);
			if (!StringValue.IsEmpty())
			{
				OutValue = Predicate(*StringValue);
			}
			return *this;
		}

		/** Creates a collection of children nodes, whereas a collection is an xml node that contains these children. */
		template <typename ChildType, typename = std::enable_if_t<std::is_base_of_v<FDMXGDTFNode, ChildType>>>
		const FDMXGDTFNodeInitializer& CreateChildCollection(const TCHAR* InCollectTag, const TCHAR* InChildTag, TArray<TSharedPtr<ChildType>>& OutChildren) const
		{
			if (const FXmlNode* CollectNode = XmlNode.FindChildNode(InCollectTag))
			{
				Algo::TransformIf(CollectNode->GetChildrenNodes(), OutChildren,
					[InChildTag](const FXmlNode* ChildXmlNode)
					{
						return ChildXmlNode && ChildXmlNode->GetTag() == InChildTag;
					},
					[this](const FXmlNode* ChildXmlNode)
					{
						return ConstructChild<ChildType>(ChildXmlNode);
					});
			}
		
			return *this;
		}

		/** Creates child nodes for matching tag in OutChildren array. */
		template <typename ChildType, typename = std::enable_if_t<std::is_base_of_v<FDMXGDTFNode, ChildType>>>
		const FDMXGDTFNodeInitializer& CreateChildren(const TCHAR* InChildTag, TArray<TSharedPtr<ChildType>>& OutChildren) const
		{
			Algo::TransformIf(XmlNode.GetChildrenNodes(), OutChildren,
				[InChildTag](const FXmlNode* ChildXmlNode)
				{
					return ChildXmlNode && ChildXmlNode->GetTag() == InChildTag;
				},
				[this](const FXmlNode* ChildXmlNode)
				{
					return ConstructChild<ChildType>(ChildXmlNode);
				});

			return *this;
		}
	
		/** 
		 * Creates a single child node for matching tag in OutChild. 
		 * Logs a warning if more than one child with this tag are found.
		 */
		template <typename ChildType, typename = std::enable_if_t<std::is_base_of_v<FDMXGDTFNode, ChildType>>>
		const FDMXGDTFNodeInitializer& CreateOptionalChild(const TCHAR* InChildTag, TSharedPtr<ChildType>& OutChild) const
		{
			OutChild = nullptr;

			const FXmlNode* ChildXmlNode = XmlNode.FindChildNode(InChildTag);
			if (ChildXmlNode)
			{
				OutChild = ConstructChild<ChildType>(ChildXmlNode);
			}

			// Log if not unique
			const int32 NumChildren = Algo::CountIf(XmlNode.GetChildrenNodes(),
				[InChildTag](const FXmlNode* ChildXmlNode)
				{
					return ChildXmlNode->GetTag() == InChildTag;
				});
			if (NumChildren > 1)
			{
				UE_LOG(LogDMXGDTF, Warning, TEXT("Trying to parse unique child node '%s', but found %i child nodes with this tag '%s'."), InChildTag, NumChildren, InChildTag);
			}

			return *this;
		}

		/** 
		 * Creates a single child node for matching tag in OutChild. Creates the child even if the tag does not exist.
		 * Logs a warning if none or more than one child with this tag are found.
		 */
		template <typename ChildType, typename = std::enable_if_t<std::is_base_of_v<FDMXGDTFNode, ChildType>>>
		const FDMXGDTFNodeInitializer& CreateRequiredChild(const TCHAR* InChildTag, TSharedPtr<ChildType>& OutChild) const
		{
			// Try to create a child from the tag
			CreateOptionalChild(InChildTag, OutChild);

			// Create and log if not constructed from tag
			if (!OutChild.IsValid())
			{
				OutChild = ConstructChild<ChildType>(nullptr);

				UE_LOG(LogDMXGDTF, Warning, TEXT("Failed to parse non-optional child node '%s' in '%s'."), InChildTag, UserObject->GetXmlTag());
			}

			return *this;
		}

	private:
		/** Constructs a node */
		template <typename ChildType>
		TSharedRef<ChildType> ConstructChild(const FXmlNode* InXmlNode) const
		{
			const TSharedRef<ChildType> NewNode = MakeShared<ChildType>(UserObject);
		
			if constexpr (std::is_same_v<NodeType, FDMXGDTFFixtureType>)
			{
				// Refer to self when setting the fixture type of the fixture type
				NewNode->WeakFixtureType = UserObject;
			}
			else
			{
				// Refer to parents fixture type
				NewNode->WeakFixtureType = UserObject->GetFixtureType();
			}
			checkf(NewNode->GetFixtureType().IsValid(), TEXT("Failed to construct node. The fixture type is not valid."));

			if (InXmlNode)
			{
				NewNode->Initialize(*InXmlNode);
			}

			return NewNode;
		}

		/** Parses a GDTF string as Enum */
		template <typename EnumType>
		bool ParseUEnum(const FString& GDTFString, EnumType& OutEnum) const
		{
			const UEnum* UEnumObject = StaticEnum<EnumType>();
			int64 Index = UEnumObject->GetValueByName(*GDTFString);
			if (Index != INDEX_NONE)
			{
				OutEnum = static_cast<EnumType>(Index);
				return true;
			}

			return false;
		}

		/** Parses a GDTF string as vector2 */
		FVector2D ParseVector2D(const FString& GDTFString) const
		{
			if (GDTFString.IsEmpty())
			{
				return FVector2D::ZeroVector;
			}

			TArray<FString> Substrings;
			GDTFString.ParseIntoArray(Substrings, TEXT(","));

			if (Substrings.Num() != 2)
			{
				UE_LOG(LogDMXGDTF, Warning, TEXT("Cannot parse GDTF vector 2D. Expected none or two components, but got %i."), Substrings.Num());
				return FVector2D::ZeroVector;
			}

			FVector2D Result;
			bool bSuccess = LexTryParseString(Result.X, *Substrings[0]);
			bSuccess |= LexTryParseString(Result.Y, *Substrings[1]);
			if (!bSuccess)
			{
				UE_LOG(LogDMXGDTF, Warning, TEXT("Cannot parse GDTF vector 2D. Failed to parse %s."), *GDTFString);
				return FVector2D::ZeroVector;
			}

			return Result;
		}

		/** Parses a GDTF string as vector3 */
		FVector ParseVector(const FString& GDTFString) const
		{
			if (GDTFString.IsEmpty())
			{
				return FVector::ZeroVector;
			}

			// Could be a color or a vector. Vectors are wrapped with curly brackets, otherwise they're formated the same.
			const FString VectorString = GDTFString.Replace(TEXT("{"), TEXT("")).Replace(TEXT("}"), TEXT(""));

			TArray<FString> Substrings;
			VectorString.ParseIntoArray(Substrings, TEXT(","));

			if (Substrings.Num() != 3)
			{
				UE_LOG(LogDMXGDTF, Warning, TEXT("Cannot parse GDTF vector. Expected none or three components, but got %i."), Substrings.Num());
				return FVector::ZeroVector;
			}

			FVector Result;
			bool bSuccess = LexTryParseString(Result.X, *Substrings[0]);
			bSuccess |= LexTryParseString(Result.Y, *Substrings[1]);
			bSuccess |= LexTryParseString(Result.Z, *Substrings[1]);
			if (!bSuccess)
			{
				UE_LOG(LogDMXGDTF, Warning, TEXT("Cannot parse GDTF vector 3D. Failed to parse %s."), *GDTFString);
				return FVector::ZeroVector;
			}

			return Result;
		}

		/** Parses a GDTF string as transform */
		FTransform ParseTransform(const FString& GDTFString) const
		{
			if (GDTFString.IsEmpty())
			{
				return FTransform::Identity;
			}

			// Note 3x3 and 4x4 matrices are supported

			// Parse rows
			TArray<FString> RowStrings;

			const FRegexPattern CurlyBracketRegex(TEXT("\\{([^}]+)\\}"), ERegexPatternFlags::CaseInsensitive);
			FRegexMatcher CurlyBracketMatcher(CurlyBracketRegex, GDTFString);
			while (CurlyBracketMatcher.FindNext())
			{
				RowStrings.Add(CurlyBracketMatcher.GetCaptureGroup(1));
			}

			if (RowStrings.Num() < 3)
			{
				UE_LOG(LogDMXGDTF, Warning, TEXT("Cannot parse 3x3 or 4x4 matrix from GDTF string %s."), *GDTFString);
				return FTransform::Identity;
			}

			// Parse each component
			auto GetComponentsFromStringLambda([](const FString& String)
				{
					TArray<FString> Substrings;
					String.ParseIntoArray(Substrings, TEXT(","));

					TArray<float> Result;
					Result.AddZeroed(Substrings.Num());
					for (int32 StringIndex = 0; StringIndex < Substrings.Num(); StringIndex++)
					{
						float Value = 0.f;
						if (LexTryParseString(Result[StringIndex], *Substrings[StringIndex]))
						{
							// Convert from mm to cm
							Result[StringIndex] = Value / 10.f;
						}
					}
					return Result;
				});
			const TArray<float> U = GetComponentsFromStringLambda(RowStrings[0]);
			const TArray<float> V = GetComponentsFromStringLambda(RowStrings[1]);
			const TArray<float> W = GetComponentsFromStringLambda(RowStrings[2]);
			const TArray<float> O = RowStrings.IsValidIndex(3) ? GetComponentsFromStringLambda(RowStrings[3]) : TArray<float>({ 0.f, 0.f, 0.f });

			// Only 3x3 or 4x4 is supported
			const uint32 NumRows = RowStrings.Num();
			const bool bValid = (U.Num() == NumRows && V.Num() == NumRows && W.Num() == NumRows && O.Num() == NumRows);
			if (!bValid)
			{
				UE_LOG(LogDMXGDTF, Warning, TEXT("Cannot parse 3x3 or 4x4 matrix from GDTF string %s."), *GDTFString);
				return FTransform::Identity;
			}

			// Create the matrix and resulting transform
			FMatrix Matrix;
			Matrix.M[0][0] = U[0];
			Matrix.M[0][1] = U[1];
			Matrix.M[0][2] = U[2];
			Matrix.M[0][3] = U.IsValidIndex(3) ? U[3] : 0.f;

			Matrix.M[1][0] = V[0];
			Matrix.M[1][1] = V[1];
			Matrix.M[1][2] = V[2];
			Matrix.M[1][3] = V.IsValidIndex(3) ? U[3] : 0.f;

			Matrix.M[2][0] = W[0];
			Matrix.M[2][1] = W[0];
			Matrix.M[2][2] = W[0];
			Matrix.M[2][3] = W.IsValidIndex(3) ? W[3] : 0.f;

			Matrix.M[3][0] = O[0];
			Matrix.M[3][1] = -O[1]; // From GDTF's right hand to UE's left hand coordinate system
			Matrix.M[3][2] = O[2];
			Matrix.M[3][3] = O.IsValidIndex(3) ? O[3] : 1.f;

			FTransform Transform = FTransform(Matrix);

			// Prevent the convertion to cm from affecting scale
			Transform.SetScale3D(Transform.GetScale3D() * 10.0);

			// GDTFs are facing down on the Z-Axis, but UE Actors are facing up 
			const FQuat InvertUpRotationQuaternion = FQuat(Transform.GetRotation().GetAxisY(), PI);
			Transform.SetRotation(Transform.GetRotation() * InvertUpRotationQuaternion);

			return Transform;
		}

		///** Parses a GDTF string as DMX address */
		FDMXGDTFAddress ParseDMXAddress(const FString& GDTFString) const
		{
			if (GDTFString.IsEmpty())
			{
				return FDMXGDTFAddress();
			}

			// Can be an absolute adddress
			uint64 AbsoluteAddress;
			if (LexTryParseString(AbsoluteAddress, *GDTFString))
			{
				return FDMXGDTFAddress();
			}

			// Or in the form of Universe.Channel
			TArray<FString> Substrings;
			GDTFString.ParseIntoArray(Substrings, TEXT("."));

			if (Substrings.Num() == 2)
			{
				int32 Universe;
				int32 Channel;
				if (LexTryParseString(Universe, *Substrings[0]) &&
					LexTryParseString(Channel, *Substrings[1]))
				{
					constexpr uint16 UniverseSize = 512;

					FDMXGDTFAddress Result;
					Result.AbsoluteAddress = Universe * UniverseSize + Channel - 1;
					return Result;
				}
			}

			UE_LOG(LogDMXGDTF, Warning, TEXT("Failed to parse DMX Address. '%s' is not a valid GDTF string"), *GDTFString);
			return FDMXGDTFAddress();
		}

		/** The object that uses this parser */
		const TSharedRef<NodeType> UserObject;

		/** The XML node that needs to be parsed */
		const FXmlNode& XmlNode;
	};
}
