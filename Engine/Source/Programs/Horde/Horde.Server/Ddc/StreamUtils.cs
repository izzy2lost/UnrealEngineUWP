// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using System.Threading.Tasks;
using EpicGames.AspNet;
using Microsoft.AspNetCore.Http;

#pragma warning disable CS1591
#pragma warning disable MA0032 // Use an overload with a CancellationToken

namespace Horde.Server.Ddc
{
	public static class StreamUtils
	{
		public static async Task<byte[]> ToByteArrayAsync(this Stream s)
		{
			try
			{
				await using MemoryStream ms = new MemoryStream();
				await s.CopyToAsync(ms);
				return ms.ToArray();
			}
			catch (BadHttpRequestException e)
			{
				ClientSendSlowExceptionUtil.MaybeThrowSlowSendException(e);
				throw;
			}
		}
	}
}
