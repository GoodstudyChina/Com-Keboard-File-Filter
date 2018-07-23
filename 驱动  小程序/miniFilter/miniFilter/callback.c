#pragma once 
#include"callback.h"

BOOLEAN NPUnicodeStringToChar(PUNICODE_STRING UniName, char Name[])
{
	ANSI_STRING	AnsiName;
	NTSTATUS	ntstatus;
	char*		nameptr;

	__try {
		ntstatus = RtlUnicodeStringToAnsiString(&AnsiName, UniName, TRUE);

		if (AnsiName.Length < 260) {
			nameptr = (PCHAR)AnsiName.Buffer;
			//Convert into upper case and copy to buffer
			strcpy(Name, _strupr(nameptr));
			//DbgPrint("NPUnicodeStringToChar : %s\n", Name);
		}
		RtlFreeAnsiString(&AnsiName);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		DbgPrint("NPUnicodeStringToChar EXCEPTION_EXECUTE_HANDLER\n");
		return FALSE;
	}
	return TRUE;
}

FLT_PREOP_CALLBACK_STATUS
SfPreCreate(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
)
{
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);
	return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}


FLT_POSTOP_CALLBACK_STATUS
SfPostCreate(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_opt_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
)
{
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);
	UNREFERENCED_PARAMETER(Flags);
	NTSTATUS status;
	PFLT_FILE_NAME_INFORMATION nameInfo;
	ULONG options;
	UNICODE_STRING unName;
	UNICODE_STRING ProcessName;
	char exName[256] = {""};
	/**************************获取文件名字*********************/
	status = FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT, &nameInfo);
	if (!NT_SUCCESS(status))
	{
		//STATUS_FLT_INVALID_NAME_REQUEST 0xC01C0005L 
		//DbgPrint("SfPostCreate : FltGetFileNameInformation error . 0x%x\n", status);
		return FLT_POSTOP_FINISHED_PROCESSING;
	}
	status = FltParseFileNameInformation(nameInfo);
	if (!NT_SUCCESS(status))
	{
		//DbgPrint("SfPostCreate : FltParseFileNameInformation error . 0x%x\n", status);
		return FLT_POSTOP_FINISHED_PROCESSING;
	}
	NPUnicodeStringToChar(&nameInfo->Extension, exName);
	/******************判断是目录还是文件***********************/
	options = Data->Iopb->Parameters.Create.Options;
	if ((options & FILE_DIRECTORY_FILE) != 0)
		RtlInitUnicodeString(&unName, L"Path name");
	else
		RtlInitUnicodeString(&unName, L"Full file name");
	/********************获取当前的进程名和ID**********************/
	HANDLE hProcess = PsGetCurrentProcessId();
	ProcessName.Length = 0;
	ProcessName.MaximumLength = 0;
	ProcessName.Buffer = NULL;

	if (GetProcessImagePath(&ProcessName, hProcess) == STATUS_BUFFER_OVERFLOW)
	{
		ProcessName.Buffer = ExAllocatePoolWithTag(NonPagedPool, ProcessName.Length, 'me');
		ProcessName.MaximumLength = ProcessName.Length;
		GetProcessImagePath(&ProcessName, hProcess);
	}

	if (strstr(exName, "TXT"))
	{
		DbgPrint("-------------------------------------\n");
		DbgPrint("miniFilter - Post  IRP_MJ_CREATE \n ");
		DbgPrint("%wZ name:%wZ\n", &unName, &nameInfo->Name);
		DbgPrint("Process name:%wZ	\n", &ProcessName);
		DbgPrint("ProcessID:%d ", hProcess);
		DbgPrint("--------------------------------------\n");
	}
	FltReleaseFileNameInformation(nameInfo);
	return FLT_POSTOP_FINISHED_PROCESSING;
}

