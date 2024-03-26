// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Server.Ddc;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using OpenTelemetry.Trace;

namespace Horde.Server.Tests.Ddc.UnitTests
{
	[TestClass]
	public class CompressedBufferTests
	{

		[TestMethod]
		public async Task CompressAndDecompressAsync()
		{
			byte[] bytes = Encoding.UTF8.GetBytes("this is a test string");

			CompressedBufferUtils bufferUtils = new(TracerProvider.Default.GetTracer("TestTracer"));

			using MemoryStream ms = new MemoryStream();
			IoHash uncompressedHash = bufferUtils.CompressContent(ms, OoodleCompressorMethod.Mermaid, OoodleCompressionLevel.VeryFast, bytes);
			ms.Position = 0;

			BufferedPayload bufferedPayload = await bufferUtils.DecompressContentAsync(ms, (ulong)ms.Length, CancellationToken.None);

			byte[] roundTrippedBytes = await bufferedPayload.GetStream().ReadAllBytesAsync();
			CollectionAssert.AreEqual(bytes, roundTrippedBytes);
		}
	}
}
