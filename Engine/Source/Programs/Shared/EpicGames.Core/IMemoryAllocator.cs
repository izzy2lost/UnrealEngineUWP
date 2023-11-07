// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.IO.MemoryMappedFiles;
using System.Runtime.InteropServices;

namespace EpicGames.Core
{
	/// <summary>
	/// Interface for an explicit memory allocator. Meant for use with large memory blocks, where objects would typically be allocated on the LOH and waiting for GC can cause
	/// excessive memory usage.
	/// </summary>
	public interface IMemoryAllocator<T>
	{
		/// <summary>
		/// Allocate a block of memory of the given minimum size
		/// </summary>
		/// <param name="minSize">Minimum size for the allocated data</param>
		/// <returns>Reference counted block of memory</returns>
		IMemoryOwner<T> Alloc(int minSize);
	}

	/// <summary>
	/// Implementation of <see cref="IMemoryAllocator{Byte}"/> which rents blocks from a memory pool.
	/// </summary>
	public class PoolAllocator : IMemoryAllocator<byte>
	{
		readonly MemoryPool<byte> _pool;

		/// <summary>
		/// Shared allocator instance
		/// </summary>
		public static PoolAllocator Shared { get; } = new PoolAllocator(MemoryPool<byte>.Shared);

		/// <summary>
		/// Constructor
		/// </summary>
		public PoolAllocator(MemoryPool<byte> pool) => _pool = pool;

		/// <inheritdoc/>
		public IMemoryOwner<byte> Alloc(int minSize) => _pool.Rent(minSize);
	}

	/// <summary>
	/// Implementation of <see cref="IMemoryAllocator{Byte}"/> which returns blocks from the global heap
	/// </summary>
	public class GlobalHeapAllocator : IMemoryAllocator<byte>
	{
		unsafe class Allocation : MemoryManager<byte>
		{
			IntPtr _handle;
			byte* _pointer;
			int _length;

			public Allocation(int size)
			{
				_handle = Marshal.AllocHGlobal(size);
				_pointer = (byte*)_handle.ToPointer();
				_length = size;
			}

			/// <inheritdoc/>
			public override Span<byte> GetSpan() => new Span<byte>(_pointer, _length);

			/// <inheritdoc/>
			public override MemoryHandle Pin(int elementIndex) => new MemoryHandle(_pointer + elementIndex);

			/// <inheritdoc/>
			public override void Unpin() { }

			/// <inheritdoc/>
			protected override void Dispose(bool disposing)
			{
				if (_handle != IntPtr.Zero)
				{
					Marshal.FreeHGlobal(_handle);
					_handle = IntPtr.Zero;
				}

				_pointer = null;
				_length = 0;
			}
		}

		/// <inheritdoc/>
		public IMemoryOwner<byte> Alloc(int minSize) => new Allocation(minSize);
	}
	
	/// <summary>
	/// Implementation of <see cref="IMemoryAllocator{Byte}"/> which returns memory backed by memory mapped files
	/// </summary>
	public class VirtualMemoryAllocator : IMemoryAllocator<byte>
	{
		unsafe class Allocation : MemoryManager<byte>
		{
			MemoryMappedFile? _file;
			MemoryMappedViewAccessor? _viewAccessor;
			byte* _pointer;
			readonly int _length;

			public Allocation(int length)
			{
				_file = MemoryMappedFile.CreateNew(null, length, MemoryMappedFileAccess.ReadWrite);
				_viewAccessor = _file.CreateViewAccessor();
				_viewAccessor.SafeMemoryMappedViewHandle.AcquirePointer(ref _pointer);
				_length = length;
			}

			/// <inheritdoc/>
			public override Span<byte> GetSpan() => new Span<byte>(_pointer, _length);

			/// <inheritdoc/>
			public override MemoryHandle Pin(int elementIndex) => new MemoryHandle(_pointer + elementIndex);

			/// <inheritdoc/>
			public override void Unpin() { }

			/// <inheritdoc/>
			protected override void Dispose(bool disposing)
			{
				if (disposing)
				{
					if (_pointer != null)
					{
						_viewAccessor!.SafeMemoryMappedViewHandle.ReleasePointer();
						_pointer = null;
					}
					if (_viewAccessor != null)
					{
						_viewAccessor.Dispose();
						_viewAccessor = null;
					}
					if (_file != null)
					{
						_file.Dispose();
						_file = null;
					}
				}
			}
		}

		/// <inheritdoc/>
		public IMemoryOwner<byte> Alloc(int minSize) => new Allocation(minSize);
	}
}
