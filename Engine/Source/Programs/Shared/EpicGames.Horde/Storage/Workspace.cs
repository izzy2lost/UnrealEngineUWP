// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage.Nodes;
using Microsoft.Extensions.Logging;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Provides functionality to extract and patch data in a local workspace
	/// </summary>
	public class Workspace
	{
		[DebuggerDisplay("{Name}")]
		class DirectoryState
		{
			public DirectoryState? Parent { get; }
			public Utf8String Name { get; }
			public List<DirectoryState> Directories { get; }
			public List<FileState> Files { get; }

			public ulong LayerFlags { get; set; }

			public DirectoryState(DirectoryState? parent, Utf8String name)
			{
				Parent = parent;
				Name = name;

				Directories = new List<DirectoryState>();
				Files = new List<FileState>();
			}

			public DirectoryState(DirectoryState? parent, Utf8String name, IMemoryReader reader)
			{
				Parent = parent;
				Name = name;

				int numDirectories = reader.ReadInt32();
				Directories = new List<DirectoryState>(numDirectories);

				for (int idx = 0; idx < numDirectories; idx++)
				{
					Utf8String subDirName = reader.ReadUtf8String();
					DirectoryState subDirState = new DirectoryState(this, subDirName, reader);
					Directories.Add(subDirState);
				}

				int numFiles = reader.ReadInt32();
				Files = new List<FileState>(numFiles);

				for (int idx = 0; idx < numFiles; idx++)
				{
					Utf8String fileName = reader.ReadUtf8String();
					FileState fileState = new FileState(this, fileName, reader);
					Files.Add(fileState);
				}

				LayerFlags = reader.ReadUnsignedVarInt();
			}

			public bool TryGetFile(Utf8String name, [NotNullWhen(true)] out FileState? fileState)
			{
				int index = Files.BinarySearch(x => x.Name, name);
				if (index >= 0)
				{
					fileState = Files[index];
					return true;
				}
				else
				{
					fileState = null;
					return false;
				}
			}

			public FileState FindOrAddFile(Utf8String name)
			{
				int index = Files.BinarySearch(x => x.Name, name);
				if (index >= 0)
				{
					return Files[index];
				}
				else
				{
					FileState fileState = new FileState(this, name);
					Files.Insert(~index, fileState);
					return fileState;
				}
			}

			public bool TryGetDirectory(Utf8String name, [NotNullWhen(true)] out DirectoryState? directoryState)
			{
				int index = Directories.BinarySearch(x => x.Name, name);
				if (index >= 0)
				{
					directoryState = Directories[index];
					return true;
				}
				else
				{
					directoryState = null;
					return false;
				}
			}

			public DirectoryState FindOrAddDirectory(Utf8String name)
			{
				int index = Directories.BinarySearch(x => x.Name, name);
				if (index >= 0)
				{
					return Directories[index];
				}
				else
				{
					DirectoryState subDirState = new DirectoryState(this, name);
					Directories.Insert(~index, subDirState);
					return subDirState;
				}
			}

			public void Write(IMemoryWriter writer)
			{
				writer.WriteInt32(Directories.Count);
				foreach (DirectoryState directory in Directories)
				{
					writer.WriteUtf8String(directory.Name);
					directory.Write(writer);
				}

				writer.WriteInt32(Files.Count);
				foreach (FileState file in Files)
				{
					writer.WriteUtf8String(file.Name);
					file.Write(writer);
				}

				writer.WriteUnsignedVarInt(LayerFlags);
			}
		}

		[DebuggerDisplay("{Name}")]
		class FileState
		{
			public DirectoryState Parent { get; }
			public Utf8String Name { get; }
			public long Length { get; private set; }
			public long LastModifiedTimeUtc { get; private set; }
			public IoHash Hash { get; set; }

			public ulong LayerFlags { get; set; }
			public List<ChunkInfo> Chunks { get; } = new List<ChunkInfo>();

			public FileState(DirectoryState parent, Utf8String name)
			{
				Parent = parent;
				Name = name;
			}

			public FileState(DirectoryState parent, Utf8String name, IMemoryReader reader)
			{
				Parent = parent;
				Name = name;

				Length = reader.ReadInt64();
				LastModifiedTimeUtc = reader.ReadInt64();
				Hash = reader.ReadIoHash();

				LayerFlags = reader.ReadUnsignedVarInt();
				Chunks = reader.ReadList(() => new ChunkInfo(reader));
			}

			public bool Modified(FileInfo fileInfo) => Length != fileInfo.Length || LastModifiedTimeUtc != fileInfo.LastWriteTimeUtc.Ticks;

			public void Update(FileInfo fileInfo)
			{
				Length = fileInfo.Length;
				LastModifiedTimeUtc = fileInfo.LastWriteTimeUtc.Ticks;
			}

			public void Write(IMemoryWriter writer)
			{
				Debug.Assert(Hash != IoHash.Zero);
				writer.WriteInt64(Length);
				writer.WriteInt64(LastModifiedTimeUtc);
				writer.WriteIoHash(Hash);
				writer.WriteUnsignedVarInt(LayerFlags);
				writer.WriteList(Chunks, x => x.Write(writer));
			}
		}

		class ChunkInfo
		{
			public IoHash Hash { get; }
			public long Offset { get; }
			public long Length { get; }

			public ChunkInfo(IoHash hash, long offset, long length)
			{
				Hash = hash;
				Offset = offset;
				Length = length;
			}

			public ChunkInfo(IMemoryReader reader)
			{
				Hash = reader.ReadIoHash();
				Offset = (long)reader.ReadUnsignedVarInt();
				Length = (long)reader.ReadUnsignedVarInt();
			}

			public void Write(IMemoryWriter writer)
			{
				writer.WriteIoHash(Hash);
				writer.WriteUnsignedVarInt((ulong)Offset);
				writer.WriteUnsignedVarInt((ulong)Length);
			}
		}

		class LayerState
		{
			public WorkspaceLayerId Id { get; }
			public ulong Flag { get; }

			public LayerState(WorkspaceLayerId id, ulong flag)
			{
				Id = id;
				Flag = flag;
			}

			public LayerState(IMemoryReader reader)
			{
				Id = new WorkspaceLayerId(new StringId(reader.ReadUtf8String()));
				Flag = reader.ReadUnsignedVarInt();
			}

			public void Write(IMemoryWriter writer)
			{
				writer.WriteUtf8String(Id.Id.Text);
				writer.WriteUnsignedVarInt(Flag);
			}
		}

		class WorkspaceState
		{
			public DirectoryState Root { get; set; }
			public List<LayerState> Layers { get; private set; }

			public WorkspaceState()
			{
				Root = new DirectoryState(null, Utf8String.Empty);
				Layers = new List<LayerState> { new LayerState(WorkspaceLayerId.Default, 1) };
			}

			public WorkspaceState(IMemoryReader reader)
			{
				Root = new DirectoryState(null, Utf8String.Empty, reader);
				Layers = reader.ReadList(() => new LayerState(reader));
			}

			public void Write(IMemoryWriter writer)
			{
				Root.Write(writer);
				writer.WriteList(Layers, x => x.Write(writer));
			}
		}

		readonly DirectoryReference _rootDir;
		readonly FileReference _stateFile;
		readonly WorkspaceState _state;
		readonly ILogger _logger;

		const string HordeDirName = ".horde";
		const string StateFileName = "contents.dat";

		/// <summary>
		/// Root directory for the workspace
		/// </summary>
		public DirectoryReference RootDir => _rootDir;

		/// <summary>
		/// Layers current in this workspace
		/// </summary>
		public IReadOnlyList<WorkspaceLayerId> Layers => _state.Layers.Select(x => x.Id).ToList();

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="rootDir">Directory for the workspace</param>
		/// <param name="stateFile">Path to the state file for this directory</param>
		/// <param name="state">Current state</param>
		/// <param name="logger">Logger for diagnostic output</param>
		private Workspace(DirectoryReference rootDir, FileReference stateFile, WorkspaceState state, ILogger logger)
		{
			_rootDir = rootDir;
			_stateFile = stateFile;
			_state = state;
			_logger = logger;
		}

		/// <summary>
		/// Create a new workspace instance in the given location. Opens the existing instance if it already contains workspace data.
		/// </summary>
		/// <param name="rootDir">Root directory for the workspace</param>
		/// <param name="logger">Logger for output</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Workspace instance</returns>
		public static async Task<Workspace> CreateAsync(DirectoryReference rootDir, ILogger logger, CancellationToken cancellationToken = default)
		{
			FileReference stateFile = FileReference.Combine(rootDir, HordeDirName, StateFileName);

			using FileStream? stream = FileTransaction.OpenRead(stateFile);
			if (stream != null)
			{
				throw new InvalidOperationException($"Workspace already exists in {rootDir}; use Open instead.");
			}

			Workspace workspace = new Workspace(rootDir, stateFile, new WorkspaceState(), logger);
			await workspace.SaveAsync(cancellationToken);

			return workspace;
		}

		/// <summary>
		/// Attempts to open an existing workspace for the current directory. 
		/// </summary>
		/// <param name="currentDir">Root directory for the workspace</param>
		/// <param name="logger">Logger for output</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Workspace instance</returns>
		public static async Task<Workspace?> TryOpenAsync(DirectoryReference currentDir, ILogger logger, CancellationToken cancellationToken = default)
		{
			for (DirectoryReference? testDir = currentDir; testDir != null; testDir = testDir.ParentDirectory)
			{
				FileReference stateFile = FileReference.Combine(testDir, HordeDirName, StateFileName);
				using (FileStream? stream = FileTransaction.OpenRead(stateFile))
				{
					if (stream != null)
					{
						byte[] data = await stream.ReadAllBytesAsync(cancellationToken);
						WorkspaceState state = new WorkspaceState(new MemoryReader(data));
						return new Workspace(testDir, stateFile, state, logger);
					}
				}
			}
			return null;
		}

		/// <summary>
		/// Save the current state of the workspace
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task SaveAsync(CancellationToken cancellationToken)
		{
			DirectoryReference.CreateDirectory(_stateFile.Directory);

			using (FileTransactionStream stream = FileTransaction.OpenWrite(_stateFile))
			{
				using (ChunkedMemoryWriter writer = new ChunkedMemoryWriter(64 * 1024))
				{
					_state.Write(writer);
					await writer.CopyToAsync(stream, cancellationToken);
				}
				stream.CompleteTransaction();
			}
		}

		#region Layers

		/// <summary>
		/// Add or update a layer with the given identifier
		/// </summary>
		/// <param name="id">Identifier for the layer</param>
		public void AddLayer(WorkspaceLayerId id)
		{
			if (_state.Layers.Any(x => x.Id == id))
			{
				throw new InvalidOperationException($"Layer {id} already exists");
			}

			ulong flags = 0;
			for (int idx = 0; idx < _state.Layers.Count; idx++)
			{
				flags |= _state.Layers[idx].Flag;
			}
			if (flags == ~0UL)
			{
				throw new InvalidOperationException("Maximum number of layers reached");
			}

			ulong nextFlag = (flags + 1) ^ flags;
			_state.Layers.Add(new LayerState(id, nextFlag));
		}

		/// <summary>
		/// Removes a layer with the given identifier. Does not remove any files in the workspace.
		/// </summary>
		/// <param name="layerId">Layer to update</param>
		public void RemoveLayer(WorkspaceLayerId layerId)
		{
			int layerIdx = _state.Layers.FindIndex(x => x.Id == layerId);
			if (layerIdx > 0) // Note: Excluding default layer at index 0
			{
				LayerState layer = _state.Layers[layerIdx];
				if ((_state.Root.LayerFlags & layer.Flag) != 0)
				{
					throw new InvalidOperationException($"Workspace still contains files for layer {layerId}");
				}
				_state.Layers.RemoveAt(layerIdx);
			}
		}

		LayerState? GetLayerState(WorkspaceLayerId layerId) => _state.Layers.FirstOrDefault(x => x.Id == layerId);

		#endregion

		/// <summary>
		/// Syncs a layer to the given contents
		/// </summary>
		/// <param name="layerId">Identifier for the layer</param>
		/// <param name="contents">New contents for the layer</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task SyncAsync(WorkspaceLayerId layerId, DirectoryNode? contents, CancellationToken cancellationToken = default)
		{
			LayerState? layerState = GetLayerState(layerId);
			if (layerState == null)
			{
				throw new InvalidOperationException($"Layer '{layerId}' does not exist");
			}

			await SyncDirectoryAsync(_rootDir, _state.Root, contents, layerState.Flag, _logger, cancellationToken);
		}

		static async Task SyncDirectoryAsync(DirectoryReference dirPath, DirectoryState dirState, DirectoryNode? dirNode, ulong flag, ILogger logger, CancellationToken cancellationToken)
		{
			// Remove any directories that no longer exist
			for (int subDirIdx = 0; subDirIdx < dirState.Directories.Count; subDirIdx++)
			{
				DirectoryState subDirState = dirState.Directories[subDirIdx];
				if ((subDirState.LayerFlags & flag) != 0)
				{
					if (dirNode == null || !dirNode.TryGetDirectoryEntry(subDirState.Name, out _))
					{
						DirectoryReference subDirPath = DirectoryReference.Combine(dirPath, subDirState.Name.ToString());
						await SyncDirectoryAsync(subDirPath, subDirState, null, flag, logger, cancellationToken);
					}
				}
			}

			// Remove any files that no longer exist
			for (int fileIdx = 0; fileIdx < dirState.Files.Count; fileIdx++)
			{
				FileState fileState = dirState.Files[fileIdx];
				if ((fileState.LayerFlags & flag) != 0)
				{
					if (dirNode == null || !dirNode.TryGetFileEntry(fileState.Name, out _))
					{
						FileReference filePath = FileReference.Combine(dirPath, fileState.Name.ToString());
						await SyncFileAsync(filePath, fileState, null, flag, logger, cancellationToken);
					}
				}
			}

			// Actually delete all the unreferenced directories
			dirState.Directories.RemoveAll(x => x.LayerFlags == 0);
			dirState.Files.RemoveAll(x => x.LayerFlags == 0);

			// Clear out the layer flag for this directory. It'll be added back if we add/reuse files below.
			dirState.LayerFlags &= ~flag;

			// Add files for this directory
			if (dirNode != null)
			{
				DirectoryReference.CreateDirectory(dirPath);

				// Update directories
				foreach (DirectoryEntry subDirEntry in dirNode.Directories)
				{
					DirectoryReference subDirPath = DirectoryReference.Combine(dirPath, subDirEntry.Name.ToString());
					DirectoryState subDirState = dirState.FindOrAddDirectory(subDirEntry.Name);

					DirectoryNode subDirNode = await subDirEntry.ExpandAsync(cancellationToken);
					await SyncDirectoryAsync(subDirPath, subDirState, subDirNode, flag, logger, cancellationToken);

					dirState.LayerFlags |= flag;
				}

				// Update files
				foreach (FileEntry fileEntry in dirNode.Files)
				{
					FileReference filePath = FileReference.Combine(dirPath, fileEntry.Name.ToString());
					FileState fileState = dirState.FindOrAddFile(fileEntry.Name);

					await SyncFileAsync(filePath, fileState, fileEntry, flag, logger, cancellationToken);

					dirState.LayerFlags |= flag;
				}
			}

			// Delete the directory if it's no longer needed
			if (dirState.LayerFlags == 0 && dirState.Parent != null)
			{
				FileUtils.ForceDeleteDirectory(dirPath);
			}
		}

		static async Task SyncFileAsync(FileReference filePath, FileState fileState, FileEntry? fileEntry, ulong flag, ILogger logger, CancellationToken cancellationToken)
		{
			if (fileEntry == null)
			{
				fileState.LayerFlags &= ~flag;
				if (fileState.LayerFlags == 0)
				{
					FileUtils.ForceDeleteFile(filePath);
				}
			}
			else if (fileState.Hash != fileEntry.Hash)
			{
				FileInfo fileInfo = filePath.ToFileInfo();

				logger.LogInformation("Updating {File} to {Hash}", fileInfo, fileEntry.Hash);
				ChunkedDataNode fileNode = await fileEntry.ExpandAsync(cancellationToken);
				await fileNode.CopyToFileAsync(fileInfo, cancellationToken);
				fileInfo.Refresh();

				fileState.Hash = fileEntry.Hash;
				fileState.Update(fileInfo);
			}
		}
	}
}
