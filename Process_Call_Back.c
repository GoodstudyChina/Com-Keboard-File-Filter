#pragma once

#include<ntddk.h>
#include<ntdef.h>
#include<wdm.h>


typedef struct _LDR_DATA_TABLE_ENTRY
{
	LIST_ENTRY InLoadOrderLinks;
	LIST_ENTRY InMemoryOrderLinks;
	LIST_ENTRY InInitializationOrderLinks;
	PVOID DllBase;
	PVOID EntryPoint;
	ULONG SizeOfImage;
	UNICODE_STRING FullDllName;
	UNICODE_STRING BaseDllName;
	ULONG Flags;
	USHORT LoadCount;
	USHORT TlsIndex;
	union {
		LIST_ENTRY HashLinks;
		struct
		{
			PVOID SectionPointer;
			ULONG CheckSum;
		}d;
	}s;
	union {
		struct
		{
			ULONG TimeDateStamp;
		}q;
		struct
		{
			PVOID LoadedImports;
		}w;
	}e;
	PVOID EntryPointActivationContext;
	PVOID PatchInformation;
}LDR_DATA_TABLE_ENTRY, *PLDR_DATA_TABLE_ENTRY;

PVOID obHandle;	//定义一个void*类型的变量，它将会作为ObRegisterCallbacks函数的第2个参数。

VOID DriverUnload(IN PDRIVER_OBJECT pDriverObj)
{
	UNREFERENCED_PARAMETER(pDriverObj);

	if (NULL != obHandle)
	{
		ObUnRegisterCallbacks(obHandle); //卸载回掉函数
	}

	KdPrint(("Unloading .....\n"));
}

OB_PREOP_CALLBACK_STATUS
preCall(PVOID RegistrationContext, POB_PRE_OPERATION_INFORMATION pOperationInformation)
{
	UNREFERENCED_PARAMETER(RegistrationContext);
	UNREFERENCED_PARAMETER(pOperationInformation);

	KdPrint(("a new process by create \n "));////在这里 对新创建的进程 进行操作。

	return OB_PREOP_SUCCESS;
}

NTSTATUS ProtectProcess()
{
	NTSTATUS re = STATUS_SUCCESS;
	OB_CALLBACK_REGISTRATION obReg;
	OB_OPERATION_REGISTRATION opReg;


	memset(&obReg, 0, sizeof(obReg));
	obReg.Version = ObGetFilterVersion();
	obReg.OperationRegistrationCount = 1;
	obReg.RegistrationContext = NULL;
	RtlInitUnicodeString(&obReg.Altitude, L"321000");

	memset(&opReg, 0, sizeof(opReg)); //初始化结构体变量

									  //下面 请注意这个结构体的成员字段的设置
	opReg.ObjectType = PsProcessType;
	opReg.Operations = OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE;

	opReg.PreOperation = (POB_PRE_OPERATION_CALLBACK)&preCall; //在这里注册一个回调函数指针

	obReg.OperationRegistration = &opReg; //注意这一条语句

	re = ObRegisterCallbacks(&obReg, &obHandle); //在这里注册回调函数
	if (!NT_SUCCESS(re))
	{
		KdPrint(("ObRegistreCallbacks failed \n"));
	}
	return re;
}

NTSTATUS
DriverEntry(IN PDRIVER_OBJECT pDriverObj, IN PUNICODE_STRING pRegistryString)
{
	NTSTATUS re = STATUS_SUCCESS;
	
	UNREFERENCED_PARAMETER(pDriverObj);
	UNREFERENCED_PARAMETER(pRegistryString);
	KdPrint(("My Driver install success \r\n"));

	pDriverObj->DriverUnload = DriverUnload;

	//Protect by process  name

	//绕过MmVerifyCallBackFunction
	PLDR_DATA_TABLE_ENTRY ldr;
	ldr = (PLDR_DATA_TABLE_ENTRY)pDriverObj->DriverSection;
	ldr->Flags |= 0x20;
	re = ProtectProcess();
	return re;
}