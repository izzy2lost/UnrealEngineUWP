#if WITH_MARKETPLACE

namespace Microsoft
{
	namespace Xbox
	{
		namespace Services
		{
			namespace Marketplace
			{
				private ref class CatalogItemAvailability sealed
				{
				public:
					property Platform::String^ CurrencyCode
					{
						Platform::String^ get() { return Availability->Price->CurrencyCode; }
					}

					property Platform::String^ DisplayListPrice
					{
						Platform::String^ get() { return Availability->Price->FormattedBasePrice; }
					}

					property Platform::String^ DisplayPrice
					{
						Platform::String^ get() { return Availability->Price->FormattedPrice; }
					}

					property double Price
					{
						double get() { return ExtendedData.Price; }
					}

					property double ListPrice
					{
						double get() { return ExtendedData.ListPrice; }
					}

					property uint32 ConsumableQuantity
					{
						uint32 get() { return ExtendedData.ConsumableQuantity; }
					}

					property Platform::String^ SignedOffer
					{
						Platform::String^ get() { return Availability->StoreId; }
					}

					property Platform::String^ OfferId
					{
						Platform::String^ get() { return Availability->StoreId; }
					}

					property bool IsPurchasable
					{
						bool get() { return ExtendedData.IsPurchasable; }
					}

					// The following properties are not part of the data schema for Store products on UWP.
					// Values are provided as a convenience to minimize disruption to UE code, but they're
					// not likely to be correct if used for anything important.
					// Note that at the time of writing these are only used for (optional) logging in any case.

					property Platform::String^ ContentId
					{
						Platform::String^ get() { return Availability->StoreId; }
					}

					property Windows::Foundation::Collections::IVectorView<Platform::String^>^ AcceptablePaymentInstrumentTypes
					{
						Windows::Foundation::Collections::IVectorView<Platform::String^>^ get()
						{
							static Platform::Collections::Vector<Platform::String^>^ PaymentInstruments = ref new Platform::Collections::Vector<Platform::String^>();
							return PaymentInstruments->GetView();
						}
					}

					property Platform::String^ AvailabilityTitle
					{
						Platform::String^ get() { return ""; }
					}

					property Platform::String^ AvailabilityDescription
					{
						Platform::String^ get() { return ""; }
					}

					property Platform::String^ DistributionType
					{
						Platform::String^ get() { return ""; }
					}

					property Platform::String^ PromotionalText
					{
						Platform::String^ get() { return ""; }
					}

					property Platform::String^ OfferDisplayDataJson
					{
						Platform::String^ get() { return "{}"; }
					}

				internal:
					CatalogItemAvailability(Windows::Services::Store::StoreProduct^ InProduct, Windows::Services::Store::StoreSku^ InSku, Windows::Services::Store::StoreAvailability^ InAvailability);

				private:

					struct
					{
						double Price;
						double ListPrice;
						uint32 ConsumableQuantity;
						bool IsPurchasable;
					} ExtendedData;

					Windows::Services::Store::StoreSku^ Sku;
					Windows::Services::Store::StoreAvailability^ Availability;
				};

				private ref class CatalogItemDetails sealed
				{
				public:
					property Platform::String^ Name
					{
						Platform::String^ get() { return Product->Title; }
					}

					property Platform::String^ Description
					{
						Platform::String^ get() { return Product->Description; }
					}

					property Platform::String^ ProductId
					{
						Platform::String^ get() { return Product->StoreId; }
					}

					property Windows::Foundation::Collections::IVectorView<CatalogItemAvailability^>^ Availabilities
					{
						Windows::Foundation::Collections::IVectorView<CatalogItemAvailability^>^ get() { return AvailabilitiesVector->GetView(); }
					}

					// The following properties are not part of the data schema for Store products on UWP.
					// Values are provided as a convenience to minimize disruption to UE code, but they're
					// not likely to be correct if used for anything important.
					// Note that at the time of writing these are only used for (optional) logging in any case.

					property Platform::String^ Id
					{
						Platform::String^ get() { return Product->StoreId; }
					}

					property Platform::String^ ReducedName
					{
						Platform::String^ get() { return Product->Title; }
					}

					property uint32 TitleId
					{
						uint32 get() { return Microsoft::Xbox::Services::XboxLiveAppConfiguration::SingletonInstance->TitleId; }
					}

					property Platform::String^ ServiceConfigId
					{
						Platform::String^ get() { return Microsoft::Xbox::Services::XboxLiveAppConfiguration::SingletonInstance->ServiceConfigurationId; }
					}

					property Platform::String^ SandboxId
					{
						Platform::String^ get() { return Microsoft::Xbox::Services::XboxLiveAppConfiguration::SingletonInstance->Sandbox; }
					}

					property Windows::Foundation::DateTime ReleaseDate
					{
						Windows::Foundation::DateTime get() { return Windows::Foundation::DateTime(); }
					}

					property bool IsBundle
					{
						// Derive from enumerating SKUs?
						bool get() { return false; }
					}

					property bool IsPartOfAnyBundle
					{
						// Derive from enumerating SKUs?
						bool get() { return false; }
					}

				internal:
					CatalogItemDetails(Windows::Services::Store::StoreProduct^ InProduct);

				private:
					Windows::Services::Store::StoreProduct^ Product;
					Platform::Collections::Vector<CatalogItemAvailability^>^ AvailabilitiesVector;
				};

