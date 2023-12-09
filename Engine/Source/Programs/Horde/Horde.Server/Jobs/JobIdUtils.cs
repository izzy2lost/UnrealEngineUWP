// Copyright Epic Games, Inc. All Rights Reserved.

using Horde.Server.Utilities;

namespace Horde.Server.Jobs
{
	/// <summary>
	/// Utility methods for <see cref="JobId"/>
	/// </summary>
	public static class JobIdUtils
	{
		/// <summary>
		/// Creates a new, random JobId
		/// </summary>
		public static JobId GenerateNewId()
		{
			return new JobId(BinaryIdUtils.CreateNew());
		}
	}
}
