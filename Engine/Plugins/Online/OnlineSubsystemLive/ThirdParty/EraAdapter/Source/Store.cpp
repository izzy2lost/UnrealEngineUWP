//-----------------------------------------------------------------------------
//				Copyright (C) Microsoft. All rights reserved.
//-----------------------------------------------------------------------------
#include "pch.h"
#include "Store.h"
#include "User.h"

#include <future>

using namespace EraAdapter::Windows::Xbox::ApplicationModel::Store;
using namespace EraAdapter::Windows::Xbox::System;
using namespace Platform;
using namespace ::Windows::Foundation;
using namespace ::Windows::Services::Store;
using namespace concurrency;

String^
ProductPurchasedEventArgs::Receipt::get()
{
	return _receipt;
}

ProductPurchasedEventArgs::ProductPurchasedEventArgs(
	String^ receipt
) :
	_receipt(receipt)
{
}

///////////////////////////////////////////////////////////////////////////////
//
//  Product
//

::Windows::Services::Store::StoreContext^ Product::s_context = nullptr;

IAsyncOperation<PrivilegeCheckResult>^
Product::CheckPrivilegeAsync(
	User^ user,
	uint32 privilegeId,
	bool attemptResolution,
	String^ friendlyDisplay
)
{
	using namespace Microsoft::Xbox::Services::System;

	task<bool> uwpPrivilegeCheck =
		attemptResolution ?
			create_task(TitleCallableUI::CheckGamingPrivilegeWithUI(static_cast<GamingPrivilege>(privilegeId), friendlyDisplay)) :
			task_from_result(TitleCallableUI::CheckGamingPrivilegeSilently(static_cast<GamingPrivilege>(privilegeId)));

	return create_async([uwpPrivilegeCheck]()
	{
		return uwpPrivilegeCheck.then(
			[](bool hasPrivilege)
		{
			return hasPrivilege ? PrivilegeCheckResult::NoIssue : PrivilegeCheckResult::Restricted;
		});
	});
}

IAsyncAction^
Product::ShowPurchaseAsync(
	User^ user,
	String^ offer)
{
	if (::Windows::Foundation::Metadata::ApiInformation::IsTypePresent("Windows.Services.Store.StoreContext"))
	{
		// Since this can only safely be called by the UI thread in any case this init should be fine without
		// additional thread safety.
		if (s_context == nullptr)
		{
			s_context = StoreContext::GetDefault();
		}

		auto purchaseTask = concurrency::create_task(s_context->RequestPurchaseAsync(offer)).then(
			[user, offer](StorePurchaseResult^ purchaseResult)
		{
			switch (purchaseResult->Status)
			{
			case StorePurchaseStatus::Succeeded:
			case StorePurchaseStatus::AlreadyPurchased:
				return RequestDownloadOrUpdateContentForOfferAsync(user, offer);
				// Match Xbox behavior by throwing on failure.
			case StorePurchaseStatus::NotPurchased:
				// User canceled
				throw ref new OperationCanceledException();

			default:
				if (FAILED(purchaseResult->ExtendedError.Value))
				{
					throw ref new COMException(purchaseResult->ExtendedError.Value);
				}
				else
				{
					throw ref new FailureException();
				}
			}
		});

		return create_async([purchaseTask]()
		{
			return purchaseTask;
		});
	}
	else
	{
		return create_async(
			[]()
		{
			throw ref new NotImplementedException();
		});
	}
}

IAsyncAction^
Product::ShowPurchaseForStoreIdAsync(
	User^ user,
	String^ offer)
{
	return ShowPurchaseAsync(user, offer);
}

IAsyncAction^
Product::ShowRedeemCodeAsync(
	User^ user,
	String^ offer)
{
	task<bool> launchTask = create_task(::Windows::System::Launcher::LaunchUriAsync(ref new Uri("http://microsoft.com/redeem")));

	return create_async([launchTask]()
	{
		return launchTask.then(
			[](bool launchSucceeded)
		{
		});
	});
}

EventRegistrationToken Product::ProductPurchased::add(ProductPurchasedEventHandler^ handler)
{
	return _productPurchased += ref new EventHandler<ProductPurchasedEventArgs^>(
		[handler](Object^, ProductPurchasedEventArgs^ args)
	{
		handler(args);
	});
}

void Product::ProductPurchased::remove(EventRegistrationToken token)
{
	_productPurchased -= token;
}

