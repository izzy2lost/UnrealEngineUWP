// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemLivePrivatePCH.h"
#include "OnlineEventsInterfaceLive.h"
#include "OnlineSubsystemLive.h"
#include "OnlineSessionInterfaceLive.h"
#include "OnlineSubsystemLiveTypes.h"
#include "SimpleTokenParser.h"
#include "OnlineJsonSerializer.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

// @ATG_CHANGE : BEGIN - Adding XIM
#if USE_XIM
#include "Xim/OnlineSessionInterfaceXim.h"
#endif
// @ATG_CHANGE : END

// @ATG_CHANGE : BEGIN - UWP LIVE support
#if PLATFORM_XBOXONE
// @ATG_CHANGE : END

static char * WideCharToMultiByteCopy( const FString& Source )
{
	size_t ConvertedChars = 0;

	const int32 SrcLenWithoutTerminator = Source.Len();

	ANSICHAR * Dest = new ANSICHAR[SrcLenWithoutTerminator + 1];

	wcstombs_s( &ConvertedChars, Dest, SrcLenWithoutTerminator + 1, *Source, SrcLenWithoutTerminator );

	check( ConvertedChars == SrcLenWithoutTerminator + 1 );

	Dest[SrcLenWithoutTerminator] = '\0';

	return Dest;
}

FOnlineEventsLive::FOnlineEventsLive(FOnlineSubsystemLive* InSubsystem) :
	Subsystem( InSubsystem ),
	Provider( NULL ),
	RegHandle( (REGHANDLE)0 )
{
	LoadAndInitFromJsonConfig( TEXT( "Events.json" ) );
}

FOnlineEventsLive::~FOnlineEventsLive()
{
	UnregisterProvider();

	// Free memory used for copied multi-byte strings
	for ( int32 i = 0; i < Events.Num(); i++ )
	{
		if ( Events[i].EventMemoryOffset != -1 )
		{
			ETX_EVENT_DESCRIPTOR * Event = (ETX_EVENT_DESCRIPTOR*)&EventMemory[Events[i].EventMemoryOffset];

			if ( Event->Name != NULL )
			{
				delete []Event->Name;
			}

			if ( Event->SchemaVersion != NULL )
			{
				delete []Event->SchemaVersion;
			}
		}
	}
}

class FEventsConfig : public FOnlineJsonSerializable
{
public:
	FString EventsHeaderName;

	BEGIN_ONLINE_JSON_SERIALIZER
		ONLINE_JSON_SERIALIZE( "EventsHeaderName", EventsHeaderName );
	END_ONLINE_JSON_SERIALIZER
};

bool FOnlineEventsLive::LoadAndInitFromJsonConfig( const TCHAR* JsonConfigName )
{
	const FString BaseDir = FPaths::ProjectDir() + TEXT( "Config/OSS/Live/" );
	const FString JSonConfigFilename = BaseDir + JsonConfigName;;

	FString JSonText;

	if ( !FFileHelper::LoadFileToString( JSonText, *JSonConfigFilename ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Failed to parse json OSS events config: %s"), *JSonConfigFilename );
		return false;
	}

	FEventsConfig JsonConfig;

	if ( !JsonConfig.FromJson( JSonText ) )
	{
		return false;
	}

	const FString Filename = BaseDir + JsonConfig.EventsHeaderName;

	const bool Result = ParseHeader( *Filename );
	
	if ( !Result )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Failed to parse event header: %s"), *Filename );
	}

	// These were temp
	Fields.Empty();

	return Result;
}

bool FOnlineEventsLive::ParseHeader( const TCHAR* Filename )
{
	FString Text;

	if ( !FFileHelper::LoadFileToString( Text, Filename ) )
	{
		return false;
	}

	FSimpleTokenParser	HeaderParser;
	FSimpleToken		Token;

	HeaderParser.ResetParser( *Text, 0 );

	while ( HeaderParser.GetToken( Token ) )
	{
		if ( Token.Matches( TEXT( "ETX_FIELD_DESCRIPTOR" ) ) )
		{
			if ( !ParseDescriptor( HeaderParser, 0 ) )
			{
				UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Failed to parse ETX_FIELD_DESCRIPTOR: %s"), Filename );
				return false;
			}
		}

		if ( Token.Matches( TEXT( "ETX_EVENT_DESCRIPTOR" ) ) )
		{
			if ( !ParseDescriptor( HeaderParser, 1 ) )
			{
				UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Failed to parse ETX_EVENT_DESCRIPTOR: %s"), Filename );
				return false;
			}
		}

		if ( Token.Matches( TEXT( "ETX_PROVIDER_DESCRIPTOR" ) ) )
		{
			if ( !ParseProvider( HeaderParser ) )
			{
				UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Failed to parse ETX_EVENT_DESCRIPTOR: %s"), Filename );
				return false;
			}
		}

		if ( Token.StartsWith( TEXT( "EventWrite" ) ) )
		{
			if ( !ParseEventParameterNames( HeaderParser, Token.Identifier ) )
			{
				UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Failed to parse event: %s"), Filename );
				return false;
			}
		}
	}

	return true;
}

