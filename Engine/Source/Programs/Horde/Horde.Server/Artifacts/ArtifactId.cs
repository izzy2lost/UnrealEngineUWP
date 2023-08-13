// Copyright Epic Games, Inc. All Rights Reserved.

using System.ComponentModel;
using EpicGames.Horde;
using Horde.Server.Utilities;
using MongoDB.Bson;

namespace Horde.Server.Artifacts
{
	/// <summary>
	/// Unique id for an artifact
	/// </summary>
	/// <param name="Id">Identifier for the artifact</param>
	[TypeConverter(typeof(BinaryIdTypeConverter<ArtifactId, ArtifactIdConverter>))]
	[BinaryIdConverter(typeof(ArtifactIdConverter))]
	public record struct ArtifactId(BinaryId Id)
	{
		/// <summary>
		/// Creates a new random artifact id
		/// </summary>
		/// <returns>New artifact id</returns>
		public static ArtifactId GenerateNewId() => new ArtifactId(BinaryIdUtils.CreateNew());

		/// <inheritdoc cref="ObjectId.Parse(System.String)"/>
		public static ArtifactId Parse(string text) => new ArtifactId(BinaryId.Parse(text));

		/// <inheritdoc/>
		public override string ToString() => Id.ToString();
	}

	/// <summary>
	/// Converter class to and from ObjectId values
	/// </summary>
	class ArtifactIdConverter : BinaryIdConverter<ArtifactId>
	{
		/// <inheritdoc/>
		public override ArtifactId FromBinaryId(BinaryId id) => new ArtifactId(id);

		/// <inheritdoc/>
		public override BinaryId ToBinaryId(ArtifactId value) => value.Id;
	}
}
