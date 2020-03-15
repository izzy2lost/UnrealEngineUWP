#include "../OnlineSubsystemLivePrivatePCH.h"

#if WITH_MARKETPLACE

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "Windows/AllowWindowsPlatformAtomics.h"

namespace Microsoft
{
namespace Xbox
{
namespace Services
{
namespace Marketplace
{

using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Services::Store;

CatalogItemAvailability::CatalogItemAvailability(Windows::Services::Store::StoreProduct^ InProduct, StoreSku^ InSku, StoreAvailability^ InAvailability)
	: Sku(InSku)
	, Availability(InAvailability)
{
	ExtendedData.ConsumableQuantity = 0;
	ExtendedData.Price = 0;
	ExtendedData.ListPrice = 0;
	ExtendedData.IsPurchasable = false;

	if (String::CompareOrdinal(InProduct->ProductKind, L"Consumable") == 0)
	{
		TSharedRef<TJsonReader<>> SkuReader = TJsonReaderFactory<>::Create(Sku->ExtendedJsonData->Data());
		TSharedPtr<FJsonObject> SkuExtendedJsonObject;
		if (FJsonSerializer::Deserialize(SkuReader, SkuExtendedJsonObject) && SkuExtendedJsonObject.IsValid())
		{
			if (!SkuExtendedJsonObject->TryGetNumberField(TEXT("ConsumableQuantity"), ExtendedData.ConsumableQuantity))
			{
				ExtendedData.ConsumableQuantity = 0;
			}
		}
	}

	TSharedRef<TJsonReader<>> AvailabilityReader = TJsonReaderFactory<>::Create(Availability->ExtendedJsonData->Data());
	TSharedPtr<FJsonObject> AvailabilityExtendedJsonObject;
	if (FJsonSerializer::Deserialize(AvailabilityReader, AvailabilityExtendedJsonObject) && AvailabilityExtendedJsonObject.IsValid())
	{
		TSharedPtr<FJsonObject> OrderManagementObject = AvailabilityExtendedJsonObject->GetObjectField(TEXT("OrderManagementData"));
		if (OrderManagementObject.IsValid())
		{
			TSharedPtr<FJsonObject> PriceObject = OrderManagementObject->GetObjectField(TEXT("Price"));
			if (PriceObject.IsValid())
			{
				ExtendedData.Price = PriceObject->GetNumberField(TEXT("MSRP"));
				ExtendedData.ListPrice = PriceObject->GetNumberField(TEXT("ListPrice"));
			}
		}

		const TArray<TSharedPtr<FJsonValue>>& Actions = AvailabilityExtendedJsonObject->GetArrayField(TEXT("Actions"));
		for (const TSharedPtr<FJsonValue>& Action : Actions)
		{
			if (Action->AsString().Compare(TEXT("Purchase"), ESearchCase::IgnoreCase) == 0)
			{
				ExtendedData.IsPurchasable = true;
				break;
			}
		}
	}

}

CatalogItemDetails::CatalogItemDetails(StoreProduct^ InProduct)
	: Product(InProduct)
{
	AvailabilitiesVector = ref new Vector<CatalogItemAvailability^>();
	for (StoreSku^ Sku : Product->Skus)
	{
		for (StoreAvailability^ Availability : Sku->Availabilities)
		{
			AvailabilitiesVector->Append(ref new CatalogItemAvailability(Product, Sku, Availability));
		}
	}
}

InventoryItem::InventoryItem(Windows::Services::Store::StoreProduct^ InProduct)
	: Product(InProduct)
{
	AggregatedCollectionData.ConsumableBalance = 0;
	AggregatedCollectionData.IsTrialEntitlement = true;
	AggregatedCollectionData.EndDate.UniversalTime = 0;
	AggregatedCollectionData.StartDate.UniversalTime = MAX_int64;
	AggregatedCollectionData.RightsObtainedDate.UniversalTime = MAX_int64;
	for (StoreSku^ Sku : Product->Skus)
	{
		if (Sku->IsInUserCollection)
		{
			StoreCollectionData^ CollectionData = Sku->CollectionData;
			AggregatedCollectionData.IsTrialEntitlement &= Sku->IsTrial;
			AggregatedCollectionData.StartDate.UniversalTime = FMath::Max(CollectionData->StartDate.UniversalTime, AggregatedCollectionData.StartDate.UniversalTime);
			AggregatedCollectionData.RightsObtainedDate.UniversalTime = FMath::Max(CollectionData->AcquiredDate.UniversalTime, AggregatedCollectionData.RightsObtainedDate.UniversalTime);
			AggregatedCollectionData.EndDate.UniversalTime = FMath::Max(CollectionData->EndDate.UniversalTime, AggregatedCollectionData.EndDate.UniversalTime);

			if (String::CompareOrdinal(InProduct->ProductKind, L"Consumable") == 0)
			{
				TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(CollectionData->ExtendedJsonData->Data());
				TSharedPtr<FJsonObject> ExtendedJsonObject;
				if (FJsonSerializer::Deserialize(JsonReader, ExtendedJsonObject) && ExtendedJsonObject.IsValid())
				{
					double CollectionQuantityForThisSku;
					if (ExtendedJsonObject->TryGetNumberField(TEXT("quantity"), CollectionQuantityForThisSku))
					{
						AggregatedCollectionData.ConsumableBalance += FMath::RoundToInt(CollectionQuantityForThisSku);
					}
				}
			}
		}
	}
}

InventoryItemsResult::InventoryItemsResult(Windows::Services::Store::StoreProductPagedQueryResult^ InStoreResult)
	: StoreResult(InStoreResult)
{
	InventoryItems = ref new Vector<InventoryItem^>();
	for (auto Product : StoreResult->Products)
	{
		UE_LOG_ONLINE(Verbose, TEXT("%s"), Product->Value->ExtendedJsonData->Data());
		InventoryItems->Append(ref new InventoryItem(Product->Value));
	}
}

IAsyncOperation<IVectorView<CatalogItemDetails^>^>^ CatalogAndInventoryServiceShim::GetCatalogItemDetailsAsync(IVectorView<String^>^ Products)
{
	if (Context == nullptr)
	{
		throw ref new Platform::FailureException();
	}

	Vector<String^>^ ProductKinds = ref new Vector<Platform::String^>{ L"Durable", L"Consumable" };
	auto GetProductsTask = concurrency::create_task(Context->GetStoreProductsAsync(ProductKinds, Products))
		.then([](StoreProductQueryResult^ Result)
	{
		Vector<CatalogItemDetails^>^ Catalog = ref new Vector<CatalogItemDetails^>();
		for (auto Product : Result->Products)
		{
			UE_LOG_ONLINE(Verbose, TEXT("%s"), Product->Value->ExtendedJsonData->Data());
			Catalog->Append(ref new CatalogItemDetails(Product->Value));
		}

		return Catalog->GetView();
	});

	return concurrency::create_async([GetProductsTask]() { return GetProductsTask; });
}

IAsyncOperation<InventoryItemsResult^>^ InventoryItemsResult::GetNextAsync(uint32 MaxItems)
{
	auto NextPageTask = concurrency::create_task(StoreResult->GetNextAsync())
		.then([](StoreProductPagedQueryResult^ Result)
	{
		return ref new InventoryItemsResult(Result);
	});
	return concurrency::create_async([NextPageTask]() { return NextPageTask; });
}

IAsyncOperation<InventoryItemsResult^>^ CatalogAndInventoryServiceShim::GetInventoryItemsForAllUsersAsync(MediaItemType MediaType, bool ExpandSatisfyingEntitlements, bool IncludeAllItemStateAndAvailabilities)
{
	return GetInventoryItemsAsync(MediaType, ExpandSatisfyingEntitlements, IncludeAllItemStateAndAvailabilities);
}

IAsyncOperation<InventoryItemsResult^>^ CatalogAndInventoryServiceShim::GetInventoryItemsAsync(MediaItemType MediaType, bool ExpandSatisfyingEntitlements, bool IncludeAllItemStateAndAvailabilities)
{
	if (Context == nullptr)
	{
		throw ref new Platform::FailureException();
	}

	Vector<String^>^ ProductKinds = ref new Vector<Platform::String^>();
	switch (MediaType)
	{
	case MediaItemType::GameV2:
		ProductKinds->Append(L"Game");
		break;

	case MediaItemType::GameContent:
		ProductKinds->Append(L"Durable");
		break;

	case MediaItemType::GameConsumable:
		ProductKinds->Append(L"Consumable");
		ProductKinds->Append(L"UnmanagedConsumable");
		break;

	default:
		throw ref new Platform::InvalidArgumentException();
	}

	auto GetInventoryTask = concurrency::create_task(Context->GetUserCollectionWithPagingAsync(ProductKinds, 10))
		.then([](StoreProductPagedQueryResult^ Result)
	{
		return ref new InventoryItemsResult(Result);
	});

	return concurrency::create_async([GetInventoryTask]() { return GetInventoryTask; });
}

CatalogAndInventoryServiceShim::CatalogAndInventoryServiceShim()
{
	if (Windows::Foundation::Metadata::ApiInformation::IsTypePresent("Windows.Services.Store.StoreContext"))
	{
		Context = Windows::Services::Store::StoreContext::GetDefault();
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("Windows.Services.Store.StoreContext is not available on this version of Windows.  Attempts to access Store or Inventory will fail."));
		Context = nullptr;
	}
}

}
}
}
}

#include "Windows/HideWindowsPlatformAtomics.h"

#endif // WITH_MARKETPLACE