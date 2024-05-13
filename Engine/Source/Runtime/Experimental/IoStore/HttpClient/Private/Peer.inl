// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

namespace UE::IoStore::HTTP
{

////////////////////////////////////////////////////////////////////////////////
static_assert(OPENSSL_VERSION_NUMBER >= 0x10100000L, "version supporting autoinit required");



////////////////////////////////////////////////////////////////////////////////
class FSslContext
{
public:
				FSslContext(const char* InHostName, const FPemCert& CertPin={});
				~FSslContext()					{ SSL_CTX_free(Context); }
				operator SSL_CTX* () const		{ return Context; }
	const char*	GetHostName() const				{ return HostName; }
	bool		AddCert(const FPemCert& Cert);

private:
	SSL_CTX*	Context;
	const char*	HostName;
};

////////////////////////////////////////////////////////////////////////////////
FSslContext::FSslContext(const char* InHostName, const FPemCert& CertPin)
: HostName(InHostName)
{
	Context = SSL_CTX_new(TLS_client_method());

	uint32 ProtoFlags = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3;
	SSL_CTX_set_options(Context, ProtoFlags);

	if (CertPin.GetSize() == 0)
	{
#if WITH_SSL
		const ISslCertificateManager& CertManager = FSslModule::Get().GetCertificateManager();
		CertManager.AddCertificatesToSslContext(Context);
#endif
		return;
	}

	AddCert(CertPin);
}

////////////////////////////////////////////////////////////////////////////////
bool FSslContext::AddCert(const FPemCert& Cert)
{
	const void* Data = Cert.GetData();
	uint32 Size = uint32(Cert.GetSize());
	if (Data == nullptr || Size == 0)
	{
		return false;
	}

	BIO* Bio = BIO_new_mem_buf(Data, Size);
	X509* FiveOhNine = PEM_read_bio_X509(Bio, nullptr, 0, nullptr);
	check(FiveOhNine != nullptr);

	X509_STORE* Store = SSL_CTX_get_cert_store(Context);
	int32 Result = X509_STORE_add_cert(Store, FiveOhNine);
	return Result == 1;
}



////////////////////////////////////////////////////////////////////////////////
class FPeer
{
public:
				FPeer() = default;
				FPeer(FSocket InSocket);
	FOutcome	Send(const char* Data, int32 Size)	{ return Socket.Send(Data, Size); }
	FOutcome	Recv(char* Out, int32 MaxSize)		{ return Socket.Recv(Out, MaxSize); }
	bool		IsValid() const						{ return Socket.IsValid(); }

private:
	FSocket		Socket;



