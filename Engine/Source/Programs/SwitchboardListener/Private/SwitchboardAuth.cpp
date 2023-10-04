// Copyright Epic Games, Inc. All Rights Reserved.

#include "SwitchboardAuth.h"
#include "SwitchboardListenerApp.h"

#include "Containers/StringConv.h"
#include "HAL/FileManagerGeneric.h"
#include "HAL/PlatformProcess.h"
#include "Logging/StructuredLog.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#include <iostream>
#include <string>

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include <ShlObj.h>
#include <wincred.h>
#include "Windows/HideWindowsPlatformTypes.h"
#else
#include <sys/file.h>
#include <termios.h>
#endif

UE_PUSH_MACRO("UI")
// Workaround for "ossl_typ.h(144): error C2365: 'UI': redefinition; previous definition was 'namespace'"
//                "ObjectMacros.h(872): note: see declaration of 'UI'"
#define UI UI_ST
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/x509.h>
UE_POP_MACRO("UI")


namespace UE::SwitchboardListener
{
	FString ReadCredentialFromStdin(const FString& CredentialDesc, bool bAllowEmpty /* = false */)
	{
		FString Credential;

		// Disable echo during token input
#if PLATFORM_WINDOWS
		HANDLE Stdin = GetStdHandle(STD_INPUT_HANDLE);
		DWORD PrevConsoleMode = 0;
		GetConsoleMode(Stdin, &PrevConsoleMode);
		SetConsoleMode(Stdin, PrevConsoleMode & ~ENABLE_ECHO_INPUT);
#else
		struct termios Term;
		tcgetattr(fileno(stdin), &Term);
		Term.c_lflag &= ~ECHO;
		tcsetattr(fileno(stdin), TCSADRAIN, &Term);
#endif

		ON_SCOPE_EXIT
		{
			// Restore console echo
#if PLATFORM_WINDOWS
			SetConsoleMode(Stdin, PrevConsoleMode);
#else
			Term.c_lflag |= ECHO;
			tcsetattr(fileno(stdin), TCSADRAIN, &Term);
#endif
		};

		while (Credential.IsEmpty())
		{
			// FIXME?: Doesn't seem like we have a suitable abstraction for this?
			std::string CredFirst, CredSecond;

			printf("\nEnter %s: ", TCHAR_TO_UTF8(*CredentialDesc));
			std::getline(std::cin, CredFirst);

			if (CredFirst.empty() && !bAllowEmpty)
			{
				printf("\nEmpty %s is not allowed!", TCHAR_TO_UTF8(*CredentialDesc));
				continue;
			}

			printf("\nConfirm %s: ", TCHAR_TO_UTF8(*CredentialDesc));
			std::getline(std::cin, CredSecond);

			if (CredFirst == CredSecond)
			{
				printf("\n\n");
				Credential = UTF8_TO_TCHAR(CredFirst.c_str());
			}
			else
			{
				printf("\nProvided values didn't match!\n");
			}
		}

		return Credential;
	}

	bool SupportsPersistentCredentials()
	{
#if PLATFORM_WINDOWS
		return true;
#else
		return false;
#endif
	}

	bool SaveCredential(const FCredential& Credential)
	{
#if PLATFORM_WINDOWS
		const FString NamespacedTargetName =
			FString::Printf(TEXT("SwitchboardListener_%s"), *Credential.CredentialName);

		CREDENTIALW Cred = { 0 };
		Cred.Type = CRED_TYPE_GENERIC;
		Cred.TargetName = const_cast<FString::ElementType*>(*NamespacedTargetName);
		Cred.CredentialBlobSize = sizeof(TCHAR) * Credential.CredentialBlob.Len();
		Cred.CredentialBlob = reinterpret_cast<BYTE*>(
			const_cast<FString::ElementType*>(*Credential.CredentialBlob));
		Cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
		Cred.UserName = const_cast<FString::ElementType*>(*Credential.UserName);
		if (::CredWriteW(&Cred, 0))
		{
			return true;
		}
		else
		{
			UE_LOGFMT(LogSwitchboard, Warning, "CredWriteW failed; GetLastError = {LastError}", static_cast<int64>(::GetLastError()));
		}
#else
		// Not implemented
#endif

		return false;
	}

