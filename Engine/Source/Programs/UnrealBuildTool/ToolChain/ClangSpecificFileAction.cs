// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.IO;
using System.Linq;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	/// <summary>
	/// Single
	/// </summary>
	internal class ClangSpecificFileAction : Action, ISpecificFileAction
	{
		DirectoryReference SourceDir;
		DirectoryReference OutputDir;
		IEnumerable<string> RspLines;

		// TODO: Entire CppCompileEnvironment needs to be saved with BinaryArchiveWriter, some options may still be unused
		bool bPreprocessOnly;
		bool bWithAssembly;

		Dictionary<string, List<FileItem>> SingleFiles = new();

		internal ClangSpecificFileAction(DirectoryReference Source, DirectoryReference Output, Action Action, CppCompileEnvironment? CompileEnvironment, IEnumerable<string> ContentLines) : base(Action)
		{
			ProducedItems.Clear();
			DependencyListFile = null;

			SourceDir = Source;
			OutputDir = Output;
			RspLines = ContentLines;
			bPreprocessOnly = CompileEnvironment?.bPreprocessOnly ?? false;
			bWithAssembly = CompileEnvironment?.bWithAssembly ?? false;
		}

		public ClangSpecificFileAction(BinaryArchiveReader Reader) : base(Reader)
		{
			SourceDir = Reader.ReadCompactDirectoryReference();
			OutputDir = Reader.ReadCompactDirectoryReference();
			RspLines = Reader.ReadList(() => Reader.ReadString())!;
			bPreprocessOnly = Reader.ReadBool();
			bWithAssembly = Reader.ReadBool();
		}

		public new void Write(BinaryArchiveWriter Writer)
		{
			base.Write(Writer);
			Writer.WriteCompactDirectoryReference(SourceDir);
			Writer.WriteCompactDirectoryReference(OutputDir);
			Writer.WriteList(RspLines.ToList(), (Str) => Writer.WriteString(Str));
			Writer.WriteBool(bPreprocessOnly);
			Writer.WriteBool(bWithAssembly);
		}

		public DirectoryReference RootDirectory => SourceDir;

		public IExternalAction? CreateAction(FileItem SourceFile, ILogger Logger)
		{
			// Keep track of all specific files, so the output file can be renamed if there's a naming conflict
			string Filename = SourceFile.Name;
			if (!SingleFiles.ContainsKey(SourceFile.Name))
			{
				SingleFiles[SourceFile.Name] = new();
			}
			else
			{
				Filename = $"{Path.GetFileNameWithoutExtension(SourceFile.Name)}{SingleFiles[Filename].Count}{Path.GetExtension(SourceFile.Name)}";
			}
			SingleFiles[SourceFile.Name].Add(SourceFile);

			string DummyName = "SingleFile.cpp";

			int FileNameIndex = CommandArguments.IndexOf(DummyName);
			string DummyPath = CommandArguments.Substring(2, FileNameIndex + DummyName.Length - 2);

			if (SourceFile.HasExtension(".h"))
			{
				FileItem DummyFile = FileItem.GetItemByFileReference(FileReference.Combine(OutputDir, "SingleFile", $"{Filename}.cpp"));
				Directory.CreateDirectory(DummyFile.Directory.FullName);
				File.WriteAllText(DummyFile.FullName, $"#include \"{SourceFile.FullName.Replace('\\', '/')}\"");
				SourceFile = DummyFile;
			}
			else if (!SourceFile.HasExtension(".cpp"))
			{
				return null;
			}

			List<string> NewRspLines = new();
			foreach (string L in RspLines)
			{
				string Line = L;
				if (Line.Contains(".cpp.bc", System.StringComparison.Ordinal) ||
					Line.Contains(".cpp.d", System.StringComparison.Ordinal) ||
					Line.Contains(".cpp.i", System.StringComparison.Ordinal) || 
					Line.Contains(".cpp.json", System.StringComparison.Ordinal) ||
					Line.Contains(".cpp.o", System.StringComparison.Ordinal))
				{
					Line = Line.Replace("SingleFile.cpp", Filename);
				}
				else
				{
					Line = Line.Replace(DummyPath, SourceFile.FullName.Replace('\\', '/'));
				}
				NewRspLines.Add(Line);
			}

			Action Action = new Action(this);
			Action.CommandArguments = CommandArguments.Replace(DummyName, Filename);
			Action.DependencyListFile = null;
			Action.StatusDescription = Filename;

			if (bPreprocessOnly)
			{
				// We have to add a produced item so this action is not skipped.
				// Note we on purpose use a different extension than what the compiler produce because otherwise up-to-date checker might see it as up-to-date
				// even though we want it to always be built
				FileItem ProducedItem = FileItem.GetItemByFileReference(FileReference.Combine(OutputDir, $"{Filename}.n"));
				Action.ProducedItems.Add(ProducedItem);
			}
			else
			{
				// We have to add a produced item so this action is not skipped.
				// Note we on purpose use a different extension than what the compiler produce because otherwise up-to-date checker might see it as up-to-date
				// even though we want it to always be built
				FileItem ProducedItem = FileItem.GetItemByFileReference(FileReference.Combine(OutputDir, $"{Filename}.o"));
				Action.ProducedItems.Add(ProducedItem);

				if (bWithAssembly)
				{
					FileItem AssemblyFile = FileItem.GetItemByFileReference(FileReference.Combine(OutputDir, $"{Filename}.asm"));
					Action.ProducedItems.Add(AssemblyFile);
				}
			}

			FileItem ResponseFile = FileItem.GetItemByPath(Action.CommandArguments.Substring(1).Trim('"'));
			File.WriteAllLines(ResponseFile.FullName, NewRspLines);

			return Action;
		}
	}

	class ClangSpecificFileActionSerializer : ActionSerializerBase<ClangSpecificFileAction>
	{
		/// <inheritdoc/>
		public override ClangSpecificFileAction Read(BinaryArchiveReader Reader)
		{
			return new ClangSpecificFileAction(Reader);
		}

		/// <inheritdoc/>
		public override void Write(BinaryArchiveWriter Writer, ClangSpecificFileAction Action)
		{
			Action.Write(Writer);
		}
	}

	class ClangSpecificFileActionGraphBuilder : ForwardingActionGraphBuilder
	{
		public ClangSpecificFileActionGraphBuilder(ILogger Logger) : base(new NullActionGraphBuilder(Logger))
		{
		}
		public override void CreateIntermediateTextFile(FileItem Location, IEnumerable<string> ContentLines, bool AllowAsync = true)
		{
			this.ContentLines = ContentLines;
		}

		public IEnumerable<string> ContentLines = Enumerable.Empty<string>();
	}
}
