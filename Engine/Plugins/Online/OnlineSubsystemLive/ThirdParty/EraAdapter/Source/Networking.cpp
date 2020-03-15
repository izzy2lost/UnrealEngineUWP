//-----------------------------------------------------------------------------
//				Copyright (C) Microsoft. All rights reserved.
//-----------------------------------------------------------------------------
#include "pch.h"
#include "Networking.h"

using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Storage::Streams;

namespace EraAdapter {
namespace Windows {
namespace Xbox {
namespace Networking {

///////////////////////////////////////////////////////////////////////////////
//
//  SecureDeviceAddress
//

SecureDeviceAddress::SecureDeviceAddress(
	IBuffer^ buffer
	)
{
	_theRealThing = ::Windows::Networking::XboxLive::XboxLiveDeviceAddress::CreateFromSnapshotBuffer(buffer);
	_eventRegistered = false;
}

SecureDeviceAddress::SecureDeviceAddress(
	::Windows::Networking::XboxLive::XboxLiveDeviceAddress^ sda
)
{
	_theRealThing = sda;
	_eventRegistered = false;
}

EventRegistrationToken
SecureDeviceAddress::BufferChanged::add(
	::Windows::Foundation::TypedEventHandler<SecureDeviceAddress^, ::Object^>^ handler
	)
{
	if (_eventRegistered == false)
	{
		_theRealThing->SnapshotChanged += ref new TypedEventHandler<::Windows::Networking::XboxLive::XboxLiveDeviceAddress ^, Object ^>(this, &SecureDeviceAddress::OnSnapshotChanged);
		_eventRegistered = true;
	}

	return _bufferChanged += handler;
}

void
SecureDeviceAddress::BufferChanged::remove(
	EventRegistrationToken token
	)
{
	_bufferChanged -= token;
}

void
SecureDeviceAddress::BufferChanged::raise(
	SecureDeviceAddress^ sender,
	Object^ args
	)
{
	_bufferChanged(sender, args);
}

void
SecureDeviceAddress::OnSnapshotChanged(
	::Windows::Networking::XboxLive::XboxLiveDeviceAddress ^address,
	Object ^args
	)
{
	BufferChanged(
		ref new SecureDeviceAddress(address),
		args
		);
}

int
SecureDeviceAddress::Compare(
	SecureDeviceAddress^ secureDeviceAddress
	)
{
	return UnderlyingObject->Compare(secureDeviceAddress->UnderlyingObject);
}

String^
SecureDeviceAddress::GetBase64String(
	)
{
	return UnderlyingObject->GetSnapshotAsBase64();
}

IBuffer^
SecureDeviceAddress::GetBuffer(
	)
{
	return UnderlyingObject->GetSnapshotAsBuffer();
}

int
SecureDeviceAddress::CompareBuffers(
	IBuffer^ buffer1,
	IBuffer^ buffer2
	)
{
	auto sda1 = ref new SecureDeviceAddress(buffer1);
	auto sda2 = ref new SecureDeviceAddress(buffer2);

	return sda1->Compare(sda2);
}

int
SecureDeviceAddress::CompareBytes(
	const Array<byte>^ bytes1,
	const Array<byte>^ bytes2
	)
{
	auto sda1 = SecureDeviceAddress::FromBytes(bytes1);
	auto sda2 = SecureDeviceAddress::FromBytes(bytes2);

	return sda1->Compare(sda2);
}

SecureDeviceAddress^
SecureDeviceAddress::CreateDedicatedServerAddress(
	String ^ hostnameOrAddress
	)
{
	throw ref new NotImplementedException();
}

SecureDeviceAddress^
SecureDeviceAddress::FromBase64String(
	String ^ base64String
	)
{
	return ref new SecureDeviceAddress(
		::Windows::Networking::XboxLive::XboxLiveDeviceAddress::CreateFromSnapshotBase64(
			base64String
			)
		);
}

SecureDeviceAddress^
SecureDeviceAddress::FromBytes(
	const Array<byte>^ bytes
	)
{
	return ref new SecureDeviceAddress(
		::Windows::Networking::XboxLive::XboxLiveDeviceAddress::CreateFromSnapshotBytes(
			bytes
		)
	);
}

SecureDeviceAddress^
SecureDeviceAddress::GetLocal()
{
	return ref new SecureDeviceAddress(
		::Windows::Networking::XboxLive::XboxLiveDeviceAddress::GetLocal()
	);
}

bool
SecureDeviceAddress::IsLocal::get()
{
	return UnderlyingObject->IsLocal;
}

Networking::NetworkAccessType
SecureDeviceAddress::NetworkAccessType::get()
{
	switch (UnderlyingObject->NetworkAccessKind)
	{
		case ::Windows::Networking::XboxLive::XboxLiveNetworkAccessKind::Moderate: return Networking::NetworkAccessType::Moderate;
		case ::Windows::Networking::XboxLive::XboxLiveNetworkAccessKind::Strict: return Networking::NetworkAccessType::Strict;
		case ::Windows::Networking::XboxLive::XboxLiveNetworkAccessKind::Open: return Networking::NetworkAccessType::Open;
	}

	throw ref new InvalidArgumentException("Unknown NetworkAccessKind");
}

///////////////////////////////////////////////////////////////////////////////
//
//  SecureDeviceAssociation
//

SecureDeviceAssociation::SecureDeviceAssociation(
	::Windows::Networking::XboxLive::XboxLiveEndpointPair^ orig
	)
{
	_theRealThing = orig;
	_eventRegistered = false;
}

EventRegistrationToken
SecureDeviceAssociation::StateChanged::add(
	TypedEventHandler<SecureDeviceAssociation^, SecureDeviceAssociationStateChangedEventArgs^>^ handler
)
{
	if (_eventRegistered == false)
	{
		_theRealThing->StateChanged += ref new TypedEventHandler<::Windows::Networking::XboxLive::XboxLiveEndpointPair ^, ::Windows::Networking::XboxLive::XboxLiveEndpointPairStateChangedEventArgs ^>(this, &SecureDeviceAssociation::OnStateChanged);
		_eventRegistered = true;
	}

	return _stateChanged += handler;
}

void
SecureDeviceAssociation::StateChanged::remove(
	EventRegistrationToken token
)
{
	_stateChanged -= token;
}

void
SecureDeviceAssociation::StateChanged::raise(
	SecureDeviceAssociation^ sender,
	SecureDeviceAssociationStateChangedEventArgs^ args
)
{
	_stateChanged(sender, args);
}

void
SecureDeviceAssociation::OnStateChanged(
	::Windows::Networking::XboxLive::XboxLiveEndpointPair ^sender,
	::Windows::Networking::XboxLive::XboxLiveEndpointPairStateChangedEventArgs ^args
	)
{
	StateChanged(
		ref new SecureDeviceAssociation(sender),
		ref new SecureDeviceAssociationStateChangedEventArgs(args)
	);
}

IAsyncAction^
SecureDeviceAssociation::DestroyAsync()
{
	return UnderlyingObject->DeleteAsync();
}

SecureDeviceAssociation^
SecureDeviceAssociation::GetAssociationByHostNamesAndPorts(
	::Windows::Networking::HostName ^ remoteHostName,
	String ^ remotePort,
	::Windows::Networking::HostName ^ localHostName,
	String ^ localPort
	)
{
	return ref new SecureDeviceAssociation(
		::Windows::Networking::XboxLive::XboxLiveEndpointPair::FindEndpointPairByHostNamesAndPorts(
			localHostName,
			localPort,
			remoteHostName,
			remotePort
			)
		);
}

SecureDeviceAssociation^
SecureDeviceAssociation::GetAssociationBySocketAddressBytes(
	const Array<byte>^ remoteSocketAddressBytes,
	const Array<byte>^ localSocketAddressBytes
	)
{
	return ref new SecureDeviceAssociation(
		::Windows::Networking::XboxLive::XboxLiveEndpointPair::FindEndpointPairBySocketAddressBytes(
			localSocketAddressBytes,
			remoteSocketAddressBytes
			)
		);
}

void
SecureDeviceAssociation::GetLocalSocketAddressBytes(
	WriteOnlyArray<byte>^ socketAddressBytes
	)
{
	UnderlyingObject->GetLocalSocketAddressBytes(socketAddressBytes);
}

void
SecureDeviceAssociation::GetRemoteSocketAddressBytes(
	WriteOnlyArray<byte>^ socketAddressBytes
	)
{
	UnderlyingObject->GetRemoteSocketAddressBytes(socketAddressBytes);
}

::Windows::Networking::HostName^
SecureDeviceAssociation::LocalHostName::get()
{
	return UnderlyingObject->LocalHostName;
}

String^
SecureDeviceAssociation::LocalPort::get()
{
	return UnderlyingObject->LocalPort;
}

::Windows::Networking::HostName^
SecureDeviceAssociation::RemoteHostName::get()
{
	return UnderlyingObject->RemoteHostName;
}

String^
SecureDeviceAssociation::RemotePort::get()
{
	return UnderlyingObject->RemotePort;
}

SecureDeviceAddress^
SecureDeviceAssociation::RemoteSecureDeviceAddress::get()
{
	return ref new SecureDeviceAddress(
		UnderlyingObject->RemoteDeviceAddress
	);
}

SecureDeviceAssociationState
SecureDeviceAssociation::State::get()
{
	switch (UnderlyingObject->State)
	{
		case ::Windows::Networking::XboxLive::XboxLiveEndpointPairState::CreatingInbound: return SecureDeviceAssociationState::CreatingInbound;
		case ::Windows::Networking::XboxLive::XboxLiveEndpointPairState::CreatingOutbound: return SecureDeviceAssociationState::CreatingOutbound;
		case ::Windows::Networking::XboxLive::XboxLiveEndpointPairState::Deleted: return SecureDeviceAssociationState::Destroyed;
		case ::Windows::Networking::XboxLive::XboxLiveEndpointPairState::DeletingLocally: return SecureDeviceAssociationState::DestroyingLocal;
		case ::Windows::Networking::XboxLive::XboxLiveEndpointPairState::Invalid: return SecureDeviceAssociationState::Invalid;
		case ::Windows::Networking::XboxLive::XboxLiveEndpointPairState::Ready: return SecureDeviceAssociationState::Ready;
		case ::Windows::Networking::XboxLive::XboxLiveEndpointPairState::RemoteEndpointTerminating: return SecureDeviceAssociationState::DestroyingRemote;
	}

	throw ref new InvalidArgumentException("Invalid XboxLiveEndpointPairState");
}

SecureDeviceAssociationTemplate^
SecureDeviceAssociation::Template::get()
{
	return ref new SecureDeviceAssociationTemplate(
		UnderlyingObject->Template
	);
}

///////////////////////////////////////////////////////////////////////////////
//
//  SecureDeviceAssociationIncomingEventArgs
//

SecureDeviceAssociationIncomingEventArgs::SecureDeviceAssociationIncomingEventArgs(
	::Windows::Networking::XboxLive::XboxLiveInboundEndpointPairCreatedEventArgs^ args
	)
{
	_theRealThing = args;
}

SecureDeviceAssociation^
SecureDeviceAssociationIncomingEventArgs::Association::get()
{
	return ref new SecureDeviceAssociation(
		UnderlyingObject->EndpointPair
	);
}

///////////////////////////////////////////////////////////////////////////////
//
//  SecureDeviceAssociationStateChangedEventArgs
//

SecureDeviceAssociationStateChangedEventArgs::SecureDeviceAssociationStateChangedEventArgs(
	::Windows::Networking::XboxLive::XboxLiveEndpointPairStateChangedEventArgs^ args
	)
{
	_theRealThing = args;
}

SecureDeviceAssociationState
SecureDeviceAssociationStateChangedEventArgs::NewState::get()
{
	switch (UnderlyingObject->NewState)
	{
		case ::Windows::Networking::XboxLive::XboxLiveEndpointPairState::CreatingInbound: return SecureDeviceAssociationState::CreatingInbound;
		case ::Windows::Networking::XboxLive::XboxLiveEndpointPairState::CreatingOutbound: return SecureDeviceAssociationState::CreatingOutbound;
		case ::Windows::Networking::XboxLive::XboxLiveEndpointPairState::Deleted: return SecureDeviceAssociationState::Destroyed;
		case ::Windows::Networking::XboxLive::XboxLiveEndpointPairState::DeletingLocally: return SecureDeviceAssociationState::DestroyingLocal;
		case ::Windows::Networking::XboxLive::XboxLiveEndpointPairState::Invalid: return SecureDeviceAssociationState::Invalid;
		case ::Windows::Networking::XboxLive::XboxLiveEndpointPairState::Ready: return SecureDeviceAssociationState::Ready;
		case ::Windows::Networking::XboxLive::XboxLiveEndpointPairState::RemoteEndpointTerminating: return SecureDeviceAssociationState::DestroyingRemote;
	}

	throw ref new InvalidArgumentException("Invalid XboxLiveEndpointPairState");
}

SecureDeviceAssociationState
SecureDeviceAssociationStateChangedEventArgs::OldState::get()
{
	switch (UnderlyingObject->OldState)
	{
		case ::Windows::Networking::XboxLive::XboxLiveEndpointPairState::CreatingInbound: return SecureDeviceAssociationState::CreatingInbound;
		case ::Windows::Networking::XboxLive::XboxLiveEndpointPairState::CreatingOutbound: return SecureDeviceAssociationState::CreatingOutbound;
		case ::Windows::Networking::XboxLive::XboxLiveEndpointPairState::Deleted: return SecureDeviceAssociationState::Destroyed;
		case ::Windows::Networking::XboxLive::XboxLiveEndpointPairState::DeletingLocally: return SecureDeviceAssociationState::DestroyingLocal;
		case ::Windows::Networking::XboxLive::XboxLiveEndpointPairState::Invalid: return SecureDeviceAssociationState::Invalid;
		case ::Windows::Networking::XboxLive::XboxLiveEndpointPairState::Ready: return SecureDeviceAssociationState::Ready;
		case ::Windows::Networking::XboxLive::XboxLiveEndpointPairState::RemoteEndpointTerminating: return SecureDeviceAssociationState::DestroyingRemote;
	}

	throw ref new InvalidArgumentException("Invalid XboxLiveEndpointPairState");
}

///////////////////////////////////////////////////////////////////////////////
//
//  SecureDeviceSocketDescription
//

SecureDeviceSocketDescription::SecureDeviceSocketDescription(
	uint16 lower,
	uint16 upper,
	SecureIpProtocol protocol,
	String^ name
	)
{
	_lower = lower;
	_upper = upper;
	_protocol = protocol;
	_name = name;
}

SecureDeviceSocketUsage
SecureDeviceSocketDescription::AllowedUsages::get()
{
	throw ref new NotImplementedException();
}

uint16
SecureDeviceSocketDescription::BoundPortRangeLower::get()
{
	return _lower;
}

uint16
SecureDeviceSocketDescription::BoundPortRangeUpper::get()
{
	return _upper;
}

SecureIpProtocol
SecureDeviceSocketDescription::IpProtocol::get()
{
	return _protocol;
}

String^
SecureDeviceSocketDescription::Name::get()
{
	return _name;
}

///////////////////////////////////////////////////////////////////////////////
//
//  SecureDeviceAssociationTemplate
//

SecureDeviceAssociationTemplate::SecureDeviceAssociationTemplate(
	::Windows::Networking::XboxLive::XboxLiveEndpointPairTemplate^ tmpl
	)
{
	_theRealThing = tmpl;
	_eventRegistered = false;
}

EventRegistrationToken
SecureDeviceAssociationTemplate::AssociationIncoming::add(
	TypedEventHandler<SecureDeviceAssociationTemplate^, SecureDeviceAssociationIncomingEventArgs^>^ handler
	)
{
	if (_eventRegistered == false)
	{
		_theRealThing->InboundEndpointPairCreated += ref new TypedEventHandler<::Windows::Networking::XboxLive::XboxLiveEndpointPairTemplate ^, ::Windows::Networking::XboxLive::XboxLiveInboundEndpointPairCreatedEventArgs ^>(this, &SecureDeviceAssociationTemplate::OnInboundEndpointPairCreated);
		_eventRegistered = true;
	}

	return _associationIncoming += handler;
}

void
SecureDeviceAssociationTemplate::AssociationIncoming::remove(
	EventRegistrationToken token
	)
{
	_associationIncoming -= token;
}

void
SecureDeviceAssociationTemplate::AssociationIncoming::raise(
	SecureDeviceAssociationTemplate^ sender,
	SecureDeviceAssociationIncomingEventArgs^ args
	)
{
	_associationIncoming(sender, args);
}

void
SecureDeviceAssociationTemplate::OnInboundEndpointPairCreated(
	::Windows::Networking::XboxLive::XboxLiveEndpointPairTemplate ^sender,
	::Windows::Networking::XboxLive::XboxLiveInboundEndpointPairCreatedEventArgs ^args
	)
{
	AssociationIncoming(
		ref new SecureDeviceAssociationTemplate(sender),
		ref new SecureDeviceAssociationIncomingEventArgs(args)
	);
}

IAsyncOperation<SecureDeviceAssociation^>^
SecureDeviceAssociationTemplate::CreateAssociationAsync(
	SecureDeviceAddress^ secureDeviceAddress,
	CreateSecureDeviceAssociationBehavior behavior
	)
{
	::Windows::Networking::XboxLive::XboxLiveEndpointPairCreationBehaviors realVal;

	switch (behavior)
	{
		case CreateSecureDeviceAssociationBehavior::Default: realVal = ::Windows::Networking::XboxLive::XboxLiveEndpointPairCreationBehaviors::None; break;
		case CreateSecureDeviceAssociationBehavior::Reevaluate: realVal = ::Windows::Networking::XboxLive::XboxLiveEndpointPairCreationBehaviors::ReevaluatePath; break;
	}

	concurrency::task_completion_event<SecureDeviceAssociation^> tce;

	concurrency::create_task(
		UnderlyingObject->CreateEndpointPairAsync(
			secureDeviceAddress->UnderlyingObject,
			realVal
			)
	).then([this, tce](concurrency::task<::Windows::Networking::XboxLive::XboxLiveEndpointPairCreationResult^> t)
	{
		try
		{
			tce.set(ref new SecureDeviceAssociation(t.get()->EndpointPair));
		}
		catch (Exception^ ex)
		{
			tce.set_exception(std::make_exception_ptr(ex));
		}
	});

	return concurrency::create_async(
		[tce] ()
		{
			return create_task(tce);
		});
}

IAsyncOperation<SecureDeviceAssociation^>^
SecureDeviceAssociationTemplate::CreateAssociationForPortsAsync(
	SecureDeviceAddress^ secureDeviceAddress,
	CreateSecureDeviceAssociationBehavior behavior,
	String ^ initiatorPort,
	String ^ acceptorPort
	)
{
	::Windows::Networking::XboxLive::XboxLiveEndpointPairCreationBehaviors realVal;

	switch (behavior)
	{
		case CreateSecureDeviceAssociationBehavior::Default: realVal = ::Windows::Networking::XboxLive::XboxLiveEndpointPairCreationBehaviors::None; break;
		case CreateSecureDeviceAssociationBehavior::Reevaluate: realVal = ::Windows::Networking::XboxLive::XboxLiveEndpointPairCreationBehaviors::ReevaluatePath; break;
	}

	concurrency::task_completion_event<SecureDeviceAssociation^> tce;

	concurrency::create_task(
		UnderlyingObject->CreateEndpointPairForPortsAsync(
			secureDeviceAddress->UnderlyingObject,
			initiatorPort,
			acceptorPort,
			realVal
			)
	).then([this, tce](concurrency::task<::Windows::Networking::XboxLive::XboxLiveEndpointPairCreationResult^> t)
	{
		try
		{
			tce.set(ref new SecureDeviceAssociation(t.get()->EndpointPair));
		}
		catch (Exception^ ex)
		{
			tce.set_exception(std::make_exception_ptr(ex));
		}
	});

	return concurrency::create_async(
		[tce] ()
		{
			return create_task(tce);
		});
}

IAsyncOperation<::Windows::Storage::Streams::IBuffer^>^
SecureDeviceAssociationTemplate::CreateCertificateRequestAsync(
	String ^ subjectName
	)
{
	throw ref new NotImplementedException();
}

SecureDeviceAssociationTemplate^
SecureDeviceAssociationTemplate::GetTemplateByName(
	String ^ name
	)
{
	return ref new SecureDeviceAssociationTemplate(
		::Windows::Networking::XboxLive::XboxLiveEndpointPairTemplate::GetTemplateByName(name)
		);
}

IAsyncAction^
SecureDeviceAssociationTemplate::InstallCertificateAsync(
	::Windows::Storage::Streams::IBuffer^ certificateBuffer
	)
{
	throw ref new NotImplementedException();
}

SecureDeviceSocketDescription^
SecureDeviceAssociationTemplate::AcceptorSocketDescription::get()
{
	if (_acceptor == nullptr)
	{
		auto protocol = SecureIpProtocol::Udp;

		switch (UnderlyingObject->SocketKind)
		{
			case ::Windows::Networking::XboxLive::XboxLiveSocketKind::Datagram: protocol = SecureIpProtocol::Udp; break;
			case ::Windows::Networking::XboxLive::XboxLiveSocketKind::Stream: protocol = SecureIpProtocol::Tcp; break;
		}

		// NOTE: XboxLiveSocketKind can also be "None" which can't be reflected in SecureIpProtocol so it
		// will default to UDP.  Don't know if this will really come up or not.

		_acceptor = ref new SecureDeviceSocketDescription(
			UnderlyingObject->AcceptorBoundPortRangeLower,
			UnderlyingObject->AcceptorBoundPortRangeUpper,
			protocol,
			UnderlyingObject->Name
			);
	}

	return _acceptor;
}

SecureDeviceAssociationUsage
SecureDeviceAssociationTemplate::AllowedUsages::get()
{
	throw ref new NotImplementedException();
}

IVectorView<SecureDeviceAssociation^>^
SecureDeviceAssociationTemplate::Associations::get()
{
	auto associations = ref new Vector<SecureDeviceAssociation^>();

	for (auto pair : UnderlyingObject->EndpointPairs)
	{
		associations->Append(
			ref new SecureDeviceAssociation(
				pair
				)
			);
	}

	return associations->GetView();
}

SecureDeviceSocketDescription^
SecureDeviceAssociationTemplate::InitiatorSocketDescription::get()
{
	if (_initiator == nullptr)
	{
		auto protocol = SecureIpProtocol::Udp;

		switch (UnderlyingObject->SocketKind)
		{
		case ::Windows::Networking::XboxLive::XboxLiveSocketKind::Datagram: protocol = SecureIpProtocol::Udp; break;
		case ::Windows::Networking::XboxLive::XboxLiveSocketKind::Stream: protocol = SecureIpProtocol::Tcp; break;
		}

		// NOTE: XboxLiveSocketKind can also be "None" which can't be reflected in SecureIpProtocol so it
		// will default to UDP.  Don't know if this will really come up or not.

		_initiator = ref new SecureDeviceSocketDescription(
			UnderlyingObject->InitiatorBoundPortRangeLower,
			UnderlyingObject->InitiatorBoundPortRangeUpper,
			protocol,
			UnderlyingObject->Name
		);
	}

	return _initiator;
}

Networking::MultiplayerSessionRequirement
SecureDeviceAssociationTemplate::MultiplayerSessionRequirement::get()
{
	throw ref new NotImplementedException();
}

String^
SecureDeviceAssociationTemplate::Name::get()
{
	return UnderlyingObject->Name;
}

IVectorView<QualityOfServiceMetric>^
SecureDeviceAssociationTemplate::QualityOfServiceMetricPathPriorities::get()
{
	throw ref new NotImplementedException();
}

String^
SecureDeviceAssociationTemplate::RelyingParty::get()
{
	throw ref new NotImplementedException();
}

IVectorView<SecureDeviceAssociationTemplate^>^
SecureDeviceAssociationTemplate::Templates::get()
{
	auto templates = ref new Vector<SecureDeviceAssociationTemplate^>();

	for (auto temp : ::Windows::Networking::XboxLive::XboxLiveEndpointPairTemplate::Templates)
	{
		templates->Append(
			ref new SecureDeviceAssociationTemplate(
				temp
				)
			);
	}

	return templates->GetView();
}

///////////////////////////////////////////////////////////////////////////////
//
//  QualityOfServiceMeasurement
//

QualityOfServiceMeasurement::QualityOfServiceMeasurement(
	::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMetricResult^ result
)
{
	_theRealThing = result;
}

QualityOfServiceMetric
QualityOfServiceMeasurement::Metric::get()
{
	switch (UnderlyingObject->Metric)
	{
		case ::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMetric::AverageInboundBitsPerSecond: return QualityOfServiceMetric::BandwidthDown;
		case ::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMetric::MaxInboundBitsPerSecond: QualityOfServiceMetric::BandwidthDownMaximum;
		case ::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMetric::MinInboundBitsPerSecond: QualityOfServiceMetric::BandwidthDownMinimum;
		case ::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMetric::AverageOutboundBitsPerSecond: QualityOfServiceMetric::BandwidthUp;
		case ::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMetric::MaxOutboundBitsPerSecond: QualityOfServiceMetric::BandwidthUpMaximum;
		case ::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMetric::MinOutboundBitsPerSecond: QualityOfServiceMetric::BandwidthUpMinimum;
		case ::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMetric::AverageLatencyInMilliseconds: QualityOfServiceMetric::LatencyAverage;
		case ::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMetric::MaxLatencyInMilliseconds: QualityOfServiceMetric::LatencyMaximum;
		case ::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMetric::MinLatencyInMilliseconds: QualityOfServiceMetric::LatencyMinimum;
	}

	throw ref new InvalidArgumentException("Unknown XboxLiveQualityOfServiceMetric");
}

IPropertyValue^
QualityOfServiceMeasurement::MetricValue::get()
{
	return dynamic_cast<IPropertyValue^>(
		PropertyValue::CreateUInt64(UnderlyingObject->Value)
		);
}

Networking::SecureDeviceAddress^
QualityOfServiceMeasurement::SecureDeviceAddress::get()
{
	return ref new Networking::SecureDeviceAddress(
		UnderlyingObject->DeviceAddress
	);
}

QualityOfServiceMeasurementStatus
QualityOfServiceMeasurement::Status::get()
{
	switch (UnderlyingObject->Status)
	{
		case ::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMeasurementStatus::Canceled: return QualityOfServiceMeasurementStatus::Unknown;
		case ::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMeasurementStatus::InProgress: return QualityOfServiceMeasurementStatus::Unknown;
		case ::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMeasurementStatus::InProgressWithProvisionalResults: return QualityOfServiceMeasurementStatus::PartialResults;
		case ::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMeasurementStatus::LocalSystemNotAuthorized: return QualityOfServiceMeasurementStatus::HostUnreachable;
		case ::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMeasurementStatus::NoCompatibleNetworkPaths: return QualityOfServiceMeasurementStatus::HostUnreachable;
		case ::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMeasurementStatus::NoLocalNetworks: return QualityOfServiceMeasurementStatus::HostUnreachable;
		case ::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMeasurementStatus::NotStarted: return QualityOfServiceMeasurementStatus::Unknown;
		case ::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMeasurementStatus::RefusedDueToConfiguration: return QualityOfServiceMeasurementStatus::SecureConnectionAttemptError;
		case ::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMeasurementStatus::RemoteSystemNotAuthorized: return QualityOfServiceMeasurementStatus::SecureConnectionAttemptError;
		case ::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMeasurementStatus::Succeeded: return QualityOfServiceMeasurementStatus::Success;
		case ::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMeasurementStatus::TimedOut: return QualityOfServiceMeasurementStatus::SecureConnectionAttemptTimedOut;
		case ::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMeasurementStatus::UnexpectedInternalError: return QualityOfServiceMeasurementStatus::Unknown;
	}

	throw ref new InvalidArgumentException("Unknown XboxLiveQualityOfServiceMeasurementStatus");
}

///////////////////////////////////////////////////////////////////////////////
//
//  MeasureQualityOfServiceResult
//

IVectorView<QualityOfServiceMeasurement^>^
MeasureQualityOfServiceResult::Measurements::get()
{
	auto measurements = ref new Vector<QualityOfServiceMeasurement^>();

	for (auto result : _source->MetricResults)
	{
		measurements->Append(ref new QualityOfServiceMeasurement(result));
	}

	return measurements->GetView();
}

QualityOfServiceMeasurement^
MeasureQualityOfServiceResult::GetMeasurement(
	SecureDeviceAddress^ address,
	QualityOfServiceMetric metric
	)
{
	throw ref new NotImplementedException();
}

IVectorView<QualityOfServiceMeasurement^>^
MeasureQualityOfServiceResult::GetMeasurementsForDevice(
	SecureDeviceAddress^ address
	)
{
	throw ref new NotImplementedException();
}

IVectorView<QualityOfServiceMeasurement^>^
MeasureQualityOfServiceResult::GetMeasurementsForMetric(
	QualityOfServiceMetric metric
	)
{
	throw ref new NotImplementedException();
}

MeasureQualityOfServiceResult::MeasureQualityOfServiceResult(
	::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMeasurement^ source
	)
{
	_source = source;
}

///////////////////////////////////////////////////////////////////////////////
//
//  QualityOfService
//

IAsyncOperation<MeasureQualityOfServiceResult^>^
QualityOfService::MeasureQualityOfServiceAsync(
	IIterable<SecureDeviceAddress^>^ addresses,
	IIterable<QualityOfServiceMetric>^ metrics,
	uint32 timeoutInMilliseconds,
	uint32 numberOfProbes
	)
{
	auto qosMeasurement = ref new ::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMeasurement();

	qosMeasurement->NumberOfProbesToAttempt = numberOfProbes;
	qosMeasurement->TimeoutInMilliseconds = timeoutInMilliseconds;

	for (auto metric : metrics)
	{
		::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMetric qosMetric;

		switch (metric)
		{
			case QualityOfServiceMetric::BandwidthDown: qosMetric = ::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMetric::AverageInboundBitsPerSecond; break;
			case QualityOfServiceMetric::BandwidthDownMaximum: qosMetric = ::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMetric::MaxInboundBitsPerSecond; break;
			case QualityOfServiceMetric::BandwidthDownMinimum: qosMetric = ::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMetric::MinInboundBitsPerSecond; break;
			case QualityOfServiceMetric::BandwidthUp: qosMetric = ::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMetric::AverageOutboundBitsPerSecond; break;
			case QualityOfServiceMetric::BandwidthUpMaximum: qosMetric = ::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMetric::MaxOutboundBitsPerSecond; break;
			case QualityOfServiceMetric::BandwidthUpMinimum: qosMetric = ::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMetric::MinOutboundBitsPerSecond; break;
			case QualityOfServiceMetric::LatencyAverage: qosMetric = ::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMetric::AverageLatencyInMilliseconds; break;
			case QualityOfServiceMetric::LatencyMaximum: qosMetric = ::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMetric::MaxLatencyInMilliseconds; break;
			case QualityOfServiceMetric::LatencyMinimum: qosMetric = ::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMetric::MinLatencyInMilliseconds; break;
			case QualityOfServiceMetric::Invalid:
			case QualityOfServiceMetric::PrivatePayload:
				throw ref new InvalidArgumentException("Unsupported Metric type");
		}

		qosMeasurement->Metrics->Append(qosMetric);
	}

	for (auto address : addresses)
	{
		qosMeasurement->DeviceAddresses->Append(address->UnderlyingObject);
	}

	concurrency::task_completion_event<MeasureQualityOfServiceResult^> tce;

	concurrency::create_task(
		qosMeasurement->MeasureAsync()
		)
	.then([tce, qosMeasurement](concurrency::task<void> t)
	{
		try
		{
			t.get();
			tce.set(ref new MeasureQualityOfServiceResult(qosMeasurement));
		}
		catch (Exception^ ex)
		{
			tce.set_exception(std::make_exception_ptr(ex));
		}
	});

	return concurrency::create_async(
		[tce] ()
		{
			return create_task(tce);
		});
}

void
QualityOfService::ClearPrivatePayload()
{
	throw ref new NotImplementedException();
}

void
QualityOfService::PublishPrivatePayload(
	const Array<unsigned char>^ payload
	)
{
	throw ref new NotImplementedException();
}

void
QualityOfService::ConfigureQualityOfService(
	uint32 maxSimultaneousProbeConnections,
	bool constrainSystemBandwidthUp,
	bool constrainSystemBandwidthDown
	)
{
	throw ref new NotImplementedException();
}

}}}}
