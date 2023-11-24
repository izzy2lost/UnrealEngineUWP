// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Serialization;
using System.Collections.Generic;

namespace EpicGames.Horde.Storage.Nodes
{
	/// <summary>
	/// A node containing arbitrary compact binary data
	/// </summary>
	[BlobType("{34A0793F-42F4-8364-A798-32862932841C}", 1)]
	public class CbNode : Node
	{
		class HandleMapper
		{
			readonly IBlobReader _reader;
			readonly List<HashedNodeRef> _refs;

			public HandleMapper(IBlobReader reader, List<HashedNodeRef> refs)
			{
				_reader = reader;
				_refs = refs;
			}

			public void IterateField(CbField field)
			{
				if (field.IsAttachment())
				{
					IBlobHandle handle = _reader.ReadBlobReference();
					_refs.Add(new HashedNodeRef(field.AsAttachment(), handle));
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

		/// <summary>
		/// The compact binary object
		/// </summary>
		public CbObject Object { get; set; }

		/// <summary>
		/// Imported nodes
		/// </summary>
		public IReadOnlyList<HashedNodeRef> References { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="obj">The compact binary object</param>
		/// <param name="references">List of references to attachments</param>
		public CbNode(CbObject obj, IReadOnlyList<HashedNodeRef> references)
		{
			Object = obj;
			References = references;
		}

		/// <summary>
		/// Deserialization constructor
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		public CbNode(IBlobReader reader)
		{
			Object = new CbObject(reader.GetMemory().ToArray());

			List<HashedNodeRef> references = new List<HashedNodeRef>();
			Object.IterateAttachments(new HandleMapper(reader, references).IterateField);

			References = references;
		}

		/// <inheritdoc/>
		public override void Serialize(IBlobWriter writer)
		{
			writer.WriteFixedLengthBytes(Object.GetView().Span);
		}
	}
}
