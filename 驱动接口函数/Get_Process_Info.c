Guid:
    GetProcessImageNameByProcessID 通过进程ID获取进程的名字，限制16字节。 在32位系统下测试好用。
	GetProcessImageFullPathByID   通过进程ID返回进程的全路径 函数在x32位系统下，好用，64位系统下不能返回在函数中申请的空间。  gxb 2018.6.5 
	GetProcessImagePath   通过进程ID返回进程的全路径  在x32和x64位系统下全好用。gxb 2018.6.5 









//declare section 
NTKERNELAPI
UCHAR * PsGetProcessImageFileName(__in PEPROCESS Process);

NTKERNELAPI NTSTATUS  PsLookupProcessByProcessId(
	HANDLE    ProcessId,
	PEPROCESS *Process
);
///gxb 2018.6.4.  Get Process name.
//output example:
//			explorer.exe services.exe svchost.ext
char* GetProcessImageNameByProcessID(ULONG ulProcessID)
{
	NTSTATUS  Status;
	PEPROCESS  EProcess = NULL;


	Status = PsLookupProcessByProcessId((HANDLE)ulProcessID, &EProcess);    //PsLookupProcessByProcessId need declare
																		
	if (!NT_SUCCESS(Status))
	{
		return FALSE;
	}
	ObDereferenceObject(EProcess);
	//Get process image by EProcess  
	return (char*)PsGetProcessImageFileName(EProcess);

}


////////////////////////------------------------------------------------------------------------------------------///////////////
//gxb 2018.6.4 Get Proces full Path
/////
//return : The process full path.
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

///////////-------------------------//////////////////////////////
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
		KdPrint(("gxb buffer is samll \r\n"));
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
		KdPrint(("------%S\n",imageName->Buffer));
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