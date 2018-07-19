#pragma once 
#include<ntddk.h>
#include"keyboardfilter.h"
NTSTATUS DriverEntry(
	IN PDRIVER_OBJECT DriverObject,
	IN PUNICODE_STRING RegistryPath
)
{
	UNREFERENCED_PARAMETER(RegistryPath);
	UNREFERENCED_PARAMETER(DriverObject);

	NTSTATUS status;
	ULONG i;
	KdPrint(("Keyborad Filter is : Entrying DriverEntry\n"));
	for (i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
	{
		DriverObject->MajorFunction[i] = keyDisaptchGeneral;
	}
	//单独地填写一个Read分发函数。因为重要的过滤就是读取来的按键信息
	//其他的都不重要。这个分发函数单独写
	DriverObject->MajorFunction[IRP_MJ_READ] = keyDisaptchRead;
	//单独地填写一个IRP_MJ_POWER函数。这是因为这类请求中间要调用
	//一个PoCallDriver和一个PoStartNextPowerIrp,比较特殊
	DriverObject->MajorFunction[IRP_MJ_POWER] = keyPower;
	//我们想知道什么时候我们绑定过的一个设备被卸载了（比如从机器上被拔掉了），所以专门写一个PNP（即插即用）分发函数
	DriverObject->MajorFunction[IRP_MJ_PNP] = keyPnp;
	//卸载函数
	DriverObject->DriverUnload = keyUnload;
	status = keyAttachDevice(DriverObject, RegistryPath);
	return status;
}