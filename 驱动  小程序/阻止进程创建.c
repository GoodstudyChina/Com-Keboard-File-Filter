#include<ntddk.h>

WCHAR* GetProcessImageFullPathByID(IN PHANDLE pID)
{
	typedef NTSTATUS NTAPI NTQUERYINFORMATIONPROCESS(
		_In_		HANDLE ProcessHandle,
		_In_		PROCESSINFOCLASS ProcessInformationClass,
		_Out_		PVOID ProcessInformation,
		_In_		ULONG ProcessInformationLength,
		_Out_opt_	PULONG ReturnLength
	);
	typedef NTQUERYINFORMATIONPROCESS FAR * LPNTQUERYINFORMATIONPROCESS;
	UNICODE_STRING ProcessImageName;
	NTSTATUS status = STATUS_SUCCESS;
	ULONG  returnedLength;
	LPNTQUERYINFORMATIONPROCESS ZwQueryInformationProcess = NULL;

	HANDLE processHandle;
	CLIENT_ID parentID;
	OBJECT_ATTRIBUTES ObjectAttributes;

	PAGED_CODE();
	if (NULL == ZwQueryInformationProcess)
	{
		UNICODE_STRING routinName;
		RtlInitUnicodeString(&routinName, L"ZwQueryInformationProcess");
		ZwQueryInformationProcess = (LPNTQUERYINFORMATIONPROCESS)MmGetSystemRoutineAddress(&routinName);
		if (NULL == ZwQueryInformationProcess)
			KdPrint(("Cannot resolve ZwQueryInformationProcess\n"));
	}

	InitializeObjectAttributes(&ObjectAttributes, 0, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, 0, 0);
	parentID.UniqueProcess = pID;
	parentID.UniqueThread = 0;

	status = ZwOpenProcess(&processHandle, PROCESS_ALL_ACCESS, &ObjectAttributes, &parentID);

	status = ZwQueryInformationProcess(processHandle,
		ProcessImageFileName,
		NULL,//buffer
		0,  //buffer size
		&returnedLength
	);

	//
	// Is the passed-in buffer going to be big enough for us? 
	// This function returns a single contguous buffer model...
	//
	ProcessImageName.Length = (USHORT)returnedLength;
	ProcessImageName.MaximumLength = (USHORT)returnedLength;
	ProcessImageName.Buffer = ExAllocatePoolWithTag(NonPagedPool,
		ProcessImageName.Length,
		'gxb');
	//
	// If we get here, the buffer IS going to be big enough for us, so
	// let's allocate some storage.
	//
	status = ZwQueryInformationProcess(processHandle,
		ProcessImageFileName,
		ProcessImageName.Buffer,
		ProcessImageName.Length,
		&returnedLength);

	if (!NT_SUCCESS(status))
		KdPrint(("ZwQueryInformationProcess error code %x ", status));

	return ProcessImageName.Buffer;
}

VOID ProcessPre (
	__inout PEPROCESS  Process,
	__in HANDLE  ProcessId,
	__in_opt PPS_CREATE_NOTIFY_INFO  CreateInfo
	)
{
	UNREFERENCED_PARAMETER(Process);
	UNREFERENCED_PARAMETER(ProcessId);
	UNREFERENCED_PARAMETER(CreateInfo);
	if (NULL == CreateInfo)
	{
		KdPrint((" Process Exit\r\n"));
		return;
	}

	KdPrint(
		("Create process ParentName:%S ProcessID:%S\n",
			GetProcessImageFullPathByID(CreateInfo->ParentProcessId),
			GetProcessImageFullPathByID(ProcessId)
			) 
	);
	if (wcswcs(GetProcessImageFullPathByID(ProcessId),L"notepad.exe") !=0)   // refuse notepad.exe
	{
		//修改返回结果为拒绝访问，使得创建进程失败
		CreateInfo->CreationStatus = STATUS_ACCESS_DENIED;
	}
	return;
}
VOID Unload(IN PDRIVER_OBJECT pDriverObject)
{
	UNREFERENCED_PARAMETER(pDriverObject);
	KdPrint(("unload driver \n"));
	PsSetCreateProcessNotifyRoutineEx(ProcessPre,TRUE);
}


NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObject,IN PUNICODE_STRING pRegistryPath)
{
	UNREFERENCED_PARAMETER(pDriverObject);
	UNREFERENCED_PARAMETER(pRegistryPath);

#ifdef _AMD64_
	DbgPrint("64\r\n");
	*((PCHAR)pDriverObject->DriverSection + 0x68) |= 0x20;
#else
	DbgPrint("32\r\n");
	*((PCHAR)pDriverObject->DriverSection + 0x34) |= 0x20;
#endif // _AMD64_

	NTSTATUS re = STATUS_SUCCESS;
	pDriverObject->DriverUnload = Unload;
	//PsSetCreateProcessNotifyRoutine只能记录进程创建或消亡，不能阻止进程的创建，
	//PsSetCreateProcessNotifyRoutineEx可以阻止进程的创建，先看一下相关函数：
	re = PsSetCreateProcessNotifyRoutineEx(ProcessPre,FALSE);
	if (!NT_SUCCESS(re))
	{
		KdPrint(("PsSetCreateProcessNotifyRoutineEx is error %x \n",re));
	}
	KdPrint(("hello world\n"));
	return re;
}