	TOptional<FCredential> LoadCredential(FStringView CredentialName)
	{
#if PLATFORM_WINDOWS
		const FString NamespacedTargetName =
			FString::Printf(TEXT("SwitchboardListener_%*s"),
				CredentialName.Len(), CredentialName.GetData());

		CREDENTIALW* OutCredPtr = nullptr;
		if (::CredReadW(*NamespacedTargetName, CRED_TYPE_GENERIC, 0, &OutCredPtr))
		{
			FCredential Result;
			Result.CredentialName = CredentialName;
			Result.UserName = FString(OutCredPtr->UserName);
			Result.CredentialBlob = FString(
				OutCredPtr->CredentialBlobSize / sizeof(TCHAR),
				reinterpret_cast<TCHAR*>(OutCredPtr->CredentialBlob));

			::CredFree(OutCredPtr);

			return Result;
		}
		else
		{
			const int64 LastError = ::GetLastError();
			if (LastError == ERROR_NOT_FOUND)
			{
				UE_LOGFMT(LogSwitchboard, Verbose, "CredReadW returned ERROR_NOT_FOUND");
			}
			else
			{
				UE_LOGFMT(LogSwitchboard, Warning, "CredReadW failed; GetLastError = {LastError}", static_cast<int64>(::GetLastError()));
			}
		}
#else
		// Not implemented
#endif

		return {};
	}


	void FillSecureRandom(TArray<uint8>& InOutArray)
	{
		const int32 NumBytes = InOutArray.Num();
		if (ensure(NumBytes > 0))
		{
			check(1 == RAND_bytes(
				reinterpret_cast<unsigned char*>(InOutArray.GetData()),
				NumBytes));
		}
	}
}


namespace UE::SwitchboardListener::Certificates::Private
{
	/**
	 * Generate a 2048-bit RSA key.
	 *
	 * @param PrivateKey The private key struct
	 */
	EVP_PKEY* GeneratePrivateKey()
	{
		EVP_PKEY* PrivateKey = EVP_PKEY_new();

		// Generate the RSA key and assign it to pkey.
		RSA* Rsa = RSA_new();

		{
			BIGNUM* BigNum = BN_new();
			BN_set_word(BigNum, RSA_F4);
			RSA_generate_key_ex(Rsa, 2048, BigNum, nullptr);
			BN_free(BigNum);
		}

		// EVP_PKEY_assign_RSA will "use the supplied (Rsa) internally
		// and so (Rsa) will be freed when the parent pkey is freed."
		if (!EVP_PKEY_assign_RSA(PrivateKey, Rsa))
		{
			EVP_PKEY_free(PrivateKey);
			return nullptr;
		}

		return PrivateKey;
	}


	FString BIOToString(BIO* Bio)
	{
		char* Text;
		long Length = BIO_get_mem_data(Bio, &Text);

		auto Convert = StringCast<TCHAR>(Text, Length);
		FString Result(Convert.Length(), Convert.Get());
		Result.AppendChar(TEXT('\0'));
		return Result;
	}


	/**
	 * Convert a certificate to a string.
	 *
	 * @param x509 The certificate struct
	 */
	FString X509ToString(X509* x509)
	{
		BIO* Bio = BIO_new(BIO_s_mem());
		ON_SCOPE_EXIT{ BIO_free(Bio); };

		check(1 == PEM_write_bio_X509(Bio, x509));

		return BIOToString(Bio);
	}


	/**
	 * Converts a private key to a string.
	 *
	 * @param PrivateKey The private key struct
	 */
	FString PrivateKeyToString(EVP_PKEY* PrivateKey, const FString& PrivateKeyPassword)
	{
		BIO* Bio = BIO_new(BIO_s_mem());
		ON_SCOPE_EXIT{ BIO_free(Bio); };

		if (PrivateKeyPassword.IsEmpty())
		{
			check(1 == PEM_write_bio_PrivateKey_traditional(Bio, PrivateKey,
				nullptr,
				nullptr, 0, nullptr,
				nullptr));
		}
		else
		{
			check(1 == PEM_write_bio_PrivateKey_traditional(Bio, PrivateKey,
				EVP_des_ede3_cbc(),
				nullptr, 0, nullptr,
				TCHAR_TO_UTF8(*PrivateKeyPassword)));
		}

		return BIOToString(Bio);
	}


