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