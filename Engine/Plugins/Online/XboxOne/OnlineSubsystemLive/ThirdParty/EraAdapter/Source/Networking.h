//-----------------------------------------------------------------------------
//				Copyright (C) Microsoft. All rights reserved.
//-----------------------------------------------------------------------------
#pragma once

namespace EraAdapter {
namespace Windows {
namespace Xbox {
namespace Networking {

///////////////////////////////////////////////////////////////////////////////
//
//  Enumerations
//

public enum class CreateSecureDeviceAssociationBehavior
{
	Default							= 0x0000,
	Reevaluate						= 0x0001
};

public enum class SecureDeviceAssociationUsage
{
	Default							= 0x0000,
	InitiateFromMicrosoftConsole	= 0x0001,
	InitiateFromXboxLiveCompute		= 0x0002,
	InitiateFromOtherDevice			= 0x0004,
	InitiateFromWindowsDesktop		= 0x0008,
	AcceptOnMicrosoftConsole		= 0x0100,
	AcceptOnXboxLiveCompute			= 0x0200,
	AcceptOnOtherDevice				= 0x0400,
	AcceptOnWindowsDesktop			= 0x0800
};

public enum class SecureDeviceSocketUsage
{
	Initiate						= 0x00001,
	Accept							= 0x00002,
	SendChat						= 0x00100,
	SendGameData					= 0x00200,
	SendDebug						= 0x00400,
	SendInsecure					= 0x00800,
	ReceiveChat						= 0x10000,
	ReceiveGameData					= 0x20000,
	ReceiveDebug					= 0x40000,
	ReceiveInsecure					= 0x80000
};

public enum class SecureDeviceAssociationState
{
	Invalid							= 0x0000,
	CreatingOutbound				= 0x0001,
	CreatingInbound					= 0x0002,
	Ready							= 0x0003,
	DestroyingLocal					= 0x0004,
	DestroyingRemote				= 0x0005,
	Destroyed						= 0x0006
};

public enum class NetworkAccessType
{
	Open							= 0x0000,
	Moderate						= 0x0001,
	Strict							= 0x0002
};

public enum class SecureIpProtocol
{
	Udp								= 0x0001,
	Tcp								= 0x0002
};

public enum class MultiplayerSessionRequirement
{
	Required						= 0x0000,
	Optional						= 0x0001,
	None							= 0x0002
};

public enum class QualityOfServiceMetric
{
	Invalid							= 0x0000,
	PrivatePayload					= 0x0001,
	LatencyAverage					= 0x0002,
	LatencyMaximum					= 0x0003,
	LatencyMinimum					= 0x0004,
	BandwidthUp						= 0x0005,
	BandwidthUpMaximum				= 0x0006,
	BandwidthUpMinimum				= 0x0007,
	BandwidthDown					= 0x0008,
	BandwidthDownMaximum			= 0x0009,
	BandwidthDownMinimum			= 0x000A
};

public enum class QualityOfServiceMeasurementStatus
{
	Unknown							= 0x0000,
	Success							= 0x0001,
	PartialResults					= 0x0002,
	HostUnreachable					= 0x0003,
	MeasurementTimedOut				= 0x0004,
	SecureConnectionAttemptTimedOut	= 0x0005,
	SecureConnectionAttemptError	= 0x0006
};

///////////////////////////////////////////////////////////////////////////////
//
//  SecureDeviceAssociationIncomingEventArgs
//

ref class SecureDeviceAssociation;

public ref class SecureDeviceAssociationIncomingEventArgs sealed
{
public:
	property SecureDeviceAssociation^ Association
	{
		SecureDeviceAssociation^ get();
	}

internal:
	SecureDeviceAssociationIncomingEventArgs(
		::Windows::Networking::XboxLive::XboxLiveInboundEndpointPairCreatedEventArgs^ args
		);

	property ::Windows::Networking::XboxLive::XboxLiveInboundEndpointPairCreatedEventArgs^ UnderlyingObject
	{
		::Windows::Networking::XboxLive::XboxLiveInboundEndpointPairCreatedEventArgs^ get()
		{
			return _theRealThing;
		}
	}

private:
	::Windows::Networking::XboxLive::XboxLiveInboundEndpointPairCreatedEventArgs^ _theRealThing;
};

///////////////////////////////////////////////////////////////////////////////
//
//  SecureDeviceAssociationStateChangedEventArgs
//

public ref class SecureDeviceAssociationStateChangedEventArgs sealed
{
public:
	property SecureDeviceAssociationState NewState
	{
		SecureDeviceAssociationState get();
	}

	property SecureDeviceAssociationState OldState
	{
		SecureDeviceAssociationState get();
	}

internal:
	SecureDeviceAssociationStateChangedEventArgs(
		::Windows::Networking::XboxLive::XboxLiveEndpointPairStateChangedEventArgs^ args
	);

	property ::Windows::Networking::XboxLive::XboxLiveEndpointPairStateChangedEventArgs^ UnderlyingObject
	{
		::Windows::Networking::XboxLive::XboxLiveEndpointPairStateChangedEventArgs^ get()
		{
			return _theRealThing;
		}
	}

private:
	::Windows::Networking::XboxLive::XboxLiveEndpointPairStateChangedEventArgs^ _theRealThing;
};

///////////////////////////////////////////////////////////////////////////////
//
//  SecureDeviceAddress
//
ref class SecureDeviceAddress;

public interface struct ISecureDeviceAddressEvents
{
	event ::Windows::Foundation::TypedEventHandler<SecureDeviceAddress^, ::Platform::Object^>^ BufferChanged;
};

public ref class SecureDeviceAddress sealed :
	[::Windows::Foundation::Metadata::DefaultAttribute] ISecureDeviceAddressEvents
{
public:
	int
	Compare(
		SecureDeviceAddress^ secureDeviceAddress
		);
	
	Platform::String^
	GetBase64String(
		);
	
	::Windows::Storage::Streams::IBuffer^
	GetBuffer(
		);

	property bool IsLocal
	{
		bool get();
	}

	property Networking::NetworkAccessType NetworkAccessType
	{ 
		Networking::NetworkAccessType get();
	}

	virtual event ::Windows::Foundation::TypedEventHandler<SecureDeviceAddress^, ::Platform::Object^>^ BufferChanged
	{
		::Windows::Foundation::EventRegistrationToken add(::Windows::Foundation::TypedEventHandler<SecureDeviceAddress^, ::Platform::Object^>^ handler);
		void remove(::Windows::Foundation::EventRegistrationToken token);
	internal:
		void raise(SecureDeviceAddress^ sender, ::Platform::Object^ args);
	}

	static int
	CompareBuffers(
		::Windows::Storage::Streams::IBuffer^ buffer1,
		::Windows::Storage::Streams::IBuffer^ buffer2
		);

	static int
	CompareBytes(
		const ::Platform::Array<byte>^ bytes1,
		const ::Platform::Array<byte>^ bytes2
		);

	static SecureDeviceAddress^
	CreateDedicatedServerAddress(
		::Platform::String^ hostnameOrAddress
		);

	static SecureDeviceAddress^
	FromBase64String(
		::Platform::String^ base64String
		);

	static SecureDeviceAddress^
	FromBytes(
		const ::Platform::Array<byte>^
		);

	static SecureDeviceAddress^
	GetLocal(
		);

internal:
	SecureDeviceAddress(
		::Windows::Networking::XboxLive::XboxLiveDeviceAddress^ sda
		);

	SecureDeviceAddress(
		::Windows::Storage::Streams::IBuffer^ buffer
		);

	property ::Windows::Networking::XboxLive::XboxLiveDeviceAddress^ UnderlyingObject
	{
		::Windows::Networking::XboxLive::XboxLiveDeviceAddress^ get()
		{
			return _theRealThing;
		}
	}

private:
	void
	OnSnapshotChanged(
		::Windows::Networking::XboxLive::XboxLiveDeviceAddress ^sender,
		::Platform::Object ^args
		);

	event ::Windows::Foundation::TypedEventHandler<SecureDeviceAddress^, ::Platform::Object^>^ _bufferChanged;

	::Windows::Networking::XboxLive::XboxLiveDeviceAddress^ _theRealThing;
	bool _eventRegistered;
};

///////////////////////////////////////////////////////////////////////////////
//
//  SecureDeviceAssociation
//

ref class SecureDeviceAssociation;
ref class SecureDeviceAssociationTemplate;
ref class SecureDeviceAssociationStateChangedEventArgs;

public interface struct ISecureDeviceAssociationEvents
{
	event ::Windows::Foundation::TypedEventHandler<SecureDeviceAssociation^, SecureDeviceAssociationStateChangedEventArgs^>^ StateChanged;
};

public ref class SecureDeviceAssociation sealed :
	[::Windows::Foundation::Metadata::DefaultAttribute] ISecureDeviceAssociationEvents
{
public:
	::Windows::Foundation::IAsyncAction^
	DestroyAsync(
		);

	static SecureDeviceAssociation^
	GetAssociationByHostNamesAndPorts(
		::Windows::Networking::HostName^ remoteHostName,
		::Platform::String^ remotePort,
		::Windows::Networking::HostName^ localHostName,
		::Platform::String^ localPort
		);

	static SecureDeviceAssociation^
	GetAssociationBySocketAddressBytes(
		const ::Platform::Array<byte>^ remoteSocketAddressBytes,
		const ::Platform::Array<byte>^ localSocketAddressBytes
		);

	void
	GetLocalSocketAddressBytes(
		::Platform::WriteOnlyArray<byte>^ socketAddressBytes
		);

	void
	GetRemoteSocketAddressBytes(
		::Platform::WriteOnlyArray<byte>^ socketAddressBytes
		);

	property ::Windows::Networking::HostName^ LocalHostName
	{
		::Windows::Networking::HostName^ get();
	}

	property ::Platform::String^ LocalPort
	{
		::Platform::String^ get();
	}

	property ::Windows::Networking::HostName^ RemoteHostName
	{
		::Windows::Networking::HostName^ get();
	}

	property ::Platform::String^ RemotePort
	{
		::Platform::String^ get();
	}

	property SecureDeviceAddress^ RemoteSecureDeviceAddress
	{
		SecureDeviceAddress^ get();
	}

	property SecureDeviceAssociationState State
	{
		SecureDeviceAssociationState get();
	}

	property SecureDeviceAssociationTemplate^ Template
	{
		SecureDeviceAssociationTemplate^ get();
	}

	virtual event ::Windows::Foundation::TypedEventHandler<SecureDeviceAssociation^, SecureDeviceAssociationStateChangedEventArgs^>^ StateChanged
	{
		::Windows::Foundation::EventRegistrationToken add(::Windows::Foundation::TypedEventHandler<SecureDeviceAssociation^, SecureDeviceAssociationStateChangedEventArgs^>^ handler);
		void remove(::Windows::Foundation::EventRegistrationToken token);
	internal:
		void raise(SecureDeviceAssociation^ sender, SecureDeviceAssociationStateChangedEventArgs^ args);
	}

internal:
	SecureDeviceAssociation(
		::Windows::Networking::XboxLive::XboxLiveEndpointPair^ sda
		);

	property ::Windows::Networking::XboxLive::XboxLiveEndpointPair^ UnderlyingObject
	{
		::Windows::Networking::XboxLive::XboxLiveEndpointPair^ get()
		{
			return _theRealThing;
		}
	}

private:
	void
	OnStateChanged(
		::Windows::Networking::XboxLive::XboxLiveEndpointPair ^sender,
		::Windows::Networking::XboxLive::XboxLiveEndpointPairStateChangedEventArgs ^args
	);

	event ::Windows::Foundation::TypedEventHandler<SecureDeviceAssociation^, SecureDeviceAssociationStateChangedEventArgs^>^ _stateChanged;
	
	::Windows::Networking::XboxLive::XboxLiveEndpointPair^ _theRealThing;
	bool _eventRegistered;
};

///////////////////////////////////////////////////////////////////////////////
//
//  SecureDeviceSocketDescription
//

public ref class SecureDeviceSocketDescription sealed
{
public:
	property SecureDeviceSocketUsage AllowedUsages
	{
		SecureDeviceSocketUsage get();
	}

	property uint16 BoundPortRangeLower
	{
		uint16 get();
	}

	property uint16 BoundPortRangeUpper
	{
		uint16 get();
	}

	property SecureIpProtocol IpProtocol
	{
		SecureIpProtocol get();
	}

	property ::Platform::String^ Name
	{
		::Platform::String^ get();
	}

internal:
	SecureDeviceSocketDescription(
		uint16 lower,
		uint16 upper,
		SecureIpProtocol protocol,
		::Platform::String^ name
		);

private:
	uint16 _lower;
	uint16 _upper;
	SecureIpProtocol _protocol;
	::Platform::String^ _name;
};

///////////////////////////////////////////////////////////////////////////////
//
//  SecureDeviceAssociationTemplate
//

ref class SecureDeviceAssociationTemplate;
ref class SecureDeviceAssociationIncomingEventArgs;

public interface struct ISecureDeviceAssociationTemplateEvents
{
	event ::Windows::Foundation::TypedEventHandler<SecureDeviceAssociationTemplate^, SecureDeviceAssociationIncomingEventArgs^>^ AssociationIncoming;
};

public ref class SecureDeviceAssociationTemplate sealed :
	[::Windows::Foundation::Metadata::DefaultAttribute] ISecureDeviceAssociationTemplateEvents
{
public:
	::Windows::Foundation::IAsyncOperation<SecureDeviceAssociation^>^
	CreateAssociationAsync(
		SecureDeviceAddress^ secureDeviceAddress,
		CreateSecureDeviceAssociationBehavior behavior
		);

	::Windows::Foundation::IAsyncOperation<SecureDeviceAssociation^>^
	CreateAssociationForPortsAsync(
		SecureDeviceAddress^ secureDeviceAddress,
		CreateSecureDeviceAssociationBehavior behavior,
		::Platform::String^ initiatorPort,
		::Platform::String^ acceptorPort
		);

	::Windows::Foundation::IAsyncOperation<::Windows::Storage::Streams::IBuffer^>^
	CreateCertificateRequestAsync(
		::Platform::String^ subjectName
		);

	static SecureDeviceAssociationTemplate^
	GetTemplateByName(
		::Platform::String^ name
		);

	::Windows::Foundation::IAsyncAction^
	InstallCertificateAsync(
		::Windows::Storage::Streams::IBuffer^ certificateBuffer
		);

	property SecureDeviceSocketDescription^ AcceptorSocketDescription
	{
		SecureDeviceSocketDescription^ get();
	}

	property SecureDeviceAssociationUsage AllowedUsages
	{
		SecureDeviceAssociationUsage get();
	}

	property ::Windows::Foundation::Collections::IVectorView<SecureDeviceAssociation^>^ Associations
	{
		::Windows::Foundation::Collections::IVectorView<SecureDeviceAssociation^>^ get();
	}

	property SecureDeviceSocketDescription^ InitiatorSocketDescription
	{
		SecureDeviceSocketDescription^ get();
	}

	property Networking::MultiplayerSessionRequirement MultiplayerSessionRequirement
	{
		Networking::MultiplayerSessionRequirement get();
	}

	property ::Platform::String^ Name
	{
		::Platform::String^ get();
	}

	property ::Windows::Foundation::Collections::IVectorView<QualityOfServiceMetric>^ QualityOfServiceMetricPathPriorities
	{
		::Windows::Foundation::Collections::IVectorView<QualityOfServiceMetric>^ get();
	}

	property ::Platform::String^ RelyingParty
	{
		::Platform::String^ get();
	}

	static property ::Windows::Foundation::Collections::IVectorView<SecureDeviceAssociationTemplate^>^ Templates
	{
		::Windows::Foundation::Collections::IVectorView<SecureDeviceAssociationTemplate^>^ get();
	}

	virtual event ::Windows::Foundation::TypedEventHandler<SecureDeviceAssociationTemplate^, SecureDeviceAssociationIncomingEventArgs^>^ AssociationIncoming
	{
		::Windows::Foundation::EventRegistrationToken add(::Windows::Foundation::TypedEventHandler<SecureDeviceAssociationTemplate^, SecureDeviceAssociationIncomingEventArgs^>^ handler);
		void remove(::Windows::Foundation::EventRegistrationToken token);
	internal:
		void raise(SecureDeviceAssociationTemplate^ sender, SecureDeviceAssociationIncomingEventArgs^ args);
	}

internal:
	SecureDeviceAssociationTemplate(
		::Windows::Networking::XboxLive::XboxLiveEndpointPairTemplate^ tmpl
		);

	property ::Windows::Networking::XboxLive::XboxLiveEndpointPairTemplate^ UnderlyingObject
	{
		::Windows::Networking::XboxLive::XboxLiveEndpointPairTemplate^ get()
		{
			return _theRealThing;
		}
	}

private:
	void
	OnInboundEndpointPairCreated(
		::Windows::Networking::XboxLive::XboxLiveEndpointPairTemplate ^sender,
		::Windows::Networking::XboxLive::XboxLiveInboundEndpointPairCreatedEventArgs ^args
	);

	event ::Windows::Foundation::TypedEventHandler<SecureDeviceAssociationTemplate^, SecureDeviceAssociationIncomingEventArgs^>^ _associationIncoming;

	::Windows::Networking::XboxLive::XboxLiveEndpointPairTemplate^ _theRealThing;
	SecureDeviceSocketDescription^ _acceptor;
	SecureDeviceSocketDescription^ _initiator;
	bool _eventRegistered;
};

///////////////////////////////////////////////////////////////////////////////
//
//  QualityOfServiceMeasurement
//

public ref class QualityOfServiceMeasurement sealed
{
public:
	property QualityOfServiceMetric Metric
	{
		QualityOfServiceMetric get();
	}

	property ::Windows::Foundation::IPropertyValue^ MetricValue
	{
		::Windows::Foundation::IPropertyValue^ get();
	}

	property Networking::SecureDeviceAddress^ SecureDeviceAddress
	{
		Networking::SecureDeviceAddress^ get();
	}

	property QualityOfServiceMeasurementStatus Status
	{
		QualityOfServiceMeasurementStatus get();
	}

internal:
	QualityOfServiceMeasurement(
		::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMetricResult^ result
		);

	property ::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMetricResult^ UnderlyingObject
	{
		::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMetricResult^ get()
		{
			return _theRealThing;
		}
	}

private:
	::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMetricResult^ _theRealThing;
};

///////////////////////////////////////////////////////////////////////////////
//
//  MeasureQualityOfServiceResult
//

public ref class MeasureQualityOfServiceResult sealed
{
public:
	property ::Windows::Foundation::Collections::IVectorView<QualityOfServiceMeasurement^>^ Measurements
	{
		::Windows::Foundation::Collections::IVectorView<QualityOfServiceMeasurement^>^ get();
	}

	QualityOfServiceMeasurement^
	GetMeasurement(
		SecureDeviceAddress^ address,
		QualityOfServiceMetric metric
	);

	::Windows::Foundation::Collections::IVectorView<QualityOfServiceMeasurement^>^
	GetMeasurementsForDevice(
		SecureDeviceAddress^ address
	);

	::Windows::Foundation::Collections::IVectorView<QualityOfServiceMeasurement^>^
	GetMeasurementsForMetric(
		QualityOfServiceMetric metric
	);

internal:
	MeasureQualityOfServiceResult(
		::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMeasurement^ source
	);

private:
	::Windows::Networking::XboxLive::XboxLiveQualityOfServiceMeasurement^ _source;
};

///////////////////////////////////////////////////////////////////////////////
//
//  QualityOfService
//

public ref class QualityOfService sealed
{
public:
	static ::Windows::Foundation::IAsyncOperation<MeasureQualityOfServiceResult^>^
	MeasureQualityOfServiceAsync(
		::Windows::Foundation::Collections::IIterable<SecureDeviceAddress^>^ addresses,
		::Windows::Foundation::Collections::IIterable<QualityOfServiceMetric>^ metrics,
		uint32 timeoutInMilliseconds,
		uint32 numberOfProbes
	);

	static void
	ClearPrivatePayload();

	static void
	PublishPrivatePayload(
		const ::Platform::Array<unsigned char>^ payload
	);

	static void
	ConfigureQualityOfService(
		uint32 maxSimultaneousProbeConnections,
		bool constrainSystemBandwidthUp,
		bool constrainSystemBandwidthDown
	);
};

}}}}