#include<ntifs.h>

NTSTATUS GetProcessImageFullPathByID(_Out_ PUNICODE_STRING ProcessImageFullPath,IN PHANDLE pID)
{
	
	typedef NTSTATUS NTAPI NTQUERYINFORMATIONPROCESS(
		_In_		HANDLE ProcessHandle,
		_In_		PROCESSINFOCLASS ProcessInformationClass,
		_Out_		PVOID ProcessInformation,
		_In_		ULONG ProcessInformationLength,
		_Out_opt_	PULONG ReturnLength
	);
	typedef NTQUERYINFORMATIONPROCESS FAR * LPNTQUERYINFORMATIONPROCESS;


	NTSTATUS Status = STATUS_SUCCESS;
	ULONG  returnedLength;
	LPNTQUERYINFORMATIONPROCESS ZwQueryInformationProcess = NULL;
	ULONG  bufferLength;
	PVOID buffer;
	PUNICODE_STRING imageName;

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
	if (STATUS_INFO_LENGTH_MISMATCH != Status) {

		KdPrint(("ZwQueryInformationProcess error code %x ", Status));
		return Status;
	}
	

	InitializeObjectAttributes(&ObjectAttributes, 0, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, 0, 0);
	parentID.UniqueProcess = pID;
	parentID.UniqueThread = 0;
	Status = ZwOpenProcess(&processHandle, PROCESS_ALL_ACCESS, &ObjectAttributes, &parentID);
	if (!NT_SUCCESS(Status))
	{
		KdPrint(("ZwOpenProcess error code : %d \n", Status));
		return Status;
	}

	Status = ZwQueryInformationProcess(processHandle,
		ProcessImageFileName,
		NULL,//buffer
		0,  //buffer size
		&returnedLength
	);

	if (STATUS_INFO_LENGTH_MISMATCH != Status) {

		return  Status;

	}

	//   
	// Is the passed-in buffer going to be big enough for us?    
	// This function returns a single contguous buffer model...   
	//   
	bufferLength = returnedLength - sizeof(UNICODE_STRING);

	if (ProcessImageFullPath->MaximumLength < bufferLength) {

		ProcessImageFullPath->Length = (USHORT)returnedLength;
		ProcessImageFullPath->MaximumLength = (USHORT)returnedLength;
		return  STATUS_BUFFER_OVERFLOW;
	}
	//   
	// If we get here, the buffer IS going to be big enough for us, so   
	// let's allocate some storage.   
	//   
	buffer = ExAllocatePoolWithTag(PagedPool, returnedLength, 'gxb');

	if (NULL == buffer) {

		return  STATUS_INSUFFICIENT_RESOURCES;

	}

	//   
	// Now lets go get the data   
	//   
	Status = ZwQueryInformationProcess(processHandle,
		ProcessImageFileName,
		buffer,
		returnedLength,
		&returnedLength);

	if (NT_SUCCESS(Status)) {
		//   
		// Ah, we got what we needed   
		//   
		imageName = (PUNICODE_STRING)buffer;
		KdPrint(("888--1111  %S", imageName->Buffer));
		RtlCopyUnicodeString(ProcessImageFullPath, imageName);

	}

	ZwClose(processHandle);

	//   
	// free our buffer   
	//   
	ExFreePool(buffer);

	//   
	// And tell the caller what happened.   
	//      
	return  Status;
}
NTSTATUS GetCurrentProcessImageFullPath(_Out_ PUNICODE_STRING ProcessImageName, _In_ PHANDLE PId)
{

	typedef NTSTATUS NTAPI NTQUERYINFORMATIONPROCESS(
		_In_		HANDLE ProcessHandle,
		_In_		PROCESSINFOCLASS ProcessInformationClass,
		_Out_		PVOID ProcessInformation,
		_In_		ULONG ProcessInformationLength,
		_Out_opt_	PULONG ReturnLength
	);
	typedef NTQUERYINFORMATIONPROCESS FAR * LPNTQUERYINFORMATIONPROCESS;

	NTSTATUS status = STATUS_SUCCESS;
	ULONG  returnedLength;
	LPNTQUERYINFORMATIONPROCESS ZwQueryInformationProcess = NULL;

	HANDLE hProcess;
	CLIENT_ID clentID;
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
	clentID.UniqueProcess = PId;
	clentID.UniqueThread = 0;
	status = ZwOpenProcess(&hProcess, PROCESS_ALL_ACCESS, &ObjectAttributes, &clentID);

	if (!NT_SUCCESS(status))
	{
		KdPrint(("ZwOpenProcess error code : %d \n", status));
		return status;
	}


	status = ZwQueryInformationProcess(hProcess,
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
		ProcessImageName->Length = (USHORT)returnedLength;
		KdPrint(("ProcessImageName's Buffer Is Toooo small %d \r\n", returnedLength));
		return STATUS_BUFFER_OVERFLOW;
	}
	//
	// If we get here, the buffer IS going to be big enough for us, so
	// let's allocate some storage.
	//
	status = ZwQueryInformationProcess(hProcess,
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
NTSTATUS
GetProcessImagePath(
	OUT PUNICODE_STRING ProcessImagePath,
	IN   HANDLE    dwProcessId
)
{
	NTSTATUS Status;
	HANDLE  hProcess;
	PEPROCESS pEprocess;
	ULONG  returnedLength;
	ULONG  bufferLength;
	PVOID  buffer;
	PUNICODE_STRING imageName;


	typedef NTSTATUS(*QUERY_INFO_PROCESS) (
		__in HANDLE ProcessHandle,
		__in PROCESSINFOCLASS ProcessInformationClass,
		__out_bcount(ProcessInformationLength) PVOID ProcessInformation,
		__in ULONG ProcessInformationLength,
		__out_opt PULONG ReturnLength
		);

	QUERY_INFO_PROCESS ZwQueryInformationProcess =NULL;


	PAGED_CODE();  // this eliminates the possibility of the IDLE Thread/Process   

	if (NULL == ZwQueryInformationProcess) {

		UNICODE_STRING routineName;

		RtlInitUnicodeString(&routineName, L"ZwQueryInformationProcess");

		ZwQueryInformationProcess =
			(QUERY_INFO_PROCESS)MmGetSystemRoutineAddress(&routineName);

		if (NULL == ZwQueryInformationProcess) {
			DbgPrint("Cannot resolve ZwQueryInformationProcess/n");
		}
	}

	Status = PsLookupProcessByProcessId((HANDLE)dwProcessId, &pEprocess);
	if (!NT_SUCCESS(Status))
		return Status;

	Status = ObOpenObjectByPointer(pEprocess,           // Object   
		OBJ_KERNEL_HANDLE,   // HandleAttributes   
		NULL,                // PassedAccessState OPTIONAL   
		GENERIC_READ,        // DesiredAccess   
		*PsProcessType,      // ObjectType   
		KernelMode,          // AccessMode   
		&hProcess);
	if (!NT_SUCCESS(Status))
		return Status;


	//   
	// Step one - get the size we need   
	//   
	Status = ZwQueryInformationProcess(hProcess,
		ProcessImageFileName,
		NULL,  // buffer   
		0,  // buffer size   
		&returnedLength);


	if (STATUS_INFO_LENGTH_MISMATCH != Status) {
		KdPrint(("ZwQueryInformationProcess don't get buffer length\r\n"));
		return  Status;

	}

	//   
	// Is the passed-in buffer going to be big enough for us?    
	// This function returns a single contguous buffer model...   
	//   
	bufferLength = returnedLength - sizeof(UNICODE_STRING);

	if (ProcessImagePath->MaximumLength < bufferLength) {

		ProcessImagePath->Length = (USHORT)returnedLength;
		ProcessImagePath->MaximumLength = (USHORT)returnedLength;
		KdPrint((" buffer is samll \r\n"));
		return  STATUS_BUFFER_OVERFLOW;

	}

	//   
	// If we get here, the buffer IS going to be big enough for us, so   
	// let's allocate some storage.   
	//   
	buffer = ExAllocatePoolWithTag(PagedPool, returnedLength, 'ipgD');

	if (NULL == buffer) {

		return  STATUS_INSUFFICIENT_RESOURCES;

	}

	//   
	// Now lets go get the data   
	//   
	Status = ZwQueryInformationProcess(hProcess,
		ProcessImageFileName,
		buffer,
		returnedLength,
		&returnedLength);

	if (NT_SUCCESS(Status)) {
		//   
		// Ah, we got what we needed   
		//   
		imageName = (PUNICODE_STRING)buffer;
		//KdPrint(("------%S\n",imageName->Buffer));
		RtlCopyUnicodeString(ProcessImagePath, imageName);

	}

	ZwClose(hProcess);

	//   
	// free our buffer   
	//   
	ExFreePool(buffer);

	//   
	// And tell the caller what happened.   
	//      
	return  Status;

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
	UNICODE_STRING ParentName;
	UNICODE_STRING ProcessName;

	if (NULL == CreateInfo)
	{
		KdPrint((" gxb Process Exit\r\n"));
		return;
	}


	ParentName.Length = 0;
	ParentName.MaximumLength = 0;
	ParentName.Buffer = NULL;

	if (GetProcessImagePath(&ParentName, CreateInfo->ParentProcessId) == STATUS_BUFFER_OVERFLOW)
	{
		ParentName.Buffer = ExAllocatePoolWithTag(NonPagedPool, ParentName.Length,'me');
		ParentName.MaximumLength = ParentName.Length;
		GetProcessImagePath(&ParentName, CreateInfo->ParentProcessId);
	}

	ProcessName.Length = 0;
	ProcessName.MaximumLength = 0;
	ProcessName.Buffer = NULL;
	if (GetProcessImagePath(&ProcessName, ProcessId) == STATUS_BUFFER_OVERFLOW)
	{
		ProcessName.Buffer = ExAllocatePoolWithTag(NonPagedPool, ProcessName.Length, 'nn');
		ProcessName.MaximumLength = ProcessName.Length;
		GetProcessImagePath(&ProcessName, ProcessId);
	}

	KdPrint(
		("gxb Create process \n gxbParentName:%S \n gxbProcessName:%S   \n",
			ParentName.Buffer,
			ProcessName.Buffer
			)
	);

	if (wcswcs(ProcessName.Buffer,L"SCBYDL") !=0)   // refuse notepad.exe
	{
		//修改返回结果为拒绝访问，使得创建进程失败
		CreateInfo->CreationStatus = STATUS_ACCESS_DENIED;
		KdPrint(("gxb refuse Process %S \r\n", ProcessName.Buffer));
	}
	ExFreePool(ParentName.Buffer);
	ExFreePool(ProcessName.Buffer);
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



	////PsSetLoadImageNotifyRoutine
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