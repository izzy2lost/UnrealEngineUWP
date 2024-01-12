// Copyright Epic Games, Inc. All Rights Reserved.

#pragma warning disable CA1027 // Use [Flags] attribute.
#pragma warning disable CA1069 // Overlapping constants in enum.

namespace EpicGames.Horde.Compute
{
	/// <summary>
	/// Version number for the compute protocol
	/// </summary>
	public enum ComputeProtocol
	{
		/// <summary>
		/// No version specified
		/// </summary>
		Unknown,

		/// <summary>
		/// Initial version number
		/// </summary>
		Initial,

		/// <summary>
		/// Constant for the latest protocol version
		/// </summary>
		Latest = (int)Initial
	}
}