	// TODO: !!!!!! remove this !!!!!
public:
	explicit operator const FSocket& () const { return Socket; }
	explicit operator const FSocket* () const { return &Socket; }
};

////////////////////////////////////////////////////////////////////////////////
FPeer::FPeer(FSocket InSocket)
: Socket(MoveTemp(InSocket))
{
}



////////////////////////////////////////////////////////////////////////////////
class FTlsPeer
	: public FPeer
{
public:
				FTlsPeer()								= default;
				~FTlsPeer();
				FTlsPeer(FTlsPeer&& Rhs)				{ Move(MoveTemp(Rhs)); }
				FTlsPeer& operator = (FTlsPeer&& Rhs)	{ return Move(MoveTemp(Rhs)); }
				FTlsPeer(FSocket InSocket, const FSslContext* Context=nullptr);
	FTlsPeer&	Move(FTlsPeer&& Rhs);
	FOutcome	Handshake();
	FOutcome	Send(const char* Data, int32 Size);
	FOutcome	Recv(char* Out, int32 MaxSize);

protected:
	SSL*		Ssl = nullptr;

private:
	FOutcome	GetOutcome(int32 SslResult, const char* Message="tls error") const;
	int32		BioWrite(const char* Data, size_t Size, size_t* BytesWritten, BIO* Bio);
	int32		BioRead(char* Data, size_t Size, size_t* BytesRead, BIO* Bio);
	long		BioControl(int Cmd, long, void*, BIO*);
};

////////////////////////////////////////////////////////////////////////////////
FTlsPeer::FTlsPeer(FSocket InSocket, const FSslContext* Context)
: FPeer(MoveTemp(InSocket))
{
	if (Context == nullptr)
	{
		return;
	}

	static BIO_METHOD* BioMethod = nullptr;
	if (BioMethod == nullptr)
	{
		int32 BioId = BIO_get_new_index() | BIO_TYPE_SOURCE_SINK;
		BioMethod = BIO_meth_new(BioId, "IasBIO");

#define METH_THUNK(x_, y_)\
		x_(BioMethod, [] <typename... T> (BIO* b, T... t) { \
			return (decltype(this)(BIO_get_data(b)))->y_(Forward<T>(t)..., b); \
		})
		METH_THUNK(BIO_meth_set_write_ex,	BioWrite);
		METH_THUNK(BIO_meth_set_read_ex,	BioRead);
		METH_THUNK(BIO_meth_set_ctrl,		BioControl);
#undef METH_THUNK
	}

	BIO* Bio = BIO_new(BioMethod);
	BIO_set_data(Bio, this);

	// SSL_MODE_ENABLE_PARTIAL_WRITE ??!!!

	Ssl = SSL_new(*Context);
	SSL_set_connect_state(Ssl);
	SSL_set0_rbio(Ssl, Bio);
	SSL_set0_wbio(Ssl, Bio);
	BIO_up_ref(Bio);

	if (const char* HostName = Context->GetHostName(); HostName != nullptr)
	{
		SSL_set_tlsext_host_name(Ssl, HostName);
	}
}

////////////////////////////////////////////////////////////////////////////////
FTlsPeer& FTlsPeer::Move(FTlsPeer&& Rhs)
{
	FPeer::operator = (MoveTemp(Rhs));

	Swap(Ssl, Rhs.Ssl);

	auto PatchBio = [] (FTlsPeer& Peer)
	{
		if (Peer.Ssl != nullptr)
		{
			BIO* Bio = SSL_get_rbio(Peer.Ssl);
			check(Bio != nullptr);
			BIO_set_data(Bio, &Peer);
		}
	};
	PatchBio(*this);
	PatchBio(Rhs);

	return *this;
}

////////////////////////////////////////////////////////////////////////////////
FTlsPeer::~FTlsPeer()
{
	if (Ssl != nullptr)
	{
		SSL_free(Ssl);
	}
}

////////////////////////////////////////////////////////////////////////////////
FOutcome FTlsPeer::Handshake()
{
	if (Ssl == nullptr)
	{
		return FOutcome::Ok();
	}

	int32 Result = SSL_do_handshake(Ssl);
	if (Result == 0) return FOutcome::Error("unsuccessful tls handshake");
	if (Result != 1) return GetOutcome(Result, "tls handshake error");

	if (Result = SSL_get_verify_result(Ssl); Result != X509_V_OK)
	{
		return FOutcome::Error("x509 verification error", Result);
	}

	return FOutcome::Ok();
}

////////////////////////////////////////////////////////////////////////////////
FOutcome FTlsPeer::Send(const char* Data, int32 Size)
{
	if (Ssl == nullptr)
	{
		return FPeer::Send(Data, Size);
	}

	int32 Result = SSL_write(Ssl, Data, Size);
	return (Result > 0) ? FOutcome::Ok(Result) : GetOutcome(Result);
}

////////////////////////////////////////////////////////////////////////////////
FOutcome FTlsPeer::Recv(char* Out, int32 MaxSize)
{
	if (Ssl == nullptr)
	{
		return FPeer::Recv(Out, MaxSize);
	}

	int32 Result = SSL_read(Ssl, Out, MaxSize);
	return (Result > 0) ? FOutcome::Ok(Result) : GetOutcome(Result);
}

////////////////////////////////////////////////////////////////////////////////
FOutcome FTlsPeer::GetOutcome(int32 SslResult, const char* Message) const
{
	int32 Error = SSL_get_error(Ssl, SslResult);
	if (Error != SSL_ERROR_WANT_READ && Error != SSL_ERROR_WANT_WRITE)
	{
		return FOutcome::Error(Message, Error);
	}
	return FOutcome::Waiting();
}

////////////////////////////////////////////////////////////////////////////////
int32 FTlsPeer::BioWrite(const char* Data, size_t Size, size_t* BytesWritten, BIO* Bio)
{
	*BytesWritten = 0;
	BIO_clear_retry_flags(Bio);

	FOutcome Outcome = FPeer::Send(Data, Size);
	if (Outcome.IsWaiting())
	{
		BIO_set_retry_write(Bio);
		return 0;
	}

	if (Outcome.IsError())
	{
		return -1;
	}

	*BytesWritten = Outcome.GetResult();
	return 1;
}

////////////////////////////////////////////////////////////////////////////////
int32 FTlsPeer::BioRead(char* Data, size_t Size, size_t* BytesRead, BIO* Bio)
{
	*BytesRead = 0;
	BIO_clear_retry_flags(Bio);

	FOutcome Outcome = FPeer::Recv(Data, Size);
	if (Outcome.IsWaiting())
	{
		BIO_set_retry_read(Bio);
		return 0;
	}

	if (Outcome.IsError())
	{
		return -1;
	}

	*BytesRead = Outcome.GetResult();
	return 1;
}

////////////////////////////////////////////////////////////////////////////////
long FTlsPeer::BioControl(int Cmd, long, void*, BIO*)
{
	return (Cmd == BIO_CTRL_FLUSH) ? 1 : 0;
}



////////////////////////////////////////////////////////////////////////////////
class FHttpPeer
	: public FTlsPeer
{
public:
				FHttpPeer() = default;
				FHttpPeer(FSocket InSocket, FSslContext* Context=nullptr);
	FOutcome	Handshake();

private:
	void		AssignProto();
	int32		Proto = 0;
};

////////////////////////////////////////////////////////////////////////////////
FHttpPeer::FHttpPeer(FSocket InSocket, FSslContext* Context)
: FTlsPeer(MoveTemp(InSocket), Context)
{
	if (Ssl == nullptr)
	{
		return;
	}

	static const uint8 AlpnProtos[] =
		"\x08" "http/1.1"
	//	"\x02" "h2"
		;
	SSL_set_alpn_protos(Ssl, AlpnProtos, sizeof(AlpnProtos) - 1);
}

////////////////////////////////////////////////////////////////////////////////
FOutcome FHttpPeer::Handshake()
{
	FOutcome Outcome = FTlsPeer::Handshake();
	if (Outcome.IsOk())
	{
		AssignProto();
	}

	return Outcome;
}

////////////////////////////////////////////////////////////////////////////////
void FHttpPeer::AssignProto()
{
	Proto = 1;

	if (Ssl == nullptr)
	{
		return;
	}

	const char* AlpnProto = nullptr;
	uint32 AlpnProtoLen;
	SSL_get0_alpn_selected(Ssl, &(const uint8*&)AlpnProto, &AlpnProtoLen);
	if (AlpnProto == nullptr)
	{
		return;
	}

	FAnsiStringView Needle(AlpnProto, AlpnProtoLen);
	FAnsiStringView Candidates[] = {
		"http/1.1",
	//	"h2"
	};
	for (int32 i = 0; i < UE_ARRAY_COUNT(Candidates); ++i)
	{
		const FAnsiStringView& Candidate = Candidates[i];
		if (AlpnProtoLen != Candidate.Len())
		{
			continue;
		}

		if (Candidate != Needle)
		{
			continue;
		}

		Proto = i + 1;
		break;
	}
}

} // namespace UE::IoStore::HTTP
