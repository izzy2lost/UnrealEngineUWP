// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Serialization;
using System;
using System.Buffers.Binary;
using System.ComponentModel;
using System.Globalization;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading;

namespace EpicGames.Horde.Storage.Bundles
{
	/// <summary>
	/// Unique identifier for a blob, as a utf-8 string. Clients should not assume any internal structure to this identifier; it only
	/// has meaning to the <see cref="IStorageClient"/> implementation.
	/// </summary>
	[JsonConverter(typeof(BundleLocatorJsonConverter))]
	[TypeConverter(typeof(BundleLocatorTypeConverter))]
	[CbConverter(typeof(BundleLocatorCbConverter))]
	public struct BundleLocator : IEquatable<BundleLocator>
	{
		/// <summary>
		/// Dummy enum to allow invoking the constructor which takes a sanitized full path
		/// </summary>
		public enum Sanitize
		{
			/// <summary>
			/// Dummy value
			/// </summary>
			None
		}

		/// <summary>
		/// Empty blob locator
		/// </summary>
		public static BundleLocator Empty { get; } = default;

		/// <summary>
		/// Identifier for the blob
		/// </summary>
		public Utf8String Path { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public BundleLocator(string path)
			: this(path.AsSpan())
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="path">Path to the blob</param>
		public BundleLocator(ReadOnlySpan<char> path)
			: this(new Utf8String(path))
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public BundleLocator(Utf8String path)
		{
			Path = path;
			ValidatePathArgument(nameof(path), path.Span);
		}

		/// <summary>
		/// Validates a given string as a blob id
		/// </summary>
		/// <param name="name">Name of the argument</param>
		/// <param name="text">String to validate</param>
		public static void ValidatePathArgument(string name, ReadOnlySpan<byte> text)
		{
			if (text.Length == 0)
			{
				throw new ArgumentException("Blob paths cannot be empty", name);
			}
			if (text[^1] == '/')
			{
				throw new ArgumentException("Blob paths cannot start or end with a slash", name);
			}

			int lastSlashIdx = -1;

			for (int idx = 0; idx < text.Length; idx++)
			{
				if (text[idx] == '/')
				{
					if (lastSlashIdx == idx - 1)
					{
						throw new ArgumentException("Leading and consecutive slashes are not permitted in blob paths", name);
					}
					else
					{
						lastSlashIdx = idx;
					}
				}
				else
				{
					if (!IsValidChar(text[idx]))
					{
						throw new ArgumentException($"'{(char)text[idx]} is not a valid blob path character", name);
					}
				}
			}
		}

		static readonly uint[] s_validChars = CreateValidCharsArray();

		static uint[] CreateValidCharsArray()
		{
			const string ValidChars = "0123456789abcdefghijklmnopqrstuvwxyz_#/$%-";

			uint[] validChars = new uint[256 / 8];
			for (int idx = 0; idx < ValidChars.Length; idx++)
			{
				int index = ValidChars[idx];
				validChars[index / 32] |= 1U << (index & 31);
			}

			return validChars;
		}

		static bool IsValidChar(byte character)
		{
			return (s_validChars[character / 32] & (1U << (character & 31))) != 0;
		}

		static bool IsValidChar(char character) => (uint)character < 0x80 && IsValidChar((byte)character);

		/// <summary>
		/// Checks whether this blob id is valid
		/// </summary>
		/// <returns>True if the identifier is valid</returns>
		public bool IsValid() => Path.Length > 0;

		static ulong s_uniqueId = GetUniqueIdSeed();

		/// <summary>
		/// Static constructor
		/// </summary>
		static ulong GetUniqueIdSeed()
		{
			Random rnd = new Random(HashCode.Combine(DateTime.UtcNow.Ticks, Environment.ProcessId, Environment.TickCount64, Environment.MachineName.GetHashCode(StringComparison.Ordinal)));

			Span<byte> process = stackalloc byte[8];
			rnd.NextBytes(process);

			return BinaryPrimitives.ReadUInt64LittleEndian(process);
		}

		/// <summary>
		/// Create a unique locator with the given prefix
		/// </summary>
		/// <param name="basePath">Prefix for the locator</param>
		public static BundleLocator CreateUnique(Utf8String basePath) => CreateUnique(basePath, DateTime.UtcNow);

		/// <summary>
		/// Create a unique locator with the given prefix
		/// </summary>
		/// <param name="basePath">Path prefix for the new locator</param>
		/// <param name="utcNow">Current time</param>
		public static BundleLocator CreateUnique(Utf8String basePath, DateTime utcNow)
		{
			byte[] data;
			if (basePath.Length == 0)
			{
				data = new byte[24];
			}
			else
			{
				data = new byte[basePath.Length + 1 + 24];
				basePath.Span.CopyTo(data);
				data[basePath.Length] = (byte)'/';
			}

			Span<byte> output = data.AsSpan(data.Length - 24);

			uint timestamp = (uint)((utcNow - DateTime.UnixEpoch).Ticks / TimeSpan.TicksPerSecond);
			StringUtils.FormatUtf8HexString(timestamp, output);

			ulong seed = Interlocked.Increment(ref s_uniqueId);
			StringUtils.FormatUtf8HexString(seed, output.Slice(8));

			return new BundleLocator(new Utf8String(data));
		}

		/// <inheritdoc/>
		public override bool Equals(object? obj) => obj is BundleLocator other && Equals(other);

		/// <inheritdoc/>
		public bool Equals(BundleLocator locator) => Path == locator.Path;

		/// <inheritdoc/>
		public override int GetHashCode() => Path.GetHashCode();

		/// <summary>
		/// Checks whether this blob is within the given folder
		/// </summary>
		/// <param name="folderName">Name of the folder</param>
		/// <returns>True if the the blob id is within the given folder</returns>
		public bool WithinFolder(Utf8String folderName)
		{
			Utf8String path = Path;
			return path.Length > folderName.Length && path.StartsWith(folderName) && path[folderName.Length] == '/';
		}

		/// <inheritdoc/>
		public override string ToString() => Path.ToString();

		/// <inheritdoc/>
		public static bool operator ==(BundleLocator lhs, BundleLocator rhs) => lhs.Equals(rhs);

		/// <inheritdoc/>
		public static bool operator !=(BundleLocator lhs, BundleLocator rhs) => !lhs.Equals(rhs);
	}

