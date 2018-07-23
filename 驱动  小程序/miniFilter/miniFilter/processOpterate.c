#include"processOpterate.h"
#pragma data_seg()
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

	QUERY_INFO_PROCESS ZwQueryInformationProcess = NULL;


	//PAGED_CODE();  // this eliminates the possibility of the IDLE Thread/Process   

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
		//KdPrint((" buffer is samll \r\n"));
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