bool FOnlineEventsLive::ParseDescriptor( FSimpleTokenParser & HeaderParser, const int32 Type )
{
	FSimpleToken Token;

	if ( !HeaderParser.GetToken( Token ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Failed to parse token.  Line: %i" ), HeaderParser.InputLine );
		return false;
	}

	if ( Token.TokenType != TOKEN_Identifier )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Expected identifier type token.  Line: %i" ), HeaderParser.InputLine );
		return false;
	}

	if ( Type == 0 )
	{
		// Add and construct field
		Fields.SetNum( Fields.Num() + 1 );

		FOnlineEventFieldsLive & Field = Fields[ Fields.Num() - 1 ];

		Field.SetName( Token.Identifier );
	}

	if ( !HeaderParser.ExpectToken( Token, TEXT( "[" ) ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Expected '[' but found %s instead.  Line: %i" ), Token.Identifier, HeaderParser.InputLine );
		return false;
	}

	if ( !HeaderParser.GetToken( Token ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Failed to parse token.  Line: %i" ), HeaderParser.InputLine );
		return false;
	}

	if ( Token.TokenType != TOKEN_Const || Token.PropertyType != CPT_Int )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Invalid token type.  Line: %i" ), HeaderParser.InputLine );
		return false;
	}

	const int32 NumFieldDescriptors = Token.Int;

	if ( !HeaderParser.ExpectToken( Token, TEXT( "]" ) ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Expected ']' but found %s instead.  Line: %i" ), Token.Identifier, HeaderParser.InputLine );
		return false;
	}

	if ( !HeaderParser.ExpectToken( Token, TEXT( "=" ) ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Expected '=' but found %s instead.  Line: %i" ), Token.Identifier, HeaderParser.InputLine );
		return false;
	}

	if ( !HeaderParser.ExpectToken( Token, TEXT( "{" ) ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Expected '{' but found %s instead.  Line: %i" ), Token.Identifier, HeaderParser.InputLine );
		return false;
	}

	for ( int32 i = 0; i < NumFieldDescriptors; i++ )
	{
		if ( i > 0 && !HeaderParser.ExpectToken( Token, TEXT( "," ) ) )
		{
			UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Expected ',' but found %s instead.  Line: %i" ), Token.Identifier, HeaderParser.InputLine );
			return false;
		}

		if ( Type == 0 )
		{
			if ( !ParseFieldDescriptorElement( HeaderParser ) )
			{
				return false;
			}
		}
		else
		{
			if ( !ParseEventDescriptorElement( HeaderParser ) )
			{
				return false;
			}
		}
	}

	if ( !HeaderParser.ExpectToken( Token, TEXT( "}" ) ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Expected '}' but found %s instead.  Line: %i" ), Token.Identifier, HeaderParser.InputLine );
		return false;
	}

	if ( !HeaderParser.ExpectToken( Token, TEXT( ";" ) ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Expected ';' but found %s instead.  Line: %i" ), Token.Identifier, HeaderParser.InputLine );
		return false;
	}

	return true;
}

class FFieldTypeNameMap
{
public:
	FFieldTypeNameMap(const TCHAR* const InName, const int32 InValue)
		: Name(InName)
		, Value(InValue)
	{
	}

	const TCHAR *	Name;
	const int32		Value;
};

#define DECLARE_FIELD_TYPE_MAP( Name ) FFieldTypeNameMap( TEXT( PREPROCESSOR_TO_STRING( Name ) ), Name )

static const FFieldTypeNameMap FieldTypeNameMappings[] = 
{
	DECLARE_FIELD_TYPE_MAP( EtxFieldType_UnicodeString ),
	DECLARE_FIELD_TYPE_MAP( EtxFieldType_Int8 ),
	DECLARE_FIELD_TYPE_MAP( EtxFieldType_UInt8 ),
	DECLARE_FIELD_TYPE_MAP( EtxFieldType_Int16 ),
	DECLARE_FIELD_TYPE_MAP( EtxFieldType_UInt16 ),
	DECLARE_FIELD_TYPE_MAP( EtxFieldType_Int32 ),
	DECLARE_FIELD_TYPE_MAP( EtxFieldType_UInt32 ),
	DECLARE_FIELD_TYPE_MAP( EtxFieldType_Int64 ),
	DECLARE_FIELD_TYPE_MAP( EtxFieldType_UInt64 ),
	DECLARE_FIELD_TYPE_MAP( EtxFieldType_Float ),
	DECLARE_FIELD_TYPE_MAP( EtxFieldType_Double ),
	DECLARE_FIELD_TYPE_MAP( EtxFieldType_Boolean ),
	DECLARE_FIELD_TYPE_MAP( EtxFieldType_Binary ),
	DECLARE_FIELD_TYPE_MAP( EtxFieldType_GUID ),
	DECLARE_FIELD_TYPE_MAP( EtxFieldType_Pointer ),
	DECLARE_FIELD_TYPE_MAP( EtxFieldType_FILETIME ),
	DECLARE_FIELD_TYPE_MAP( EtxFieldType_SYSTEMTIME ),
	DECLARE_FIELD_TYPE_MAP( EtxFieldType_CountedUnicodeString ),
	DECLARE_FIELD_TYPE_MAP( EtxFieldType_IPv4 ),
	DECLARE_FIELD_TYPE_MAP( EtxFieldType_IPv6 )
};

static int32 FindFieldTypeFromName( const TCHAR* Name )
{
	for ( int32 i = 0; i < UE_ARRAY_COUNT( FieldTypeNameMappings ); i++ )
	{
		if ( !FCString::Stricmp( FieldTypeNameMappings[i].Name, Name ) )
		{
			return FieldTypeNameMappings[i].Value;
		}
	}

	return -1;
}

static const TCHAR* FindFieldNameFromType( const int32 Type )
{
	for ( int32 i = 0; i < UE_ARRAY_COUNT( FieldTypeNameMappings ); i++ )
	{
		if ( FieldTypeNameMappings[i].Value == Type )
		{
			return FieldTypeNameMappings[i].Name;
		}
	}

	return TEXT( "UNKNOWN" );
}

bool FOnlineEventsLive::ParseFieldDescriptorElement( FSimpleTokenParser & HeaderParser )
{
	FSimpleToken Token;

	if ( !HeaderParser.ExpectToken( Token, TEXT( "{" ) ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Expected '{' but found %s instead.  Line: %i" ), Token.Identifier, HeaderParser.InputLine );
		return false;
	}

	if ( !HeaderParser.GetToken( Token ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Failed to parse token.  Line: %i" ), HeaderParser.InputLine );
		return false;
	}

	if ( Token.TokenType != TOKEN_Identifier )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Invalid token type.  Line: %i" ), HeaderParser.InputLine );
		return false;
	}

	const int32 FieldTypeValue = FindFieldTypeFromName( Token.Identifier );

	if ( FieldTypeValue == -1 )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Invalid field type: %s.  Line: %i" ), Token.Identifier, HeaderParser.InputLine );
		return false;
	}

	if ( !HeaderParser.ExpectToken( Token, TEXT( "," ) ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Expected ',' but found %s instead.  Line: %i" ), Token.Identifier, HeaderParser.InputLine );
		return false;
	}

	if ( !HeaderParser.GetToken( Token ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Failed to parse token.  Line: %i" ), HeaderParser.InputLine );
		return false;
	}

	if ( Token.TokenType != TOKEN_Const || Token.PropertyType != CPT_Int )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Invalid token type.  Line: %i" ), HeaderParser.InputLine );
		return false;
	}

	// Add the field to the current event
	FOnlineEventFieldsLive & Field = Fields[ Fields.Num() - 1 ];

	Field.AddField( FieldTypeValue, Token.Int );

	if ( !HeaderParser.ExpectToken( Token, TEXT( "}" ) ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Expected '}' but found %s instead.  Line: %i" ), Token.Identifier, HeaderParser.InputLine );
		return false;
	}

	return true;
}

bool FOnlineEventsLive::ParseEventDescriptorElement( FSimpleTokenParser & HeaderParser )
{
	FSimpleToken Token;

	if ( !HeaderParser.ExpectToken( Token, TEXT( "{" ) ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Expected '{' but found %s instead.  Line: %i" ), Token.Identifier, HeaderParser.InputLine );
		return false;
	}

	if ( !HeaderParser.ExpectToken( Token, TEXT( "{" ) ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Expected '{' but found %s instead.  Line: %i" ), Token.Identifier, HeaderParser.InputLine );
		return false;
	}

	// Parse EVENT_DESCRIPTOR
	int32 Values[7];

	if ( !ParseIntValues( HeaderParser, Values, 7 ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: ParseIntValues failed." ) );
		return false;
	}

	if ( !HeaderParser.ExpectToken( Token, TEXT( "}" ) ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Expected '{' but found %s instead.  Line: %i" ), Token.Identifier, HeaderParser.InputLine );
		return false;
	}

	if ( !HeaderParser.ExpectToken( Token, TEXT( "," ) ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Expected ',' but found %s instead.  Line: %i" ), Token.Identifier, HeaderParser.InputLine );
		return false;
	}

	// Parse name
	FString Name;

	if ( !HeaderParser.GetToken( Token ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Failed to parse token.  Line: %i" ), HeaderParser.InputLine );
		return false;
	}

	if ( Token.TokenType != TOKEN_Const || Token.PropertyType != CPT_String )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Invalid token type.  Line: %i" ), HeaderParser.InputLine );
		return false;
	}

	Name = Token.String;

	if ( !HeaderParser.ExpectToken( Token, TEXT( "," ) ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Expected ',' but found %s instead.  Line: %i" ), Token.Identifier, HeaderParser.InputLine );
		return false;
	}

	// Parse scheme
	FString Schema;

	if ( !HeaderParser.GetToken( Token ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Failed to parse token.  Line: %i" ), HeaderParser.InputLine );
		return false;
	}

	if ( Token.TokenType != TOKEN_Const || Token.PropertyType != CPT_String )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Invalid token type.  Line: %i" ), HeaderParser.InputLine );
		return false;
	}

	Schema = Token.String;

	if ( !HeaderParser.ExpectToken( Token, TEXT( "," ) ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Expected ',' but found %s instead.  Line: %i" ), Token.Identifier, HeaderParser.InputLine );
		return false;
	}

	// Parse fields name
	FString FieldName;

	if ( !HeaderParser.GetToken( Token ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Failed to parse token.  Line: %i" ), HeaderParser.InputLine );
		return false;
	}

	if ( Token.TokenType != TOKEN_Identifier )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Invalid token type.  Line: %i" ), HeaderParser.InputLine );
		return false;
	}

	FieldName = Token.Identifier;

	if ( !HeaderParser.ExpectToken( Token, TEXT( "," ) ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Expected ',' but found %s instead.  Line: %i" ), Token.Identifier, HeaderParser.InputLine );
		return false;
	}

	// Find the fields
	int32 FieldIndex = -1;

	for ( int32 i = 0; i < Fields.Num(); i++ )
	{
		if ( Fields[i].GetName() == FieldName )
		{
			FieldIndex = i;
			break;
		}
	}

	if ( FieldIndex == -1 )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Invalid field name: %s.  Line: %i" ), Token.Identifier, HeaderParser.InputLine );
	}

	// Parse num fields
	if ( !HeaderParser.GetToken( Token ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Failed to parse token.  Line: %i" ), HeaderParser.InputLine );
		return false;
	}

	if ( Token.TokenType != TOKEN_Const || Token.PropertyType != CPT_Int )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Invalid token type.  Line: %i" ), HeaderParser.InputLine );
		return false;
	}

	if ( Fields[FieldIndex].GetNumFields() != Token.Int )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Field numbers don't match: %i != %i.  Line: %i" ), Fields[FieldIndex].GetNumFields(), Token.Int, HeaderParser.InputLine );
		return false;
	}

	// Parse other fields that we don't care about (we hard code them)
	for ( int32 i = 0; i < 9; i++ )
	{
		if ( !HeaderParser.ExpectToken( Token, TEXT( "," ) ) )
		{
			UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Expected ',' but found %s instead.  Line: %i" ), Token.Identifier, HeaderParser.InputLine );
			return false;
		}

		if ( !HeaderParser.GetToken( Token ) )
		{
			UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Failed to parse token.  Line: %i" ), HeaderParser.InputLine );
			return false;
		}
	}

	if ( !HeaderParser.ExpectToken( Token, TEXT( "}" ) ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Expected '{' but found %s instead.  Line: %i" ), Token.Identifier, HeaderParser.InputLine );
		return false;
	}

	// Add the event
	AddEvent( Values[0], Values[1], Name, Schema, Fields[FieldIndex] );

	return true;
}

bool FOnlineEventsLive::ParseProvider( FSimpleTokenParser & HeaderParser )
{
	FSimpleToken Token;

	if ( !HeaderParser.GetToken( Token ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Failed to parse token.  Line: %i" ), HeaderParser.InputLine );
		return false;
	}

	if ( Token.TokenType != TOKEN_Identifier )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Expected identifier type token.  Line: %i" ), HeaderParser.InputLine );
		return false;
	}

	if ( !HeaderParser.ExpectToken( Token, TEXT( "=" ) ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Expected '=' but found %s instead.  Line: %i" ), Token.Identifier, HeaderParser.InputLine );
		return false;
	}

	if ( !HeaderParser.ExpectToken( Token, TEXT( "{" ) ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Expected '{' but found %s instead.  Line: %i" ), Token.Identifier, HeaderParser.InputLine );
		return false;
	}

	// Parse provider name
	if ( !HeaderParser.GetToken( Token ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Failed to parse token.  Line: %i" ), HeaderParser.InputLine );
		return false;
	}

	if ( Token.TokenType != TOKEN_Const || Token.PropertyType != CPT_String )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Invalid token type.  Line: %i" ), HeaderParser.InputLine );
		return false;
	}

	FString ProviderName = Token.String;

	if ( !HeaderParser.ExpectToken( Token, TEXT( "," ) ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Expected '{' but found %s instead.  Line: %i" ), Token.Identifier, HeaderParser.InputLine );
		return false;
	}

	if ( !HeaderParser.ExpectToken( Token, TEXT( "{" ) ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Expected '{' but found %s instead.  Line: %i" ), Token.Identifier, HeaderParser.InputLine );
		return false;
	}

	int32 Values[3];

	if ( !ParseIntValues( HeaderParser, Values, 3 ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: ParseIntValues failed." ) );
		return false;
	}

	if ( !HeaderParser.ExpectToken( Token, TEXT( "," ) ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Expected '{' but found %s instead.  Line: %i" ), Token.Identifier, HeaderParser.InputLine );
		return false;
	}

	if ( !HeaderParser.ExpectToken( Token, TEXT( "{" ) ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Expected '{' but found %s instead.  Line: %i" ), Token.Identifier, HeaderParser.InputLine );
		return false;
	}

	int32 Values2[8];

	if ( !ParseIntValues( HeaderParser, Values2, 8 ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: ParseIntValues failed." ) );
		return false;
	}

	if ( !HeaderParser.ExpectToken( Token, TEXT( "}" ) ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Expected '{' but found %s instead.  Line: %i" ), Token.Identifier, HeaderParser.InputLine );
		return false;
	}

	if ( !HeaderParser.ExpectToken( Token, TEXT( "}" ) ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Expected '{' but found %s instead.  Line: %i" ), Token.Identifier, HeaderParser.InputLine );
		return false;
	}

	if ( !HeaderParser.ExpectToken( Token, TEXT( "," ) ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Expected '{' but found %s instead.  Line: %i" ), Token.Identifier, HeaderParser.InputLine );
		return false;
	}

	// Parse num events
	if ( !HeaderParser.GetToken( Token ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Failed to parse token.  Line: %i" ), HeaderParser.InputLine );
		return false;
	}

	if ( Token.TokenType != TOKEN_Const || Token.PropertyType != CPT_Int )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Invalid token type.  Line: %i" ), HeaderParser.InputLine );
		return false;
	}

	if ( Events.Num() != Token.Int )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Event numbers don't match: %i != %i.  Line: %i" ), Events.Num(), Token.Int, HeaderParser.InputLine );
		return false;
	}

	// Skip to semi-colon
	while ( true )
	{
		if ( !HeaderParser.GetToken( Token ) )
		{
			UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Failed to parse token.  Line: %i" ), HeaderParser.InputLine );
			return false;
		}

		if ( Token.Matches( TEXT( ";" ) ) )
		{
			break;
		}
	}

	GUID ProviderGUID = { (unsigned long)Values[0], (unsigned short)Values[1], (unsigned short)Values[2], (unsigned char)Values2[0], (unsigned char)Values2[1], (unsigned char)Values2[2], (unsigned char)Values2[3], (unsigned char)Values2[4], (unsigned char)Values2[5], (unsigned char)Values2[6], (unsigned char)Values2[7] };

	if ( !RegisterProvider( ProviderName, ProviderGUID, (REGHANDLE)0 ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: RegisterProvider FAILED." ) );
		return false;
	}

	return true;
}

bool FOnlineEventsLive::ParseIntValues( FSimpleTokenParser& HeaderParser, int32* Values, const int32 NumValues )
{
	FSimpleToken Token;

	for ( int32 i = 0; i < NumValues; i++ )
	{
		if ( i > 0 && !HeaderParser.ExpectToken( Token, TEXT( "," ) ) )
		{
			UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Expected ',' but found %s instead.  Line: %i" ), Token.Identifier, HeaderParser.InputLine );
			return false;
		}

		if ( !HeaderParser.GetToken( Token ) )
		{
			UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Failed to parse token.  Line: %i" ), HeaderParser.InputLine );
			return false;
		}

		if ( Token.TokenType != TOKEN_Const || Token.PropertyType != CPT_Int )
		{
			UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive: Invalid token type.  Line: %i" ), HeaderParser.InputLine );
			return false;
		}

		Values[i] = Token.Int;
	}

	return true;
}

bool FOnlineEventsLive::ParseEventParameterNames( FSimpleTokenParser & HeaderParser, const TCHAR* EventName  )
{
	FSimpleToken Token;

	if ( !HeaderParser.ExpectToken( Token, TEXT( "(" ) ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive::ParseEventParameterNames: Failed to parse token.  Line: %i" ), HeaderParser.InputLine );
		return false;
	}

	FString RealEventName( EventName );

	RealEventName = RealEventName.RightChop( 10 );		// Chop off "EventName"

	// Find this event
	FOnlineEventLive * Event = FindEventByName( RealEventName );

	if ( Event == NULL )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive::ParseEventParameterNames: Failed find event %s.  Line: %i" ), *RealEventName, HeaderParser.InputLine );
		return false;
	}

	// Get the fields
	FOnlineEventFieldsLive & Fields = Event->Fields;

	// Add the default parameter that every event has
	Fields.AddFieldParmName( TEXT( "Default" ) );

	// LastFieldParmName will be the token just before the ',' we parse
	FString LastFieldParmName;

	check( LastFieldParmName.IsEmpty() );

	// Parse field parameters
	while ( true )
	{
		if ( !HeaderParser.GetToken( Token ) )
		{
			UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive::ParseEventParameterNames: Failed to parse token.  Line: %i" ), HeaderParser.InputLine );
			return false;
		}

		if ( Token.Matches( TEXT( "," ) ) || Token.Matches( TEXT( ")" ) ) )
		{
			if ( LastFieldParmName.IsEmpty() )
			{
				UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive::ParseEventParameterNames: Expected parameter, but got ','.  Line: %i" ), HeaderParser.InputLine );
				return false;
			}

			// Expect field name right before commas
			Fields.AddFieldParmName( *LastFieldParmName );

			LastFieldParmName.Empty();

			if ( Token.Matches( TEXT( ")" ) ) )
			{
				// We're done
				break;
			}
		}

		LastFieldParmName = Token.Identifier;
	}

	// We should have parsed as many parameters equal to the number of fields
	if ( Fields.GetNumFields() != Fields.GetNumFieldParmNames() )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive::ParseEventParameterNames: Invalid number of parameters.  Line: %i" ), HeaderParser.InputLine );
		return false;
	}

	return true;
}

void FOnlineEventsLive::AddEvent( const uint16 Id, const uint8 Version, const FString& Name, const FString& SchemaVersion, const FOnlineEventFieldsLive& Fields )
{
	Events.SetNum( Events.Num() + 1 );		// This will and and construct a new event

	// Get a reference to this newly added event
	FOnlineEventLive & NewEvent = Events[Events.Num() - 1];

	// Setup name, and make a copy of the fields
	NewEvent.Name	= Name;
	NewEvent.Fields = Fields;

	EVENT_DESCRIPTOR LocalDesc = { (USHORT)Id, (UCHAR)Version, 0, 0, 0, 0, 0x0 };

	// We need to make copies of these string (as well as convert to multi-byte)
	const char * MultibyteName			= WideCharToMultiByteCopy( Name );
	const char * MultibyteSchemaVersion = WideCharToMultiByteCopy( SchemaVersion );

	// We have to define a local event in place, since some of the fields are const, and there isn't an appropriate constructor to new one
	ETX_EVENT_DESCRIPTOR LocalEvent = 
	{
		LocalDesc, MultibyteName, MultibyteSchemaVersion, &NewEvent.Fields.GetFieldDescByIndex( 0 ), (UINT8)NewEvent.Fields.GetNumFields(), 0, EtxEventEnabledState_Undefined, EtxEventEnabledState_ProviderDefault, EtxPopulationSample_Undefined, EtxPopulationSample_UseProviderPopulationSample, EtxEventLatency_Undefined, EtxEventLatency_ProviderDefault, EtxEventPriority_Undefined, EtxEventPriority_ProviderDefault
	};

	// Allocate memory for the event, and make a final copy
	// We need to do this in a way that all the event memory is contiguous to satisfy the needs of EtxRegister
	NewEvent.EventMemoryOffset = EventMemory.AddUninitialized( sizeof( LocalEvent ) );

	// Copy the memory over
	FMemory::Memcpy( &EventMemory[NewEvent.EventMemoryOffset], &LocalEvent, sizeof( LocalEvent ) );
}

FOnlineEventLive* FOnlineEventsLive::FindEventByName( const FString& Name )
{
	for ( int32 i = 0; i < Events.Num(); i++ )
	{
		if ( Events[i].Name == Name )
		{
			return &Events[i];
		}
	}
	return NULL;
}

bool FOnlineEventsLive::TriggerEvent( const TCHAR* Name, const FOnlineEventParmsLive& Parms )
{
	const FOnlineEventLive* Event = FindEventByName( Name );

	if ( Event == NULL )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive::TriggerEvent: Event '%s' not found." ), Name );
		return false;
	}

	if ( Event->Fields.GetNumFields() != Parms.GetNumParms() + 1 )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive::TriggerEvent: Incorrect number of parameters for event: %s.  Got %i but expected %i." ), Name, Parms.GetNumParms(), Event->Fields.GetNumFields() - 1 );
		return false;
	}

	EVENT_DATA_DESCRIPTOR * EventData = new EVENT_DATA_DESCRIPTOR[Event->Fields.GetNumFields()];
	UINT8					Scratch[64];

	EtxFillCommonFields_v7( &EventData[0], Scratch, 64 );

	// Fill in the parameters
	for ( int32 i = 0; i < Parms.GetNumParms(); i++ )
	{
		EventDataDescCreate( &EventData[ i + 1 ], Parms.GetParmData( i ), Parms.GetParmDataSize( i ) );
	}

	// Write the event
	const ETX_EVENT_DESCRIPTOR * EventDesc = (ETX_EVENT_DESCRIPTOR*)&EventMemory[Event->EventMemoryOffset];

	const bool bSuccess = EtxEventWrite( EventDesc, Provider, RegHandle, Event->Fields.GetNumFields(), EventData ) == ERROR_SUCCESS;

	delete []EventData;

	if ( !bSuccess )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive::TriggerEvent: EtxEventWrite Failed for event: %s" ), Name );
	}

	return bSuccess;
}

bool FOnlineEventsLive::RegisterProvider( const FString& Name, const GUID& Guid, const REGHANDLE InRegHandle )
{
	// We need to make a copy of this string (as well as convert to multi-byte)
	const char * MultibyteName = WideCharToMultiByteCopy( Name );

	ETX_PROVIDER_DESCRIPTOR LocalProvider = { MultibyteName, Guid, (UINT32)Events.Num(), (ETX_EVENT_DESCRIPTOR *)EventMemory.GetData(), 0, EtxProviderEnabledState_Undefined, EtxProviderEnabledState_OnByDefault, 0, 100, EtxProviderLatency_Undefined, EtxProviderLatency_RealTime, EtxProviderPriority_Undefined, EtxProviderPriority_Critical };
	Provider = new ETX_PROVIDER_DESCRIPTOR( LocalProvider );

	RegHandle = InRegHandle;

	if ( EtxRegister( Provider, &RegHandle ) != ERROR_SUCCESS )
	{
		delete []Provider->Name;
		delete Provider;
		Provider = NULL;
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive::RegisterProvider: EtxRegister FAILED" ) );
		return false;
	}

	return true;
}

void FOnlineEventsLive::UnregisterProvider()
{
	if ( Provider != NULL )
	{
		if ( EtxUnregister( Provider, &RegHandle ) != ERROR_SUCCESS )
		{
			UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive::UnregisterProvider: EtxRegister FAILED" ) );
		}

		delete []Provider->Name;
		delete Provider;
		Provider = NULL;
	}
}

bool FOnlineEventsLive::TriggerEvent( const FUniqueNetId& PlayerId, const TCHAR* EventName, const FOnlineEventParms& Parms )
{
	FUniqueNetIdLive LiveId( PlayerId );

	if ( LiveId.UniqueNetIdStr.Len() <= 1 )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive::TriggerEvent: Invalid live id" ) );
		return false;
	}

	const FOnlineEventLive* Event = FindEventByName( EventName );

	if ( Event == NULL )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive::TriggerEvent: Event '%s' not found." ), EventName );
		return false;
	}

	// Make a list of internal parameters that will be a final copy of what we use for the actual low level event call
	FOnlineEventParmsLive InternalParms;

	for ( int32 i = 1; i < Event->Fields.GetNumFields() && i < Event->Fields.GetNumFieldParmNames(); i++ )
	{
		const FName& FieldParmName		= Event->Fields.GetFieldParmNameByIndex( i );
		const TCHAR* FieldParmTypeName = FindFieldNameFromType( Event->Fields.GetFieldDescByIndex( i ).Type );

		// First, see if it is one of the built in parameters, and fill those in for the caller
		if ( FieldParmName == TEXT( "UserId" ) )
		{
			if ( Event->Fields.GetFieldDescByIndex( i ).Type != EtxFieldType_UnicodeString )
			{
				UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive::TriggerEvent: Event '%s' cannot convert parameter '%s' from string to %s." ), EventName, *FieldParmName.ToString(), FieldParmTypeName );
				return false;
			}
			InternalParms.AddParm( *LiveId.UniqueNetIdStr, ( LiveId.UniqueNetIdStr.Len() + 1 ) * sizeof( WCHAR ) );
			continue;
		}

		if ( FieldParmName == TEXT( "PlayerSessionId" ) )
		{
			// FIXME: Get this from session
			if ( Event->Fields.GetFieldDescByIndex( i ).Type != EtxFieldType_GUID )
			{
				UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive::TriggerEvent: Event '%s' cannot convert parameter '%s' from string to %s." ), EventName, *FieldParmName.ToString(), FieldParmTypeName );
				return false;
			}
			FGuid PlayerSessionId;
			const FGuid* FoundPlayerSessionId = PlayerSessionIds.Find(LiveId);
			if (FoundPlayerSessionId != nullptr)
			{
				PlayerSessionId = *FoundPlayerSessionId;
			}
			InternalParms.AddParm( &PlayerSessionId, sizeof( PlayerSessionId ) );
			continue;
		}

// @ATG_CHANGE : BEGIN - Adding XIM
#if USE_XIM
		typedef FOnlineSessionInfoXim FSessionInfoType;
#else
		typedef FOnlineSessionInfoLive FSessionInfoType;
#endif
		// Grab the required GUIDs from the game session if it exists
		TSharedPtr<FSessionInfoType> SessionInfoLive = nullptr;
		auto SessionInterface = Subsystem->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			auto NamedSession = SessionInterface->GetNamedSession(GameSessionName);
			if (NamedSession != nullptr)
			{
				SessionInfoLive = StaticCastSharedPtr<FSessionInfoType>(NamedSession->SessionInfo);
			}
		}

		if ( FieldParmName == TEXT( "RoundId" ) )
		{
			if ( Event->Fields.GetFieldDescByIndex( i ).Type != EtxFieldType_GUID )
			{
				UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive::TriggerEvent: Event '%s' cannot convert parameter '%s' from string to %s." ), EventName, *FieldParmName.ToString(), FieldParmTypeName );
				return false;
			}

			FGuid RoundId;		

			if ( SessionInfoLive.IsValid() )
			{
				RoundId = SessionInfoLive->GetRoundId();
			}

			InternalParms.AddParm( &RoundId, sizeof( RoundId ) );
			continue;
		}

		if ( FieldParmName == TEXT( "MultiplayerCorrelationId" ) )
		{
			if ( Event->Fields.GetFieldDescByIndex( i ).Type != EtxFieldType_UnicodeString )
			{
				UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive::TriggerEvent: Event '%s' cannot convert parameter '%s' from string to %s." ), EventName, *FieldParmName.ToString(), FieldParmTypeName );
				return false;
			}

			FString MultiplayerCorrelationId( TEXT( "0" ) );

			// Grab the correlation id from the game session if it exists
			if ( SessionInfoLive.IsValid() )
			{
				const WCHAR* SessionProvidedId = SessionInfoLive->GetMultiplayerCorrelationId();
				if (SessionProvidedId != nullptr)
				{
					MultiplayerCorrelationId = SessionProvidedId;
				}
			}

			InternalParms.AddParm( *MultiplayerCorrelationId, ( MultiplayerCorrelationId.Len() + 1 ) * sizeof( WCHAR ) );
			continue;
		}
// @ATG_CHANGE : END

		// Not a built in parm type, expect the parameter to exist
		const FVariantData* Value = Parms.Find( FieldParmName );

		if ( Value == NULL )
		{
			UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive::TriggerEvent: Event '%s' expected a parameter named '%s' but was not found." ), EventName, *FieldParmName.ToString() );
			return false;
		}
		
		
#define HANDLE_POD_PARAMETER2( CppType, DefaultValue, EtxType, CppTypeOut )		\
	case EtxType:																\
	{																			\
		CppType	LocalValue = DefaultValue;										\
		Value->GetValue( LocalValue );											\
		CppTypeOut LocalValueOut = (CppTypeOut)LocalValue;						\
		if ( LocalValueOut != LocalValue )										\
		{																		\
			UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive::TriggerEvent: Event '%s' parameter cast lost information. Original Value: %s, cast from %s to %s" ), EventName, *Value->ToString(), Value->GetTypeString(), TEXT( PREPROCESSOR_TO_STRING( EtxType ) ) );	\
		}																		\
		InternalParms.AddParm( &LocalValueOut, sizeof( LocalValueOut ) );		\
		break;																	\
	}																			\

#define HANDLE_POD_PARAMETER1( KeyValueType, CppType, DefaultValue )								\
	case EOnlineKeyValuePairDataType::##KeyValueType:												\
	{																								\
		switch ( Event->Fields.GetFieldDescByIndex( i ).Type )										\
		{																							\
			HANDLE_POD_PARAMETER2( CppType, DefaultValue, EtxFieldType_Int8, int8 );		\
			HANDLE_POD_PARAMETER2( CppType, DefaultValue, EtxFieldType_UInt8, uint8 );		\
			HANDLE_POD_PARAMETER2( CppType, DefaultValue, EtxFieldType_Int16, int16 );		\
			HANDLE_POD_PARAMETER2( CppType, DefaultValue, EtxFieldType_UInt16, uint16 );	\
			HANDLE_POD_PARAMETER2( CppType, DefaultValue, EtxFieldType_Int32, int32 );		\
			HANDLE_POD_PARAMETER2( CppType, DefaultValue, EtxFieldType_UInt32, uint32 );	\
			HANDLE_POD_PARAMETER2( CppType, DefaultValue, EtxFieldType_Int64, int64 );		\
			HANDLE_POD_PARAMETER2( CppType, DefaultValue, EtxFieldType_UInt64, uint64 );	\
			HANDLE_POD_PARAMETER2( CppType, DefaultValue, EtxFieldType_Float, float );		\
			HANDLE_POD_PARAMETER2( CppType, DefaultValue, EtxFieldType_Double, double );	\
			default:																				\
			{																						\
				UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive::TriggerEvent: Event '%s' cannot convert parameter '%s' from %s to %s." ), EventName, *FieldParmName.ToString(), Value->GetTypeString(), FieldParmTypeName );	\
				return false;																		\
		}																							\
		}																							\
		break;																						\
	}																								\

		// Start with the type of FVariantData, then use macro magic to cast to etx expectation
		switch ( Value->GetType() )
		{
			// Use macro to handle basic POD types, which will give some flexibility to convert between them
			HANDLE_POD_PARAMETER1( Int32, int32, 0 );
			HANDLE_POD_PARAMETER1( UInt32, uint32, 0 );
			HANDLE_POD_PARAMETER1( Int64, uint64, 0 );		// FIXME: FVariantData bug, really uint64, not signed
			HANDLE_POD_PARAMETER1( Float, float, 0.0f );
			HANDLE_POD_PARAMETER1( Double, double, 0 );
			
			// Handle string specifically
			case EOnlineKeyValuePairDataType::String:
			{
				if ( Event->Fields.GetFieldDescByIndex( i ).Type != EtxFieldType_UnicodeString )
				{
					UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive::TriggerEvent: Event '%s' cannot convert parameter '%s' from %s to %s." ), EventName, *FieldParmName.ToString(), Value->GetTypeString(), FieldParmTypeName );
					return false;
				}

				FString	LocalValue;
				Value->GetValue( LocalValue );
				InternalParms.AddParm( *LocalValue, ( LocalValue.Len() + 1 ) * sizeof( WCHAR ) );
				break;
			}

			// Handle bool separately to avoid a ton of warnings from the macro
			case EOnlineKeyValuePairDataType::Bool:
			{
				if ( Event->Fields.GetFieldDescByIndex( i ).Type != EtxFieldType_Boolean )
				{
					UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive::TriggerEvent: Event '%s' cannot convert parameter '%s' from %s to %s." ), EventName, *FieldParmName.ToString(), Value->GetTypeString(), FieldParmTypeName );
					return false;
				}

				bool	LocalValue;
				Value->GetValue( LocalValue );

				BOOL ParamVal = LocalValue ? TRUE : FALSE;
				InternalParms.AddParm( &ParamVal, sizeof(ParamVal) );
				break;
			}
			
			// Unexpected parameter type
			default:
			{
				UE_LOG_ONLINE( Warning, TEXT( "FOnlineEventsLive::TriggerEvent: Event '%s' unsupported parameter type: %s" ), EventName, Value->GetTypeString() );
				return false;
			}
		}
	}
	
	return TriggerEvent( EventName, InternalParms );
}

void FOnlineEventsLive::SetPlayerSessionId(const FUniqueNetId& PlayerId, const FGuid& PlayerSessionId)
{
	PlayerSessionIds.Add(FUniqueNetIdLive(PlayerId), PlayerSessionId);
}

// @ATG_CHANGE : BEGIN UWP LIVE support
#else

FOnlineEventsLive::FOnlineEventsLive(FOnlineSubsystemLive* InSubsystem) :
	Subsystem(InSubsystem)
{
}

FOnlineEventsLive::~FOnlineEventsLive()
{
}

bool FOnlineEventsLive::TriggerEvent(const FUniqueNetId& PlayerId, const TCHAR* EventName, const FOnlineEventParms& Parms)
{
	auto LiveContext = Subsystem->GetLiveContext(PlayerId);
	try
	{
		auto Dimensions = ref new Windows::Foundation::Collections::PropertySet();
		auto Measurements = ref new Windows::Foundation::Collections::PropertySet();

		for (auto &Param : Parms)
		{
			switch (Param.Value.GetType())
			{
			case EOnlineKeyValuePairDataType::Float:
			{
				float Value;
				Param.Value.GetValue(Value);
				Measurements->Insert(ref new Platform::String(*Param.Key.ToString()), Value);
			}
			break;
			case EOnlineKeyValuePairDataType::Double:
			{
				double Value;
				Param.Value.GetValue(Value);
				Measurements->Insert(ref new Platform::String(*Param.Key.ToString()), Value);
			}
			break;
			case EOnlineKeyValuePairDataType::Int32:
			{
				int32 Value;
				Param.Value.GetValue(Value);
				Dimensions->Insert(ref new Platform::String(*Param.Key.ToString()), Value);
			}
			break;
			case EOnlineKeyValuePairDataType::UInt32:
			{
				uint32 Value;
				Param.Value.GetValue(Value);
				Dimensions->Insert(ref new Platform::String(*Param.Key.ToString()), Value);
			}
			break;
			case EOnlineKeyValuePairDataType::Int64:
			{
				int64 Value;
				Param.Value.GetValue(Value);
				Dimensions->Insert(ref new Platform::String(*Param.Key.ToString()), Value);
			}
			break;
			case EOnlineKeyValuePairDataType::UInt64:
			{
				uint64 Value;
				Param.Value.GetValue(Value);
				Dimensions->Insert(ref new Platform::String(*Param.Key.ToString()), Value);
			}
			break;
			case EOnlineKeyValuePairDataType::Bool:
			{
				bool Value;
				Param.Value.GetValue(Value);
				Dimensions->Insert(ref new Platform::String(*Param.Key.ToString()), Value);
			}
			break;
			case EOnlineKeyValuePairDataType::String:
			{
				FString Value;
				Param.Value.GetValue(Value);
				Dimensions->Insert(ref new Platform::String(*Param.Key.ToString()), ref new Platform::String(*Value));
			}
			break;

			default:
				break;
			}
		}

		// FIXME: Get this from session
		const FGuid* FoundPlayerSessionId = PlayerSessionIds.Find(FUniqueNetIdLive(PlayerId));
		if (FoundPlayerSessionId != nullptr)
		{
			Dimensions->Insert("PlayerSessionId", Platform::Guid(*reinterpret_cast<GUID*>(&FoundPlayerSessionId)));
		}

		LiveContext->EventsService->WriteInGameEvent(ref new Platform::String(EventName), Dimensions, Measurements);
		return true;
	}
	catch (Platform::Exception ^)
	{
		return false;
	}
}

void FOnlineEventsLive::SetPlayerSessionId(const FUniqueNetId& PlayerId, const FGuid& PlayerSessionId)
{
	PlayerSessionIds.Add(FUniqueNetIdLive(PlayerId), PlayerSessionId);
}

#endif
// @ATG_CHANGE : END
