// Copyright Epic Games, Inc. All Rights Reserved.

using System.ComponentModel;
using EpicGames.Core;

namespace EpicGames.Horde.Accounts
{
	/// <summary>
	/// Identifier for a user account
	/// </summary>
	/// <param name="Id">Id to construct from</param>
	[LogValueType]
	[JsonSchemaString]
	[TypeConverter(typeof(BinaryIdTypeConverter<AccountId, AccountIdConverter>))]
	[BinaryIdConverter(typeof(AccountIdConverter))]
	public record struct AccountId(BinaryId Id)
	{
		/// <inheritdoc cref="BinaryId.Parse(System.String)"/>
		public static AccountId Parse(string text) => new AccountId(BinaryId.Parse(text));

		/// <inheritdoc/>
		public override string ToString() => Id.ToString();
	}

	/// <summary>
	/// Converter to and from <see cref="BinaryId"/> instances.
	/// </summary>
	class AccountIdConverter : BinaryIdConverter<AccountId>
	{
		/// <inheritdoc/>
		public override AccountId FromBinaryId(BinaryId id) => new AccountId(id);

		/// <inheritdoc/>
		public override BinaryId ToBinaryId(AccountId value) => value.Id;
	}
}
