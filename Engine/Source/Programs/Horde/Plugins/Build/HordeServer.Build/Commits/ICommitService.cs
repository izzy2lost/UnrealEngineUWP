// Copyright Epic Games, Inc. All Rights Reserved.

using HordeServer.Streams;

namespace HordeServer.Commits
{
	/// <summary>
	/// Provides information about commits to a stream
	/// </summary>
	public interface ICommitService
	{
		/// <summary>
		/// Gets a commit collection for the given stream
		/// </summary>
		/// <param name="streamConfig">Stream to get commits for</param>
		/// <returns>Collection object</returns>
		ICommitCollection GetCollection(StreamConfig streamConfig);
	}
}
