package com.epicgames.makeaar;

public enum UnrealMessageType {
	Undefined,  // use up 0
	Hello,
	AttachExternalSurface,
	DetachExternalSurface,
	StopService,
	ResumeService,
	TouchEvent,
	SendConsoleCommand,
	SendData,
	Unknown;

	public static UnrealMessageType fromInteger(int x)
	{
		switch (x)
		{
			default:
			case 0:
				return Undefined;
			case 1:
				return Hello;
			case 2:
				return AttachExternalSurface;
			case 3:
				return DetachExternalSurface;
			case 4:
				return StopService;
			case 5:
				return ResumeService;
			case 6:
				return TouchEvent;
			case 7:
				return SendConsoleCommand;
			case 8:
				return SendData;
		}
	}
}