	/**
	 * Generate self-signed x509 certificate.
	 *
	 * @param PrivateKey The private key struct
	 */
	X509* GenerateCertificate(EVP_PKEY* PrivateKey)
	{
		X509* x509 = X509_new();

		// Set the serial number.
		uint64_t SerialNumber;
		check(1 == RAND_bytes(reinterpret_cast<unsigned char*>(&SerialNumber), sizeof(SerialNumber)));
		ASN1_INTEGER_set_uint64(X509_get_serialNumber(x509), SerialNumber);

		// This certificate is valid for 10 years.
		X509_gmtime_adj(X509_get_notBefore(x509), 0);
		X509_gmtime_adj(X509_get_notAfter(x509), (60L*60*24*365*10));

		// Set the public key for our certificate.
		X509_set_pubkey(x509, PrivateKey);

		// Set the common name, and copy the subject name to the issuer name.
		X509_NAME* SubjectName = X509_get_subject_name(x509);
		X509_NAME_add_entry_by_txt(SubjectName, "CN", MBSTRING_ASC,
			(unsigned char*)"SwitchboardListener", -1, -1, 0);
		X509_set_issuer_name(x509, SubjectName);

		// Actually sign the certificate with our key.
		if (!X509_sign(x509, PrivateKey, EVP_sha256()))
		{
			X509_free(x509);
			return nullptr;
		}

		return x509;
	}

#if !PLATFORM_WINDOWS
	bool EnsurePathOnlyOwnerReadable(const FString& InPath)
	{
		FString NormalizedPath = InPath;
		FPaths::NormalizeFilename(NormalizedPath);
		NormalizedPath = FPaths::ConvertRelativePathToFull(NormalizedPath);

		struct stat Stat;
		if (stat(TCHAR_TO_UTF8(*NormalizedPath), &Stat) == 0)
		{
			// Clear group/other read/write/execute bits
			Stat.st_mode &= ~(S_IRWXG | S_IRWXO);
			chmod(TCHAR_TO_UTF8(*NormalizedPath), Stat.st_mode);
			return true;
		}
		else
		{
			return false;
		}
	}
#endif
}


namespace UE::SwitchboardListener::Certificates
{
	using namespace UE::SwitchboardListener::Certificates::Private;


	TTuple<FString, FString> GetSelfSignedPaths(bool bCreate /* = false */)
	{
		IPlatformFile& FileManager = FPlatformFileManager::Get().GetPlatformFile();

#if PLATFORM_WINDOWS
		wchar_t* LocalAppDataWstr = nullptr;
		::SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &LocalAppDataWstr);
		ON_SCOPE_EXIT{ ::CoTaskMemFree(LocalAppDataWstr); };

		const FString LocalAppDataDir(LocalAppDataWstr);
		const FString SblHomeDir = LocalAppDataDir / TEXT("SwitchboardListener");
		if (bCreate)
		{
			FileManager.CreateDirectory(*SblHomeDir);
		}

		const FString CertificateFile = SblHomeDir / TEXT("transportcert.pem");
		const FString PrivateKeyFile = SblHomeDir / TEXT("transportkey.key");
#else
		const FString UserHomeDir = FPlatformProcess::UserHomeDir();
		const FString SblHomeDir = UserHomeDir / TEXT(".switchboardlistener");
		const FString SblCertDir = SblHomeDir / TEXT("cert");

		if (bCreate)
		{
			FileManager.CreateDirectory(*SblHomeDir);
			FileManager.CreateDirectory(*SblCertDir);
			EnsurePathOnlyOwnerReadable(SblCertDir);
		}

		const FString CertificateFile = SblCertDir / TEXT("transportcert.pem");
		const FString PrivateKeyFile = SblCertDir / TEXT("transportkey.key");
#endif

		return TTuple<FString, FString>(CertificateFile, PrivateKeyFile);
	}


	TOptional<FString> CreateSelfSigned(const FString& PrivateKeyPassword)
	{
		EVP_PKEY* PrivateKey = GeneratePrivateKey();
		if (!PrivateKey)
		{
			return TOptional<FString>{};
		}

		ON_SCOPE_EXIT{ EVP_PKEY_free(PrivateKey); };

		X509* x509 = GenerateCertificate(PrivateKey);
		if (!x509)
		{
			return TOptional<FString>{};
		}

		ON_SCOPE_EXIT{ X509_free(x509); };

		// Compute SHA256 fingerprint.
		uint8 Digest[EVP_MAX_MD_SIZE];
		uint32 DigestSize = 0;
		check(1 == X509_digest(x509, EVP_sha256(), Digest, &DigestSize));

		// Return the first half in the format "09:AB:CD:..."
		FString Fingerprint;
		for (uint32 ByteIdx = 0; ByteIdx < DigestSize/2; ++ByteIdx)
		{
			ByteToHex(Digest[ByteIdx], Fingerprint);
			if (ByteIdx < (DigestSize/2 - 1))
			{
				Fingerprint.AppendChar(TEXT(':'));
			}
		}

		// Get PEM strings and write to files.
		const FString PrivateKeyString = PrivateKeyToString(PrivateKey, PrivateKeyPassword);
		const FString CertificateString = X509ToString(x509);

		TTuple<FString, FString> Paths = GetSelfSignedPaths();
		const FString CertificateFile = Paths.Get<0>();
		const FString PrivateKeyFile = Paths.Get<1>();

		bool bSucceeded = true;
		bSucceeded &= FFileHelper::SaveStringToFile(CertificateString, *CertificateFile);
		bSucceeded &= FFileHelper::SaveStringToFile(PrivateKeyString, *PrivateKeyFile);

#if !PLATFORM_WINDOWS
		EnsurePathOnlyOwnerReadable(PrivateKeyFile);
#endif

		return bSucceeded ? Fingerprint : TOptional<FString>{};
	}
};
