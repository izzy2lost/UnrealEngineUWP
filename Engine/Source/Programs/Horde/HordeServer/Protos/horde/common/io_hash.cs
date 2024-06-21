// Copyright Epic Games, Inc. All Rights Reserved.

namespace HordeCommon.Wrapper
{
	using NativeIoHash = EpicGames.Core.IoHash;

#pragma warning disable CS1591 // Missing XML comment for publicly visible type or member
	partial class RpcIoHash
	{
		public static implicit operator NativeIoHash(RpcIoHash hash)
		{
			return new NativeIoHash(hash.Data.ToByteArray());
		}

		public static implicit operator RpcIoHash(NativeIoHash hash)
		{
			RpcIoHash result = new RpcIoHash();
			result.Data = Google.Protobuf.ByteString.CopyFrom(hash.ToByteArray());
			return result;
		}
	}
#pragma warning restore CS1591 // Missing XML comment for publicly visible type or member
}