	/// <summary>
	/// Type converter for BlobId to and from JSON
	/// </summary>
	sealed class BundleLocatorJsonConverter : JsonConverter<BundleLocator>
	{
		/// <inheritdoc/>
		public override BundleLocator Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options) => new BundleLocator(new Utf8String(reader.GetUtf8String().ToArray()));

		/// <inheritdoc/>
		public override void Write(Utf8JsonWriter writer, BundleLocator value, JsonSerializerOptions options) => writer.WriteStringValue(value.Path.Span);
	}

	/// <summary>
	/// Type converter from strings to BlobId objects
	/// </summary>
	sealed class BundleLocatorTypeConverter : TypeConverter
	{
		/// <inheritdoc/>
		public override bool CanConvertFrom(ITypeDescriptorContext? context, Type sourceType)
		{
			return sourceType == typeof(string);
		}

		/// <inheritdoc/>
		public override object ConvertFrom(ITypeDescriptorContext? context, CultureInfo? culture, object? value)
		{
			return new BundleLocator((string)value!);
		}
	}

	/// <summary>
	/// Type converter to compact binary
	/// </summary>
	sealed class BundleLocatorCbConverter : CbConverterBase<BundleLocator>
	{
		/// <inheritdoc/>
		public override BundleLocator Read(CbField field) => new BundleLocator(field.AsUtf8String());

		/// <inheritdoc/>
		public override void Write(CbWriter writer, BundleLocator value) => writer.WriteUtf8StringValue(value.Path);

		/// <inheritdoc/>
		public override void WriteNamed(CbWriter writer, Utf8String name, BundleLocator value) => writer.WriteUtf8String(name, value.Path);
	}

	/// <summary>
	/// Extension methods for blob locators
	/// </summary>
	public static class BlobLocatorExtensions
	{
		/// <summary>
		/// Deserialize a blob locator
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		/// <returns>The blob id that was read</returns>
		public static BundleLocator ReadBlobLocator(this IMemoryReader reader)
		{
			return new BundleLocator(reader.ReadUtf8String());
		}

		/// <summary>
		/// Serialize a blob locator
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		/// <param name="value">Value to serialize</param>
		public static void WriteBlobLocator(this IMemoryWriter writer, BundleLocator value)
		{
			writer.WriteUtf8String(value.Path);
		}
	}
}
