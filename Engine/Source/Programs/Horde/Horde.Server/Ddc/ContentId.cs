// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.ComponentModel;
using System.Globalization;
using System.IO;
using System.Text.Json;
using System.Text.Json.Serialization;
using EpicGames.Core;
using EpicGames.Serialization;

#pragma warning disable CS1591

namespace Horde.Server.Ddc
{
	/// <summary>
	/// Identifier for a compressed buffer in the store
	/// </summary>
	[TypeConverter(typeof(ContentIdTypeConverter))]
	[JsonConverter(typeof(ContentIdJsonConverter))]
	[CbConverter(typeof(ContentIdCbConverter))]
	public readonly struct ContentId : IEquatable<ContentId>
	{
		/// <summary>
		/// Hash of the blob
		/// </summary>
		public IoHash Hash { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public ContentId(IoHash hash) => Hash = hash;

		/// <summary>
		/// Parses a <see cref="ContentId"/> from a string
		/// </summary>
		public static ContentId Parse(string text) => new ContentId(IoHash.Parse(text));

		/// <inheritdoc/>
		public override readonly int GetHashCode() => Hash.GetHashCode();

		/// <inheritdoc/>
		public readonly bool Equals(ContentId other) => Hash.Equals(other.Hash);

		/// <inheritdoc/>
		public override readonly bool Equals(object? obj) => obj is ContentId contentId && Equals(contentId);

		/// <inheritdoc/>
		public override readonly string ToString() => Hash.ToString();

		/// <inheritdoc cref="IoHash.op_Equality"/>
		public static bool operator ==(ContentId left, ContentId right) => left.Hash == right.Hash;

		/// <inheritdoc cref="IoHash.op_Inequality"/>
		public static bool operator !=(ContentId left, ContentId right) => !(left == right);

		/// <summary>
		/// Constructs a ContentId from an IoHash
		/// </summary>
		public static ContentId FromIoHash(IoHash hash) => new ContentId(hash);

		/// <summary>
		/// Constructs a ContentId from an IoHash
		/// </summary>
		public static ContentId FromBlobId(BlobId blobId) => FromIoHash(blobId.AsIoHash());

		/// <summary>
		/// Converts a ContentId to IoHash
		/// </summary>
		public IoHash AsIoHash() => Hash;

		/// <summary>
		/// Converts a ContentId to BlobId
		/// </summary>
		public BlobId AsBlobIdentifier() => BlobId.FromIoHash(Hash);
	}

	/// <summary>
	/// Converts from <see cref="ContentId"/> instances to other types
	/// </summary>
	public class ContentIdTypeConverter : TypeConverter
	{
		/// <inheritdoc/>
		public override bool CanConvertFrom(ITypeDescriptorContext? context, Type sourceType)
		{
			if (sourceType == typeof(string))
			{
				return true;
			}
			return base.CanConvertFrom(context, sourceType);
		}

		/// <inheritdoc/>
		public override object? ConvertFrom(ITypeDescriptorContext? context, CultureInfo? culture, object value)
		{
			if (value is string s)
			{
				return ContentId.Parse(s);
			}

			return base.ConvertFrom(context, culture, value);
		}

		/// <inheritdoc/>
		public override bool CanConvertTo(ITypeDescriptorContext? context, Type? destinationType)
		{
			if (destinationType == typeof(string))
			{
				return true;
			}
			return base.CanConvertTo(context, destinationType);
		}

		/// <inheritdoc/>
		public override object? ConvertTo(ITypeDescriptorContext? context, CultureInfo? culture, object? value, Type destinationType)
		{
			if (destinationType == typeof(string))
			{
				return value?.ToString();
			}

			return base.ConvertTo(context, culture, value, destinationType);
		}
	}

	/// <summary>
	/// Converts from <see cref="ContentId"/> instances to other types
	/// </summary>
	public class ContentIdJsonConverter : JsonConverter<ContentId>
	{
		/// <inheritdoc/>
		public override ContentId Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options)
		{
			string? str = reader.GetString();
			if (str == null)
			{
				throw new InvalidDataException("Unable to parse content id");
			}

			return ContentId.Parse(str);
		}

		/// <inheritdoc/>
		public override void Write(Utf8JsonWriter writer, ContentId value, JsonSerializerOptions options)
		{
			writer.WriteStringValue(value.ToString());
		}
	}

	/// <summary>
	/// Serializes <see cref="ContentId"/> instances to compact binary
	/// </summary>
	public class ContentIdCbConverter : CbConverter<ContentId>
	{
		/// <inheritdoc/>
		public override ContentId Read(CbField field) => new ContentId(field.AsHash());

		/// <inheritdoc/>
		public override void Write(CbWriter writer, ContentId value) => writer.WriteBinaryAttachmentValue(value.Hash);

		/// <inheritdoc/>
		public override void WriteNamed(CbWriter writer, CbFieldName name, ContentId value) => writer.WriteBinaryAttachment(name, value.Hash);
	}
}
