#pragma once
#include<ntddk.h>
#include<ntddkbd.h>

NTSTATUS keyAttachDevice(
	IN PDRIVER_OBJECT DriverObject,
	IN PUNICODE_STRING RegisttyPath
);

NTSTATUS keyDisaptchGeneral(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp
);

NTSTATUS keyDisaptchRead(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp
);

NTSTATUS keyPower(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp
);

NTSTATUS keyPnp(
	IN PDEVICE_OBJECT DevieObject,
	IN PIRP Irp
);
VOID  keyUnload(
	IN PDRIVER_OBJECT DriverObject
);