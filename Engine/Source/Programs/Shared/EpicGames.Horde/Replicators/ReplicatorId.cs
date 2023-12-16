// Copyright Epic Games, Inc. All Rights Reserved.

using System.ComponentModel;
using EpicGames.Core;
using EpicGames.Horde.Streams;
using EpicGames.Serialization;

namespace EpicGames.Horde.Replicators
{
	/// <summary>
	/// Unique identifier for a replicator across all streams
	/// </summary>
	public record struct ReplicatorId(StreamId StreamId, StreamReplicatorId StreamReplicatorId)
	{
		/// <inheritdoc/>
		public override string ToString() => $"{StreamId}:{StreamReplicatorId}";
	}

	/// <summary>
	/// Identifier for a replicator
	/// </summary>
	/// <param name="Id">Id to construct from</param>
	[LogValueType]
	[JsonSchemaString]
	[TypeConverter(typeof(StringIdTypeConverter<StreamReplicatorId, StreamReplicatorIdConverter>))]
	[StringIdConverter(typeof(StreamReplicatorIdConverter))]
	[CbConverter(typeof(StringIdCbConverter<StreamReplicatorId, StreamReplicatorIdConverter>))]
	public record struct StreamReplicatorId(StringId Id)
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public StreamReplicatorId(string id) : this(new StringId(id))
		{
		}

		/// <inheritdoc cref="StringId.IsEmpty"/>
		public bool IsEmpty => Id.IsEmpty;

		/// <inheritdoc/>
		public override string ToString() => Id.ToString();
	}

	/// <summary>
	/// Converter to and from <see cref="StringId"/> instances.
	/// </summary>
	public class StreamReplicatorIdConverter : StringIdConverter<StreamReplicatorId>
	{
		/// <inheritdoc/>
		public override StreamReplicatorId FromStringId(StringId id) => new StreamReplicatorId(id);

		/// <inheritdoc/>
		public override StringId ToStringId(StreamReplicatorId value) => value.Id;
	}
}
