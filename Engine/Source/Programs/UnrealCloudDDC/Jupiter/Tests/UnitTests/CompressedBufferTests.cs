// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Core;
using Jupiter.Common.Implementation;
using Jupiter.Implementation;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using OpenTelemetry.Trace;

namespace Jupiter.Tests.Unit
{
    [TestClass]
    public class CompressedBufferTests
    {

        [TestMethod]
        public async Task CompressAndDecompress()
        {
            byte[] bytes = Encoding.UTF8.GetBytes("this is a test string");

            CompressedBufferUtils bufferUtils = new(TracerProvider.Default.GetTracer("TestTracer"));

            using MemoryStream ms = new MemoryStream(); 
            IoHash uncompressedHash = bufferUtils.CompressContent(ms, OoodleCompressorMethod.Mermaid, OoodleCompressionLevel.VeryFast, bytes);
            ms.Position = 0;

            IBufferedPayload bufferedPayload = await bufferUtils.DecompressContent(ms, (ulong)ms.Length);

            byte[] roundTrippedBytes = await bufferedPayload.GetStream().ReadAllBytesAsync();
            CollectionAssert.AreEqual(bytes, roundTrippedBytes);
        }
    }
}
