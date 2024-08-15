// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Artifacts;
using HordeServer.Acls;

namespace HordeServer.Artifacts
{
	/// <summary>
	/// Configuration for an artifact
	/// </summary>
	public class ArtifactTypeConfig
	{
		/// <summary>
		/// Legacy 'Name' property
		/// </summary>
		[Obsolete("Use Type instead")]
		public ArtifactType Name
		{
			get => Type;
			set => Type = value;
		}

		/// <summary>
		/// Name of the artifact type
		/// </summary>
		public ArtifactType Type { get; set; }

		/// <summary>
		/// Acl for the artifact type
		/// </summary>
		public AclConfig? Acl { get; set; }

		/// <summary>
		/// Number of artifacts to retain
		/// </summary>
		public int? KeepCount { get; set; }

		/// <summary>
		/// Number of days to retain artifacts of this type
		/// </summary>
		public int? KeepDays { get; set; }
	}
}
