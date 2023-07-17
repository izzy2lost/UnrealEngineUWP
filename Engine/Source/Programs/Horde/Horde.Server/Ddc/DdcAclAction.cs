// Copyright Epic Games, Inc. All Rights Reserved.

using Horde.Server.Acls;

namespace Horde.Server.Ddc
{
	/// <summary>
	/// Actions for manipulating objects in DDC
	/// </summary>
	public static class DdcAclAction
	{
		/// <summary>
		/// General read access to refs / blobs and so on
		/// </summary>
		public static AclAction ReadObject { get; } = new AclAction("DdcReadObject");

		/// <summary>
		/// General write access to upload refs / blobs etc
		/// </summary>
		public static AclAction WriteObject { get; } = new AclAction("DdcWriteObject");

		/// <summary>
		/// Access to delete blobs / refs etc
		/// </summary>
		public static AclAction DeleteObject { get; } = new AclAction("DdcDeleteObject");

		/// <summary>
		/// Access to perform administrative task
		/// </summary>
		public static AclAction AdminAction { get; } = new AclAction("DdcAdmin");

		/// <summary>
		/// Access to delete a particular bucket
		/// </summary>
		public static AclAction DeleteBucket { get; } = new AclAction("DdcDeleteBucket");

		/// <summary>
		/// Access to delete a whole namespace
		/// </summary>
		public static AclAction DeleteNamespace { get; } = new AclAction("DdcDeleteNamespace");
	}
}
