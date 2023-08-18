// Copyright Epic Games, Inc. All Rights Reserved.

using System.ComponentModel;
using System.Text.Json;
using EpicGames.Core;

namespace EpicGames.Horde.Api
{
	/// <summary>
	/// Identifier for a user
	/// </summary>
	/// <param name="Id">Id to construct from</param>
	[JsonSchemaString]
	[TypeConverter(typeof(BinaryIdTypeConverter<LeaseId, LeaseIdConverter>))]
	[BinaryIdConverter(typeof(LeaseIdConverter))]
	[LogValueFormatter(typeof(LeaseIdLogFormatter))]
	public record struct LeaseId(BinaryId Id)
	{
		/// <inheritdoc cref="BinaryId.TryParse(System.String, out BinaryId)"/>
		public static bool TryParse(string text, out LeaseId leaseId)
		{
			BinaryId objectId;
			if (BinaryId.TryParse(text, out objectId))
			{
				leaseId = new LeaseId(objectId);
				return true;
			}
			else
			{
				leaseId = default;
				return false;
			}
		}

		/// <inheritdoc cref="BinaryId.Parse(System.String)"/>
		public static LeaseId Parse(string text) => new LeaseId(BinaryId.Parse(text));

		/// <inheritdoc/>
		public override string ToString() => Id.ToString();
	}

	/// <summary>
	/// Converter to and from <see cref="BinaryId"/> instances.
	/// </summary>
	class LeaseIdConverter : BinaryIdConverter<LeaseId>
	{
		/// <inheritdoc/>
		public override LeaseId FromBinaryId(BinaryId id) => new LeaseId(id);

		/// <inheritdoc/>
		public override BinaryId ToBinaryId(LeaseId value) => value.Id;
	}

	/// <summary>
	/// Formats a LeaseId as a typed log value
	/// </summary>
	class LeaseIdLogFormatter : ILogValueFormatter
	{
		/// <inheritdoc/>
		public void Format(object value, Utf8JsonWriter writer)
		{
			writer.WriteStartObject();
			writer.WriteString(LogEventPropertyName.Type, "LeaseId");
			writer.WriteString(LogEventPropertyName.Text, ((LeaseId)value).ToString());
			writer.WriteEndObject();
		}
	}
}
