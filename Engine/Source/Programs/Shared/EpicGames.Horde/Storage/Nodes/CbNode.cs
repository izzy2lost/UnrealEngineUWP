// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Serialization;
using System.Collections.Generic;

namespace EpicGames.Horde.Storage.Nodes
{
	/// <summary>
	/// A node containing arbitrary compact binary data
	/// </summary>
	[BlobConverter(typeof(CbNodeConverter))]
	public class CbNode
	{
		/// <summary>
		/// The compact binary object
		/// </summary>
		public CbObject Object { get; set; }

		/// <summary>
		/// Imported nodes
		/// </summary>
		public IReadOnlyList<IBlobHandle<object>> References { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="obj">The compact binary object</param>
		/// <param name="references">List of references to attachments</param>
		public CbNode(CbObject obj, IReadOnlyList<IBlobHandle<object>> references)
		{
			Object = obj;
			References = references;
		}
	}

	class CbNodeConverter : BlobConverter<CbNode>
	{
		class HandleMapper
		{
			readonly IBlobReader _reader;
			readonly List<IBlobHandle<object>> _refs;
			int _refIdx;

			public HandleMapper(IBlobReader reader, List<IBlobHandle<object>> refs)
			{
				_reader = reader;
				_refs = refs;
			}

			public void IterateField(CbField field)
			{
				if (field.IsAttachment())
				{
					IBlobHandle handle = _reader.References[_refIdx++];
					_refs.Add(handle.ForType<object>(field.AsAttachment()));
				}
				else if (field.IsArray())
				{
					CbArray array = field.AsArray();
					array.IterateAttachments(IterateField);
				}
				else if (field.IsObject())
				{
					CbObject obj = field.AsObject();
					obj.IterateAttachments(IterateField);
				}
			}
		}

		public static BlobType BlobType { get; } = new BlobType("{34A0793F-42F4-8364-A798-32862932841C}", 1);

		/// <inheritdoc/>
		public override CbNode Read(IBlobReader reader, BlobSerializerOptions options)
		{
			CbObject obj = new CbObject(reader.GetMemory().ToArray());

			List<IBlobHandle<object>> references = new List<IBlobHandle<object>>();
			obj.IterateAttachments(new HandleMapper(reader, references).IterateField);

			return new CbNode(obj, references);
		}

		/// <inheritdoc/>
		public override BlobType Write(IBlobWriter writer, CbNode value, BlobSerializerOptions options)
		{
			writer.WriteFixedLengthBytes(value.Object.GetView().Span);
			return BlobType;
		}
	}
}
