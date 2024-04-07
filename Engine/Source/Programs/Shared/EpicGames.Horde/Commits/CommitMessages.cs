// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using EpicGames.Horde.Users;

#pragma warning disable CA2227

namespace EpicGames.Horde.Commits
{
	/// <summary>
	/// Information about a commit
	/// </summary>
	public class GetCommitResponse
	{
		/// <summary>
		/// The source changelist number
		/// </summary>
		public int Number { get; set; }

		/// <summary>
		/// Name of the user that authored this change [DEPRECATED]
		/// </summary>
		public string Author { get; set; }

		/// <summary>
		/// Information about the user that authored this change
		/// </summary>
		public GetThinUserInfoResponse AuthorInfo { get; set; }

		/// <summary>
		/// The description text
		/// </summary>
		public string Description { get; set; }

		/// <summary>
		/// Tags for this commit
		/// </summary>
		public List<CommitTag>? Tags { get; set; }

		/// <summary>
		/// List of files that were modified, relative to the stream base
		/// </summary>
		public List<string>? Files { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		public GetCommitResponse(int number, GetThinUserInfoResponse authorInfo, string description)
		{
			Number = number;
			Author = authorInfo.Name;
			AuthorInfo = authorInfo;
			Description = description;
		}
	}
}