				private enum class MediaItemType
				{
					Unknown = 0,
					GameV2 = 2,
					GameContent = 4,
					GameConsumable = 5,
				};

				private enum class InventoryItemState
				{
					Unknown,
					All,
					Enabled,
					Suspended,
					Expired,
					Canceled,
				};

				private ref class InventoryItem sealed
				{
				public:
					property Platform::String^ ProductId
					{
						Platform::String^ get() { return Product->StoreId; }
					}

					property uint32 ConsumableBalance
					{
						uint32 get() { return AggregatedCollectionData.ConsumableBalance; }
					}

					property Windows::Foundation::Uri^ Url
					{
						Windows::Foundation::Uri^ get() { return Product->LinkUri; }
					}

					property uint32 TitleId
					{
						uint32 get() { return Microsoft::Xbox::Services::XboxLiveAppConfiguration::SingletonInstance->TitleId; }
					}

					property Windows::Foundation::Uri^ ConsumableUrl
					{
						Windows::Foundation::Uri^ get() { return MediaItemType == Microsoft::Xbox::Services::Marketplace::MediaItemType::GameConsumable ? Product->LinkUri : nullptr; }
					}

					property Windows::Foundation::DateTime RightsObtainedDate
					{
						Windows::Foundation::DateTime get() { return AggregatedCollectionData.RightsObtainedDate; }
					}

					property Windows::Foundation::DateTime StartDate
					{
						Windows::Foundation::DateTime get() { return AggregatedCollectionData.StartDate; }
					}

					property Windows::Foundation::DateTime EndDate
					{
						Windows::Foundation::DateTime get() { return AggregatedCollectionData.EndDate; }
					}

					property InventoryItemState InventoryItemState
					{
						Microsoft::Xbox::Services::Marketplace::InventoryItemState get() { return Microsoft::Xbox::Services::Marketplace::InventoryItemState::Unknown; }
					}

					property MediaItemType MediaItemType
					{
						Microsoft::Xbox::Services::Marketplace::MediaItemType get() { return Microsoft::Xbox::Services::Marketplace::MediaItemType::Unknown; }
					}

					property bool IsTrialEntitlement
					{
						bool get() { return AggregatedCollectionData.IsTrialEntitlement; }
					}

					property Windows::Foundation::TimeSpan TrialTimeRemaining
					{
						Windows::Foundation::TimeSpan get() { return Windows::Foundation::TimeSpan(); }
					}
				internal:
					InventoryItem(Windows::Services::Store::StoreProduct^ InProduct);

				private:
					Windows::Services::Store::StoreProduct^ Product;

					struct
					{
						uint32 ConsumableBalance;
						Windows::Foundation::DateTime RightsObtainedDate;
						Windows::Foundation::DateTime StartDate;
						Windows::Foundation::DateTime EndDate;
						bool IsTrialEntitlement;
					} AggregatedCollectionData;
				};

				private ref class InventoryItemsResult sealed
				{
				public:
					property Windows::Foundation::Collections::IVectorView<InventoryItem^>^ Items
					{
						Windows::Foundation::Collections::IVectorView<InventoryItem^>^ get() { return InventoryItems->GetView(); }
					}

					property bool HasNext
					{
						bool get() { return StoreResult->HasMoreResults; }
					}

					Windows::Foundation::IAsyncOperation<InventoryItemsResult^>^ GetNextAsync(uint32 MaxItems);

				internal:
					InventoryItemsResult(Windows::Services::Store::StoreProductPagedQueryResult^ InStoreResult);

				private:
					Windows::Services::Store::StoreProductPagedQueryResult^ StoreResult;
					Platform::Collections::Vector<InventoryItem^>^ InventoryItems;
				};

				private ref class CatalogAndInventoryServiceShim sealed
				{
				public:
					Windows::Foundation::IAsyncOperation<InventoryItemsResult^>^ GetInventoryItemsForAllUsersAsync(MediaItemType MediaType, bool ExpandSatisfyingEntitlements, bool IncludeAllItemStateAndAvailabilities);

					Windows::Foundation::IAsyncOperation<InventoryItemsResult^>^ GetInventoryItemsAsync(MediaItemType MediaType, bool ExpandSatisfyingEntitlements, bool IncludeAllItemStateAndAvailabilities);

					Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<CatalogItemDetails^>^>^ GetCatalogItemDetailsAsync(Windows::Foundation::Collections::IVectorView<Platform::String^>^ Products);

					static CatalogAndInventoryServiceShim^ Get()
					{
						static CatalogAndInventoryServiceShim^ SingletonInstance = ref new CatalogAndInventoryServiceShim();
						return SingletonInstance;
					}

				private:
					CatalogAndInventoryServiceShim();

					Windows::Services::Store::StoreContext^ Context;
				};

				using CatalogService = CatalogAndInventoryServiceShim;
				using InventoryService = CatalogAndInventoryServiceShim;
			}
		}

	}
}

#endif