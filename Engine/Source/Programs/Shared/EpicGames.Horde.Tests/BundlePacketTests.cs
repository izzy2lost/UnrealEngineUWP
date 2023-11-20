// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Bundles;
using EpicGames.Horde.Storage.Bundles.V2;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace EpicGames.Horde.Tests
{
	[TestClass]
	public class BundlePacketTests
	{
		class FakeHandle : IBlobHandle
		{
			readonly IBlobHandle? _outer;
			readonly string? _fragment;

			public IBlobHandle? Outer => _outer;

			public FakeHandle(IBlobHandle? outer, string? fragment)
			{
				_outer = outer;
				_fragment = fragment;
			}

			public ValueTask<BlobData> ReadBlobDataAsync(CancellationToken cancellationToken = default) => throw new NotImplementedException();
			public ValueTask FlushAsync(CancellationToken cancellationToken = default) => throw new NotImplementedException();

			public ValueTask<BlobData> ReadAsync(CancellationToken cancellationToken = default)
			{
				throw new NotImplementedException();
			}

			public bool TryAppendIdentifier(Utf8StringBuilder builder)
			{
				if (_fragment == null)
				{
					return false;
				}

				builder.Append(_fragment);
				return true;
			}

			public override bool Equals(object? obj)
				=> obj is FakeHandle other && Object.Equals(_outer, other._outer) && String.Equals(_fragment, other._fragment, StringComparison.Ordinal);

			public override int GetHashCode()
				=> HashCode.Combine(_outer, _fragment);
		}

		[TestMethod]
		public void TestTypes()
		{
			Guid guid = Guid.Parse("{30F2CA83-B5D4-494C-8802-0661454CCD58}");

			// Write out some data
			BlobType blobType1 = new BlobType(guid, 1);
			BlobType blobType2 = new BlobType(guid, 2);

			FakeHandle bundleHandle = new FakeHandle(null, null);
			FakeHandle packetHandle = new FakeHandle(bundleHandle, null);

			using PacketWriter writer = new PacketWriter(bundleHandle, packetHandle, ManagedHeapAllocator.Shared, new object());
			int type1 = writer.FindOrAddType(blobType1);
			int type2 = writer.FindOrAddType(blobType2);
			int type1b = writer.FindOrAddType(blobType1); // Check it's deduped
			Assert.AreEqual(type1, type1b);

			FakeHandle fooBundleHandle = new FakeHandle(null, "foo");
			FakeHandle barBundleHandle = new FakeHandle(null, "bar");

			int import1 = writer.FindOrAddImport(new FakeHandle(fooBundleHandle, "123"));
			int import2 = writer.FindOrAddImport(new FakeHandle(fooBundleHandle, "456"));
			int import3 = writer.FindOrAddImport(new FakeHandle(fooBundleHandle, "789"));
			int import4 = writer.FindOrAddImport(new FakeHandle(barBundleHandle, "123"));
			int import4b = writer.FindOrAddImport(new FakeHandle(barBundleHandle, "123"));
			Assert.AreEqual(import4, import4b);

			Encoding.UTF8.GetBytes("hello", writer.GetOutputBuffer(0, 5).Span);
			int export1 = writer.CompleteExport(5, type1, new[] { import1, import2 });
			Assert.AreEqual(export1, 0);

			Encoding.UTF8.GetBytes(" ", writer.GetOutputBuffer(0, 1).Span);
			int export2 = writer.CompleteExport(1, type2, Array.Empty<int>());
			Assert.AreEqual(export2, 1);

			Encoding.UTF8.GetBytes("world!", writer.GetOutputBuffer(0, 6).Span);
			int export3 = writer.CompleteExport(6, type2, new[] { import2, import3, import4 });
			Assert.AreEqual(export3, 2);

			Packet packet = writer.CompletePacket();

			ArrayMemoryWriter memoryWriter = new ArrayMemoryWriter(4096);
			packet.Encode(BundleCompressionFormat.None, memoryWriter);

			// Check we can read it back in
			IRefCountedHandle<Packet> handle = Packet.Decode(memoryWriter.WrittenMemory, ManagedHeapAllocator.Shared);
			packet = handle.Target;

			Assert.AreEqual(2, packet.GetTypeCount());
			Assert.AreEqual(blobType1, packet.GetType(0));
			Assert.AreEqual(blobType2, packet.GetType(1));

			Assert.AreEqual(6, packet.GetImportCount());
			Assert.AreEqual(new PacketImport(-1, new Utf8String("foo")), packet.GetImport(0));
			Assert.AreEqual(new PacketImport(0, new Utf8String("123")), packet.GetImport(1));
			Assert.AreEqual(new PacketImport(0, new Utf8String("456")), packet.GetImport(2));
			Assert.AreEqual(new PacketImport(0, new Utf8String("789")), packet.GetImport(3));
			Assert.AreEqual(new PacketImport(-1, new Utf8String("bar")), packet.GetImport(4));
			Assert.AreEqual(new PacketImport(4, new Utf8String("123")), packet.GetImport(5));

			Assert.AreEqual(3, packet.GetExportCount());

			PacketExportHeader header1 = packet.GetExport(0).GetHeader();
			Assert.AreEqual(0, header1.TypeIdx);
			Assert.IsTrue(header1.Imports.SequenceEqual(new int[] { 1, 2 }));
			Assert.AreEqual(new Utf8String("hello"), new Utf8String(packet.GetExport(0).GetPayload()));

			PacketExportHeader header2 = packet.GetExport(1).GetHeader();
			Assert.AreEqual(1, header2.TypeIdx);
			Assert.AreEqual(0, header2.Imports.Length);
			Assert.AreEqual(new Utf8String(" "), new Utf8String(packet.GetExport(1).GetPayload()));

			PacketExportHeader header3 = packet.GetExport(2).GetHeader();
			Assert.AreEqual(1, header3.TypeIdx);
			Assert.IsTrue(header3.Imports.SequenceEqual(new int[] { 2, 3, 5 }));
			Assert.AreEqual(new Utf8String("world!"), new Utf8String(packet.GetExport(2).GetPayload()));
		}
	}
}
