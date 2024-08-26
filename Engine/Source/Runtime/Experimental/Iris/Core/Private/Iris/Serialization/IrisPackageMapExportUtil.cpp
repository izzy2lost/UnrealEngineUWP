// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/Serialization/IrisPackageMapExportUtil.h"

#include "Iris/Core/IrisLog.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamUtil.h"
#include "Iris/Serialization/NetSerializers.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/Serialization/NetReferenceCollector.h"
#include "Iris/Serialization/NetSerializerArrayStorage.h"
#include "Iris/Serialization/IrisObjectReferencePackageMap.h"
#include "Iris/ReplicationSystem/NetTokenStore.h"
#include "Net/Core/Trace/NetTrace.h"

namespace UE::Net
{

const FNetSerializer* FIrisPackageMapExportsUtil::ObjectNetSerializer = &UE_NET_GET_SERIALIZER(FObjectNetSerializer);
const FNetSerializer* FIrisPackageMapExportsUtil::NameNetSerializer = &UE_NET_GET_SERIALIZER(FNameNetSerializer);

void FIrisPackageMapExportsUtil::Serialize(FNetSerializationContext& Context, const QuantizedType& Value)
{
	FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();

	// If we have any references, export them!
	{
		const uint32 NumReferences = Value.ObjectReferenceStorage.Num();
		if (Writer->WriteBool(NumReferences != 0))
		{
			UE::Net::WritePackedUint32(Writer, NumReferences);		
			FObjectNetSerializerConfig ObjectSerializerConfig;
			for (const FNetObjectReference& Ref : MakeArrayView(Value.ObjectReferenceStorage.GetData(), NumReferences))
			{
				FNetSerializeArgs ObjectArgs;
				ObjectArgs.NetSerializerConfig = &ObjectSerializerConfig;
				ObjectArgs.Source = NetSerializerValuePointer(&Ref);

				ObjectNetSerializer->Serialize(Context, ObjectArgs);
			}
		}
	}

	// If we have any names, export them!
	{
		const uint32 NumNames = Value.NameStorage.Num();
		if (Writer->WriteBool(NumNames != 0))
		{
			UE::Net::WritePackedUint32(Writer, NumNames);
			FNetSerializerConfig NameSerializerConfig;
			for (const QuantizedType::FQuantizedName& QuantizedName : MakeArrayView(Value.NameStorage.GetData(), NumNames))
			{
				FNetSerializeArgs NameArgs;
				NameArgs.NetSerializerConfig = &NameSerializerConfig;
				NameArgs.Source = NetSerializerValuePointer(&QuantizedName);

				NameNetSerializer->Serialize(Context, NameArgs);
			}
		}
	}

	// For NetTokens
	{
		// $TODO: For asymmetrical data like nettokens we will need to do something special to support validating the default state hash as the local quantized state will differ
		// For now we ignore this in default state hash
		if (Context.IsInitializingDefaultState())
		{
			return;
		}

		const uint32 NumNetTokens = Value.NetTokenStorage.Num();
		if (Writer->WriteBool(NumNetTokens != 0))
		{
			UE::Net::WritePackedUint32(Writer, NumNetTokens);
			for (const FNetToken& NetToken : MakeArrayView(Value.NetTokenStorage.GetData(), NumNetTokens))
			{
				// Always write the token
				WriteNetToken(Context, NetToken);
				// Export or add to pending exports for later export
				FNetTokenStore::AppendExportOrWriteInlinedExportData(Context, NetToken);
			}
		}
	}
}

void FIrisPackageMapExportsUtil::Deserialize(FNetSerializationContext& Context, QuantizedType& Value)
{
	FNetBitStreamReader* Reader = Context.GetBitStreamReader();

	// Read any object references
	{
		const bool bHasObjectReferences = Reader->ReadBool();
		if (bHasObjectReferences)
		{
			const uint32 NumReferences = UE::Net::ReadPackedUint32(Reader);

			if (NumReferences > MaxExports)
			{
				UE_LOG(LogIris, Error, TEXT("FIrisPackageMapExportsUtil::Received too many object reference exports %u > max:%u"), NumReferences, MaxExports);
				Context.SetError(GNetError_ArraySizeTooLarge);
				return;
			}

			Value.ObjectReferenceStorage.AdjustSize(Context, NumReferences);

			FObjectNetSerializerConfig ObjectSerializerConfig;
			for (FNetObjectReference& Ref : MakeArrayView(Value.ObjectReferenceStorage.GetData(), Value.ObjectReferenceStorage.Num()))
			{
				FNetDeserializeArgs ObjectArgs;
				ObjectArgs.NetSerializerConfig = &ObjectSerializerConfig;
				ObjectArgs.Target = NetSerializerValuePointer(&Ref);

				ObjectNetSerializer->Deserialize(Context, ObjectArgs);
			}
		}
		else
		{
			Value.ObjectReferenceStorage.Free(Context);
		}
	}

	// Read any exported names
	{
		const bool bHasNames = Reader->ReadBool();
		if (bHasNames)
		{
			const uint32 NumNames = UE::Net::ReadPackedUint32(Reader);

			if (NumNames > MaxExports)
			{
				UE_LOG(LogIris, Error, TEXT("FIrisPackageMapExportsUtil::Received too many name exports %u > max:%u"), NumNames, MaxExports);
				Context.SetError(GNetError_ArraySizeTooLarge);
				return;
			}

			Value.NameStorage.AdjustSize(Context, NumNames);

			FNetSerializerConfig NameSerializerConfig;
			for (QuantizedType::FQuantizedName& QuantizedName : MakeArrayView(Value.NameStorage.GetData(), Value.NameStorage.Num()))
			{
				FNetDeserializeArgs NameArgs;
				NameArgs.NetSerializerConfig = &NameSerializerConfig;
				NameArgs.Target = NetSerializerValuePointer(&QuantizedName);

				NameNetSerializer->Deserialize(Context, NameArgs);
			}
		}
		else
		{
			Value.NameStorage.Free(Context);
		}
	}

	// Read any exported nettokens
	{
		const bool bHasNetTokens = Reader->ReadBool();
		if (bHasNetTokens)
		{
			const uint32 NumNetTokens = UE::Net::ReadPackedUint32(Reader);

			if (NumNetTokens > MaxExports)
			{
				UE_LOG(LogIris, Error, TEXT("FIrisPackageMapExportsUtil::Received too many NetToken exports %u > max:%u"), NumNetTokens, MaxExports);
				Context.SetError(GNetError_ArraySizeTooLarge);
				return;
			}

			Value.NetTokenStorage.AdjustSize(Context, NumNetTokens);

			for (FNetToken& TargetToken : MakeArrayView(Value.NetTokenStorage.GetData(), Value.NetTokenStorage.Num()))
			{
				// Always Read the token
				FNetToken NetToken = ReadNetToken(Context);

				if (Reader->IsOverflown())
				{
					return;
				}

				// Read inlined exports if there are any
				FNetTokenStore::ReadInlinedExportData(Context, NetToken);

				TargetToken = NetToken;
			}
		}
		else
		{
			Value.NetTokenStorage.Free(Context);
		}
	}
}

void FIrisPackageMapExportsUtil::Quantize(FNetSerializationContext& Context, const UE::Net::FIrisPackageMapExports& PackageMapExports, QuantizedType& Value)
{
	// Quantize captured references
	{
		const FIrisPackageMapExports::FObjectReferenceArray& ObjectReferences = PackageMapExports.References;
		const uint32 NumObjectReferences = ObjectReferences.Num();
		Value.ObjectReferenceStorage.AdjustSize(Context, NumObjectReferences);
		if (NumObjectReferences > 0)
		{
			FObjectNetSerializerConfig ObjectNetSerializerConfig;
			const TObjectPtr<UObject>* SourceReferences = ObjectReferences.GetData();
			FNetObjectReference* TargetReferences = Value.ObjectReferenceStorage.GetData();
			for (uint32 ReferenceIndex = 0; ReferenceIndex < NumObjectReferences; ++ReferenceIndex)
			{
				FNetQuantizeArgs ObjectArgs;
				ObjectArgs.NetSerializerConfig = &ObjectNetSerializerConfig;
				ObjectArgs.Source = NetSerializerValuePointer(SourceReferences + ReferenceIndex);
				ObjectArgs.Target = NetSerializerValuePointer(TargetReferences + ReferenceIndex);

				ObjectNetSerializer->Quantize(Context, ObjectArgs);
			}
		}
	}

	// Quantize captured names
	{
		const FIrisPackageMapExports::FNameArray& Names = PackageMapExports.Names;
		const uint32 NumNames = Names.Num();
		Value.NameStorage.AdjustSize(Context, NumNames);
		if (NumNames > 0)
		{
			FNetSerializerConfig NameNetSerializerConfig;
			const FName* SourceNames = Names.GetData();
			QuantizedType::FQuantizedName* TargetNames = Value.NameStorage.GetData();
			for (uint32 ReferenceIndex = 0; ReferenceIndex < NumNames; ++ReferenceIndex)
			{
				FNetQuantizeArgs NameArgs;
				NameArgs.NetSerializerConfig = &NameNetSerializerConfig;
				NameArgs.Source = NetSerializerValuePointer(SourceNames + ReferenceIndex);
				NameArgs.Target = NetSerializerValuePointer(TargetNames + ReferenceIndex);

				NameNetSerializer->Quantize(Context, NameArgs);
			}
		}
	}

	// Just store captured nettokens
	{
		const FIrisPackageMapExports::FNetTokensArray& NetTokens = PackageMapExports.NetTokens;
		const uint32 NumNetTokens = NetTokens.Num();
		Value.NetTokenStorage.AdjustSize(Context, NumNetTokens);
		if (NumNetTokens > 0)
		{
			const FNetToken* SourceTokens = NetTokens.GetData();
			FNetToken* TargetTokens = Value.NetTokenStorage.GetData();
			for (uint32 ReferenceIndex = 0; ReferenceIndex < NumNetTokens; ++ReferenceIndex)
			{
				TargetTokens[ReferenceIndex] = SourceTokens[ReferenceIndex];
			}
		}
	}
}

void FIrisPackageMapExportsUtil::Dequantize(FNetSerializationContext& Context, const QuantizedType& Source, UE::Net::FIrisPackageMapExports& PackageMapExports)
{
	Private::FInternalNetSerializationContext* InternalContext = Context.GetInternalContext();

	// References
	{
		UE::Net::FIrisPackageMapExports::FObjectReferenceArray& ObjectReferences = PackageMapExports.References;
		const uint32 NumObjectReferences = Source.ObjectReferenceStorage.Num();
		if (NumObjectReferences > 0U)
		{		
			ObjectReferences.SetNumUninitialized(NumObjectReferences);

			FObjectNetSerializerConfig ObjectNetSerializerConfig;
			const FNetObjectReference* SourceReferences = Source.ObjectReferenceStorage.GetData();
			TObjectPtr<UObject>* TargetReferences = ObjectReferences.GetData();
			for (uint32 ReferenceIndex = 0; ReferenceIndex < NumObjectReferences; ++ReferenceIndex)
			{
				FNetDequantizeArgs ObjectArgs;
				ObjectArgs.NetSerializerConfig = &ObjectNetSerializerConfig;
				ObjectArgs.Source = NetSerializerValuePointer(SourceReferences + ReferenceIndex);
				ObjectArgs.Target = NetSerializerValuePointer(TargetReferences + ReferenceIndex);

				ObjectNetSerializer->Dequantize(Context, ObjectArgs);
			}
		}
	}

	// Names
	{
		UE::Net::FIrisPackageMapExports::FNameArray& Names = PackageMapExports.Names;

		const uint32 NumNames = Source.NameStorage.Num();
		if (NumNames > 0U)
		{		
			Names.SetNumUninitialized(NumNames);

			FNetSerializerConfig NameNetSerializerConfig;
			const QuantizedType::FQuantizedName* SourceNames = Source.NameStorage.GetData();
			FName* TargetNames = Names.GetData();
			for (uint32 ReferenceIndex = 0; ReferenceIndex < NumNames; ++ReferenceIndex)
			{
				FNetDequantizeArgs NameArgs;
				NameArgs.NetSerializerConfig = &NameNetSerializerConfig;
				NameArgs.Source = NetSerializerValuePointer(SourceNames + ReferenceIndex);
				NameArgs.Target = NetSerializerValuePointer(TargetNames + ReferenceIndex);

				NameNetSerializer->Dequantize(Context, NameArgs);
			}
		}
	}

	// NetTokens
	{
		UE::Net::FIrisPackageMapExports::FNetTokensArray& NetTokens = PackageMapExports.NetTokens;

		const uint32 NumNetTokens = Source.NetTokenStorage.Num();
		if (NumNetTokens > 0U)
		{		
			NetTokens.SetNumUninitialized(NumNetTokens);

			const FNetToken* SourceTokens = Source.NetTokenStorage.GetData();
			FNetToken* TargetTokens = NetTokens.GetData();
			for (uint32 ReferenceIndex = 0; ReferenceIndex < NumNetTokens; ++ReferenceIndex)
			{
				TargetTokens[ReferenceIndex] = SourceTokens[ReferenceIndex];
			}
		}
	}
}

bool FIrisPackageMapExportsUtil::IsEqual(FNetSerializationContext& Context, const QuantizedType& Value0, const QuantizedType& Value1)
{
	if ((Value0.ObjectReferenceStorage.Num() != Value1.ObjectReferenceStorage.Num()) || (Value0.NameStorage.Num() != Value1.NameStorage.Num()) || (Value0.NetTokenStorage.Num() != Value1.NetTokenStorage.Num()))
	{
		return false;
	}

	if (Value0.ObjectReferenceStorage.Num() > 0 && FMemory::Memcmp(Value0.ObjectReferenceStorage.GetData(), Value1.ObjectReferenceStorage.GetData(), sizeof(FNetObjectReference) * Value0.ObjectReferenceStorage.Num()) != 0)
	{
		return false;
	}

	if (Value0.NameStorage.Num() > 0 && FMemory::Memcmp(Value0.NameStorage.GetData(), Value1.NameStorage.GetData(), sizeof(FName) * Value0.NameStorage.Num()) != 0)
	{
		return false;
	}

	// Note: this can be improved by implementing a resolved token compare. Currently this is pessimistic and does not allow compare between auth and non-auth tokens even if they would
	// resolve to same value.
	if (Value0.NetTokenStorage.Num() > 0 && FMemory::Memcmp(Value0.NetTokenStorage.GetData(), Value1.NetTokenStorage.GetData(), sizeof(FNetToken) * Value0.NetTokenStorage.Num()) != 0)
	{
		return false;
	}

	return true;
}

void FIrisPackageMapExportsUtil::CloneDynamicState(FNetSerializationContext& Context, QuantizedType& Target, const QuantizedType& Source)
{
	Target.ObjectReferenceStorage.Clone(Context, Source.ObjectReferenceStorage);
	Target.NameStorage.Clone(Context, Source.NameStorage);
	Target.NetTokenStorage.Clone(Context, Source.NetTokenStorage);
}

void FIrisPackageMapExportsUtil::FreeDynamicState(FNetSerializationContext& Context, QuantizedType& Value)
{
	// Clear all info
	Value.ObjectReferenceStorage.Free(Context);
	Value.NameStorage.Free(Context);
	Value.NetTokenStorage.Free(Context);
}

void FIrisPackageMapExportsUtil::CollectNetReferences(FNetSerializationContext& Context, const QuantizedType& Value, const FNetSerializerChangeMaskParam& ChangeMaskInfo, FNetReferenceCollector& Collector)
{
	const FNetReferenceInfo ReferenceInfo(FNetReferenceInfo::EResolveType::ResolveOnClient);	
	for (const FNetObjectReference& Ref : MakeArrayView(Value.ObjectReferenceStorage.GetData(), Value.ObjectReferenceStorage.Num()))
	{
		Collector.Add(ReferenceInfo, Ref, ChangeMaskInfo);
	}
}

bool FIrisPackageMapExportsUtil::Validate(FNetSerializationContext& Context, const QuantizedType& SourceValue)
{
	if ((SourceValue.ObjectReferenceStorage.Num() > MaxExports) || (SourceValue.NameStorage.Num() > MaxExports) || (SourceValue.NameStorage.Num() > MaxExports))
	{
		return false;
	}
	return true;
}

}