FLT_PREOP_CALLBACK_STATUS
SfPreWrite(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
)
{
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);
	PAGED_CODE();
	{
		PFLT_FILE_NAME_INFORMATION nameInfo;
		//直接获得文件名并检查
		if (NT_SUCCESS(FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT, &nameInfo)))
		{
			if (NT_SUCCESS(FltParseFileNameInformation(nameInfo)))
			{
				WCHAR pTempBuf[512] = { 0 };
				WCHAR *pNonPageBuf = NULL, *pTemp = pTempBuf;
				if (nameInfo->Name.MaximumLength > 512)
				{
					pNonPageBuf = ExAllocatePool(NonPagedPool, nameInfo->Name.MaximumLength);
					pTemp = pNonPageBuf;
				}
				RtlCopyMemory(pTemp, nameInfo->Name.Buffer, nameInfo->Name.MaximumLength);
				DbgPrint("[MiniFilter][IRP_MJ_WRITE]%wZ", &nameInfo->Name);
				_wcsupr(pTemp);
				if (NULL != wcsstr(pTemp, L"xxxx.txt")) 
				{
					if (NULL != pNonPageBuf)
						ExFreePool(pNonPageBuf);
					FltReleaseFileNameInformation(nameInfo);
					return FLT_PREOP_DISALLOW_FASTIO;
				}
				if (NULL != pNonPageBuf)
					ExFreePool(pNonPageBuf);
			}
			FltReleaseFileNameInformation(nameInfo);
		}
	}
	return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

FLT_PREOP_CALLBACK_STATUS
SfPreSetInfor(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
)
{
	NTSTATUS status;
	PFLT_FILE_NAME_INFORMATION nameInfo;
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);
	/**************************获取文件名字*********************/
	status = FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT, &nameInfo);
	if (!NT_SUCCESS(status))
	{
		//STATUS_FLT_INVALID_NAME_REQUEST 0xC01C0005L 
		DbgPrint("SfPostCreate : FltGetFileNameInformation error . 0x%x\n", status);
		return status;
	}
	status = FltParseFileNameInformation(nameInfo);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("SfPostCreate : FltParseFileNameInformation error . 0x%x\n", status);
		return status;
	}

	switch (Data->Iopb->Parameters.SetFileInformation.FileInformationClass)
	{
		case FileDispositionInformation:   //shift+delte 删除文件
		{
			DbgPrint("shift+delet file : %wZ \n ", &nameInfo->Name);
		}
			break;
		case FileRenameInformation:   //改名操作,回收站走这里
		{
			//被改成的名字
			PFILE_RENAME_INFORMATION pRnameInfo = (PFILE_RENAME_INFORMATION)Data->Iopb->Parameters.SetFileInformation.InfoBuffer;
			UNICODE_STRING pNewName;
			char name[256] = {""};
			RtlInitUnicodeString(&pNewName, pRnameInfo->FileName);
			//判断路径中是否存在Recycle.bin .
			NPUnicodeStringToChar(&pNewName, name);
			if (strstr(name, "RECYCLE.BIN"))
			{	//删除文件
				DbgPrint("recycle fie  :  %wZ \n",&nameInfo->Name);
			}	
		}
			break;
		default:
			break;
	}

	FltReleaseFileNameInformation(nameInfo);
	return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}


FLT_PREOP_CALLBACK_STATUS
SfPreRead(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
)
{
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);
	return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}

FLT_POSTOP_CALLBACK_STATUS
SfPostRead(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_opt_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
)
{
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);
	UNREFERENCED_PARAMETER(Flags);
	//NTSTATUS status;
	char exName[256] = { "" };
	PVOID Readbuffer = NULL;
	

	HANDLE hProcess = PsGetCurrentProcessId();
	UNICODE_STRING ProcessName;
	ProcessName.Length = 0;
	ProcessName.MaximumLength = 0;
	ProcessName.Buffer = NULL;

	if (GetProcessImagePath(&ProcessName, hProcess) == STATUS_BUFFER_OVERFLOW)
	{
		ProcessName.Buffer = ExAllocatePoolWithTag(NonPagedPool, ProcessName.Length, 'me');
		ProcessName.MaximumLength = ProcessName.Length;
		GetProcessImagePath(&ProcessName, hProcess);
	}
	
	NPUnicodeStringToChar(&ProcessName, exName);
	DbgPrint("*SfPostRead*************Process name:%s	\n", exName);
	
	if (strstr(exName, "NOTEPAD"))
	{
		Readbuffer=FltGetNewSystemBufferAddress(Data);
		if (FlagOn(Data->Iopb->IrpFlags, DO_BUFFERED_IO))
		{
			DbgPrint("缓冲区IO\n");
		}
		else {
			if (FlagOn(Data->Iopb->IrpFlags, DO_DIRECT_IO))
			{
				DbgPrint("直接IO\n");
			}
			else {
				DbgPrint("其他IO\n");
			}
		}
	}
	return FLT_POSTOP_FINISHED_PROCESSING;
}
