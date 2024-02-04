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
	SendData
}