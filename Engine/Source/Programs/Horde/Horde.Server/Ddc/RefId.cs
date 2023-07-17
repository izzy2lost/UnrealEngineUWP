// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.ComponentModel;
using System.Globalization;
using System.Text.Json;
using System.Text.Json.Serialization;
using EpicGames.Core;
using EpicGames.Serialization;

namespace Horde.Server.Ddc
{
	/// <summary>
	/// Identifier for a DDC ref. Wrapper around an IoHash.
	/// </summary>
	[JsonConverter(typeof(RefIdJsonConverter))]
	[TypeConverter(typeof(RefIdTypeConverter))]
	[CbConverter(typeof(RefIdCbConverter))]
	public readonly struct RefId : IEquatable<RefId>
	{
		/// <summary>
		/// Hash of the ref name
		/// </summary>
		public IoHash Hash { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public RefId(IoHash hash) => Hash = hash;

		/// <summary>
		/// Parses a ref id from a string
		/// </summary>
		public static RefId Parse(string str) => new RefId(IoHash.Parse(str));

		/// <inheritdoc/>
		public override readonly bool Equals(object? obj) => obj is RefId id && Hash.Equals(id.Hash);

		/// <inheritdoc/>
		public override readonly int GetHashCode() => Hash.GetHashCode();

		/// <inheritdoc/>
		public readonly bool Equals(RefId other) => Hash.Equals(other.Hash);

		/// <inheritdoc/>
		public override readonly string ToString() => Hash.ToString();

		/// <inheritdoc cref="IoHash.op_Equality"/>
		public static bool operator ==(RefId left, RefId right) => left.Hash == right.Hash;

		/// <inheritdoc cref="IoHash.op_Inequality"/>
		public static bool operator !=(RefId left, RefId right) => left.Hash != right.Hash;
	}

	/// <summary>
	/// Type converter for RefId to and from JSON
	/// </summary>
	sealed class RefIdJsonConverter : JsonConverter<RefId>
	{
		/// <inheritdoc/>
		public override RefId Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options) => RefId.Parse(reader.GetString() ?? String.Empty);

		/// <inheritdoc/>
		public override void Write(Utf8JsonWriter writer, RefId value, JsonSerializerOptions options) => writer.WriteStringValue(value.ToString());
	}

	/// <summary>
	/// Type converter from strings to RefId objects
	/// </summary>
	sealed class RefIdTypeConverter : TypeConverter
	{
		/// <inheritdoc/>
		public override bool CanConvertFrom(ITypeDescriptorContext? context, Type? sourceType) => sourceType == typeof(string);

		/// <inheritdoc/>
		public override object ConvertFrom(ITypeDescriptorContext? context, CultureInfo? culture, object? value) => RefId.Parse((string)value!);
	}

	/// <summary>
	/// Type converter to compact binary objects
	/// </summary>
	sealed class RefIdCbConverter : CbConverterBase<RefId>
	{
		public override RefId Read(CbField field) => RefId.Parse(field.AsString());

		/// <inheritdoc/>
		public override void Write(CbWriter writer, RefId value) => writer.WriteStringValue(value.ToString());

		/// <inheritdoc/>
		public override void WriteNamed(CbWriter writer, Utf8String name, RefId value) => writer.WriteString(name, value.ToString());
	}
}