IAsyncAction^
Product::RequestDownloadOrUpdateContentForOfferAsync(
	User^ user,
	String^ offer
)
{
	if (::Windows::Foundation::Metadata::ApiInformation::IsTypePresent("Windows.Services.Store.StoreContext"))
	{
		// Since this can only safely be called by the UI thread in any case this init should be fine without
		// additional thread safety.
		if (s_context == nullptr)
		{
			s_context = StoreContext::GetDefault();
		}

		// First discover whether this offer even has associated downloadable content
		// Limit query to offer types that support downloadable content
		auto productKinds = ref new Platform::Collections::Vector<String^>{ L"Durable" };
		auto products = ref new Platform::Collections::Vector<String^>{ offer };
		auto taskChain = concurrency::create_task(s_context->GetStoreProductsAsync(productKinds, products)).then(
			[products, offer](StoreProductQueryResult^ queryResult)
		{
			if (FAILED(queryResult->ExtendedError.Value))
			{
				throw ref new COMException(queryResult->ExtendedError.Value);
			}

			StoreProduct^ locatedProduct = nullptr;
			try
			{
				queryResult->Products->Lookup(offer);
			}
			catch (Platform::OutOfBoundsException^)
			{
				// Don't report this as an error - it could be that the passed in offer is valid, just not a type that supports DLC.
				return concurrency::create_task([]() {});
			}

			// ...but if the lookup succeeded it better have found a valid offer.
			if (locatedProduct == nullptr)
			{
				throw ref new FailureException();
			}

			if (locatedProduct->HasDigitalDownload)
			{
				using namespace ::Windows::Data::Json;

				JsonValue^ extendedJsonData = JsonValue::Parse(locatedProduct->ExtendedJsonData);
				JsonObject^ extendedProperties = extendedJsonData->GetObject()->GetNamedObject(L"Properties");
				String^ packageFamilyName = extendedProperties->GetNamedString(L"PackageFamilyName");

				bool alreadyHasPackage = false;
				for (::Windows::ApplicationModel::Package^ existingPackage : ::Windows::ApplicationModel::Package::Current->Dependencies)
				{
					if (String::CompareOrdinal(existingPackage->Id->FamilyName, packageFamilyName) == 0)
					{
						alreadyHasPackage = true;
						break;
					}
				}

				if (alreadyHasPackage)
				{
					// Package exists locally, but maybe it needs updating
					auto checkForUpdates = concurrency::create_task(s_context->GetAppAndOptionalStorePackageUpdatesAsync()).then(
						[packageFamilyName](::Windows::Foundation::Collections::IVectorView<StorePackageUpdate^>^ availableUpdates)
					{
						Platform::Collections::Vector<StorePackageUpdate^>^ updatesToInstall = ref new Platform::Collections::Vector<StorePackageUpdate^>();
						for (StorePackageUpdate^ update : availableUpdates)
						{
							if (String::CompareOrdinal(update->Package->Id->FamilyName, packageFamilyName) == 0)
							{
								updatesToInstall->Append(update);
							}
						}

						return FinishTaskChainForDownloadOperation(s_context->RequestDownloadAndInstallStorePackageUpdatesAsync(updatesToInstall));
					});

					return checkForUpdates;
				}
				else
				{
					// Requested offer has associated content but not present on current machine.  Download it!
					return FinishTaskChainForDownloadOperation(s_context->RequestDownloadAndInstallStorePackagesAsync(products));
				}
			}
			else
			{
				// No download required.
				return concurrency::create_task([]() {});
			}
		});
		return concurrency::create_async(
			[taskChain]()
		{
			return taskChain;
		});
	}
	else
	{
		return create_async(
			[]()
		{
			throw ref new NotImplementedException();
		});
	}
}

concurrency::task<void> Product::FinishTaskChainForDownloadOperation(
	IAsyncOperationWithProgress<StorePackageUpdateResult^, StorePackageUpdateStatus>^ downloadOperation)
{
	concurrency::task_completion_event<void> downloadStartedOrWillNotHappen;

	downloadOperation->Progress = ref new AsyncOperationProgressHandler<StorePackageUpdateResult^, StorePackageUpdateStatus>(
		[downloadStartedOrWillNotHappen](IAsyncOperationWithProgress<StorePackageUpdateResult^, StorePackageUpdateStatus>^, StorePackageUpdateStatus)
	{
		// We should allow the 'purchase' call to complete as soon as the download is on its way.
		// The operation will then finish completely in the background.
		downloadStartedOrWillNotHappen.set();
	});
	downloadOperation->Completed = ref new AsyncOperationWithProgressCompletedHandler<StorePackageUpdateResult^, StorePackageUpdateStatus>(
		[downloadStartedOrWillNotHappen](IAsyncOperationWithProgress<StorePackageUpdateResult^, StorePackageUpdateStatus>^ completedOperation, AsyncStatus)
	{
		if (completedOperation->Status == AsyncStatus::Error)
		{
			downloadStartedOrWillNotHappen.set_exception(ref new COMException(completedOperation->ErrorCode.Value));
		}
		else
		{
			StorePackageUpdateResult^ downloadResult = completedOperation->GetResults();
			// Only report as an error if there was actually something to download
			if (downloadResult->OverallState != StorePackageUpdateState::Completed &&
				downloadResult->StorePackageUpdateStatuses->Size > 0 &&
				downloadResult->StorePackageUpdateStatuses->GetAt(0).PackageDownloadSizeInBytes > 0)
			{
				downloadStartedOrWillNotHappen.set_exception(ref new FailureException());
			}
			else
			{
				downloadStartedOrWillNotHappen.set();
			}
		}
	});

	return concurrency::create_task(downloadStartedOrWillNotHappen);
}
