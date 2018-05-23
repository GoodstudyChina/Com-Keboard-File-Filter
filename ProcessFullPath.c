#include<ntddk.h>

VOID UnloadDriver(PDRIVER_OBJECT pDriver);
VOID CreateProcessRoutineSpy(
	IN HANDLE ParentID,
	IN HANDLE ProcessID,
	IN BOOLEAN Create
);
NTSTATUS GetCurrentProcessImageFullPath(PUNICODE_STRING ProcessImageName);

NTSTATUS DriverEntry(PDRIVER_OBJECT pDriver, PUNICODE_STRING Registry)
{
	NTSTATUS status = STATUS_SUCCESS;
	UNREFERENCED_PARAMETER(pDriver);
	UNREFERENCED_PARAMETER(Registry);
	KdPrint(("gxb Driver install success\n"));

	status = PsSetCreateProcessNotifyRoutine(CreateProcessRoutineSpy, FALSE);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("PsSetCreateProcessNotifyRoutine faile status: %d \n", status));
	}
	pDriver->DriverUnload = UnloadDriver;
	return status;
}

VOID UnloadDriver(PDRIVER_OBJECT pDriver)
{
	UNREFERENCED_PARAMETER(pDriver);
	NTSTATUS status;
	status = PsSetCreateProcessNotifyRoutine(CreateProcessRoutineSpy,TRUE);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("uninstall PsSetCreateThreadNotifyRoutine is faile status : %d\n ",status));
	}
	return;
}
VOID CreateProcessRoutineSpy(
	IN HANDLE ParentID,
	IN HANDLE ProcessID,
	IN BOOLEAN Create
)
{
	UNREFERENCED_PARAMETER(ParentID);
	UNREFERENCED_PARAMETER(ProcessID);
	UNREFERENCED_PARAMETER(Create);
	//HANDLE ProcessHandle;
	NTSTATUS re;
	WCHAR s[1024] = L"";
	UNICODE_STRING ProcessImageName;

	if (Create)
	{
		KdPrint(("Process Created.ParentID: (%d) Processid %d\n", ParentID, ProcessID));
	}
	else{
		KdPrint(("Process Terminated.ParentID: (%d) Processid %d\n", ParentID, ProcessID));
	}


	ProcessImageName.Buffer = NULL;
	ProcessImageName.Length =0;
	ProcessImageName.MaximumLength = 0;

	re = GetCurrentProcessImageFullPath(&ProcessImageName);
	if (STATUS_BUFFER_OVERFLOW == re)
	{
		ProcessImageName.Buffer = ExAllocatePoolWithTag(NonPagedPool,
			ProcessImageName.Length,
			'gxb');
		ProcessImageName.MaximumLength = ProcessImageName.Length;
		re = GetCurrentProcessImageFullPath(&ProcessImageName);
	}
	KdPrint(("sssssssssssssss%wZ \n", ProcessImageName.Buffer));
}
/*
/第一次调用改函数，将会在PUNICODE_STRING的变量中存放路径的大小
/第二次进行实际的调用操作。
*/
NTSTATUS GetCurrentProcessImageFullPath(_Out_ PUNICODE_STRING ProcessImageName)
{
	typedef NTSTATUS NTAPI NTQUERYINFORMATIONPROCESS(
		_In_		HANDLE ProcessHandle,
		_In_		PROCESSINFOCLASS ProcessInformationClass,
		_Out_		PVOID ProcessInformation,
		_In_		ULONG ProcessInformationLength,
		_Out_opt_	PULONG ReturnLength
	);
	typedef NTQUERYINFORMATIONPROCESS FAR * LPNTQUERYINFORMATIONPROCESS;

	NTSTATUS status=STATUS_SUCCESS;
	ULONG  returnedLength;
	LPNTQUERYINFORMATIONPROCESS ZwQueryInformationProcess = NULL;


	PAGED_CODE();
	if (NULL == ZwQueryInformationProcess)
	{
		UNICODE_STRING routinName;
		RtlInitUnicodeString(&routinName, L"ZwQueryInformationProcess");
		ZwQueryInformationProcess = (LPNTQUERYINFORMATIONPROCESS)MmGetSystemRoutineAddress(&routinName);
		if (NULL == ZwQueryInformationProcess)
			KdPrint(("Cannot resolve ZwQueryInformationProcess\n"));
	}

	status = ZwQueryInformationProcess(NtCurrentProcess(),
		ProcessImageFileName,
		NULL,//buffer
		0,  //buffer size
		&returnedLength
	);
	if (STATUS_INFO_LENGTH_MISMATCH != status)
		return status;
	//
	// Is the passed-in buffer going to be big enough for us? 
	// This function returns a single contguous buffer model...
	//
	if (ProcessImageName->MaximumLength < returnedLength) {
		ProcessImageName->Length =(USHORT) returnedLength;
		KdPrint(("ProcessImageName's Buffer Is Toooo small %d \r\n", returnedLength));
		return STATUS_BUFFER_OVERFLOW;
	}
	//
	// If we get here, the buffer IS going to be big enough for us, so
	// let's allocate some storage.
	//
	status = ZwQueryInformationProcess(NtCurrentProcess(),
		ProcessImageFileName,
		ProcessImageName->Buffer,
		ProcessImageName->Length,
		&returnedLength);
	if (NT_SUCCESS(status))
	{
		KdPrint(("current n %wz\n", ProcessImageName->Buffer));
	}

	return status;
}