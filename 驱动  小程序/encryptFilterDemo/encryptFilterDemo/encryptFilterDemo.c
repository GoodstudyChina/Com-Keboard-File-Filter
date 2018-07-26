#include"encryptFilterDemo.h"
//////
PFLT_FILTER  g_filterHandle;
/////
NPAGED_LOOKASIDE_LIST g_ContextList;
//注册回调函数
CONST FLT_OPERATION_REGISTRATION Callbacks[] = {
	{
		IRP_MJ_READ,
		0,
		SfPreRead,
		SfPostRead    //读完数据后，对数据进行解密，并显示出来
	},
	{
		IRP_MJ_WRITE,
		0,
		SfPreWrite,  //写之前对数据进行加密操作
		SfPostWrite  
	},
{ IRP_MJ_OPERATION_END }
};
//清理卷上的上下文信息
CONST FLT_CONTEXT_REGISTRATION ContextNotifications[] = {

	{ FLT_VOLUME_CONTEXT,
	0,
	CleanupVolumeContext,
	sizeof(VOLUME_CONTEXT),
	'xcBS'
	},

{ FLT_CONTEXT_END }
};
//
//  定义文件过滤注入，结构
//
CONST FLT_REGISTRATION FilterRegistration = {

	sizeof(FLT_REGISTRATION),         //  Size
	FLT_REGISTRATION_VERSION,           //  Version
	0,                                  //  Flags
	ContextNotifications,               // 注册清理上下文的函数
	Callbacks,                          //  Operation callbacks
	FilterUnload,                           //  MiniFilterUnload
	InstanceSetup,                    // 在卷上绑定上下文
	InstanceQueryTeardown,            //  InstanceQueryTeardown
	NULL,            //  InstanceTeardownStart
	NULL,         //  InstanceTeardownComplete
	NULL,         //  GenerateFileName
	NULL,         //  GenerateDestinationFileName
	NULL          //  NormalizeNameComponent

};
//清理卷的上下文
VOID
CleanupVolumeContext(
	_In_ PFLT_CONTEXT Context,
	_In_ FLT_CONTEXT_TYPE ContextType
)
{
	PVOLUME_CONTEXT ctx = Context;

	PAGED_CODE();

	UNREFERENCED_PARAMETER(ContextType);

	FLT_ASSERT(ContextType == FLT_VOLUME_CONTEXT);

	if (ctx->Name.Buffer != NULL) {

		ExFreePool(ctx->Name.Buffer);
		ctx->Name.Buffer = NULL;
	}
}

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
NTSTATUS
InstanceQueryTeardown(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
)
{
	PAGED_CODE();
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);

	return STATUS_SUCCESS;
}
NTSTATUS
FilterUnload(
	_In_ FLT_FILTER_UNLOAD_FLAGS Flags
)
{
	PAGED_CODE();
	UNREFERENCED_PARAMETER(Flags);
	FltUnregisterFilter(g_filterHandle);
	ExDeleteNPagedLookasideList(&g_ContextList);
	return STATUS_SUCCESS;
}
NTSTATUS InstanceSetup(PCFLT_RELATED_OBJECTS FltObjects, FLT_INSTANCE_SETUP_FLAGS Flags, DEVICE_TYPE VolumeDeviceType, FLT_FILESYSTEM_TYPE VolumeFilesystemType)
{
	PAGED_CODE();
	//_asm int 3;
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);
	UNREFERENCED_PARAMETER(VolumeDeviceType);
	UNREFERENCED_PARAMETER(VolumeFilesystemType);
	/*为每一个卷，附加一个上下文*/
	PVOLUME_CONTEXT ctx = NULL;
	NTSTATUS status = STATUS_SUCCESS;
	ULONG retLen;
	UCHAR volPropBuffer[sizeof(FLT_VOLUME_PROPERTIES) + 512];
	PFLT_VOLUME_PROPERTIES volProp = (PFLT_VOLUME_PROPERTIES)volPropBuffer;
	PDEVICE_OBJECT devObj = NULL;
	PUNICODE_STRING workingName = NULL;
	USHORT size;
	try
	{
		status = FltAllocateContext(
			FltObjects->Filter,
			FLT_VOLUME_CONTEXT,
			sizeof(VOLUME_CONTEXT),
			NonPagedPool,
			&ctx
		);
		if (!NT_SUCCESS(status))
		{
			DbgPrint("InstanceSetup ,FltAllocateContext failed 0x%x \n", status);
			leave;
		}
		//得到磁盘的信息 获得该卷的属性
		status = FltGetVolumeProperties(
			FltObjects->Volume,
			volProp,
			sizeof(volPropBuffer),
			&retLen
		);
		if (!NT_SUCCESS(status))
		{
			DbgPrint("InstanceSetup,FltGetVolumePropertiese : 0x%x\n", status);
			leave;
		}
		//保存磁盘的信息
		ctx->SectorSize = max(volProp->SectorSize, 0x200);
		ctx->Name.Buffer = NULL;
		//获取该存储对象的，名称
		status = FltGetDiskDeviceObject(FltObjects->Volume, &devObj);
		if (NT_SUCCESS(status)) {
			//如果DOS name获取失败
			status = IoVolumeDeviceToDosName(devObj, &ctx->Name);
		}
		//尝试获取NT name
		if (!NT_SUCCESS(status))
		{
			if (volProp->RealDeviceName.Length > 0)
			{
				workingName = &volProp->RealDeviceName;
			}
			else if (volProp->FileSystemDeviceName.Length > 0) {
				workingName = &volProp->FileSystemDeviceName;
			}
			else {
				status = STATUS_FLT_DO_NOT_ATTACH;
				leave;
			}

			size = workingName->Length + sizeof(WCHAR);
			ctx->Name.Buffer = ExAllocatePoolWithTag(NonPagedPool,
				size,
				'mnBS');
			if (ctx->Name.Buffer == NULL)
			{
				status = STATUS_INSUFFICIENT_RESOURCES;
				leave;
			}
			ctx->Name.Length = 0;
			ctx->Name.MaximumLength = size;
			//复制名字
			RtlCopyUnicodeString(&ctx->Name, workingName);
			RtlAppendUnicodeToString(&ctx->Name,
				L":");
		}
		//设置上下文
		status = FltSetVolumeContext(FltObjects->Volume,
			FLT_SET_CONTEXT_KEEP_IF_EXISTS,
			ctx,
			NULL);
		if (status == STATUS_FLT_CONTEXT_ALREADY_DEFINED)
		{
			status = STATUS_SUCCESS;
		}
	}
	finally{
		if (ctx)
		FltReleaseContext(ctx);
	if (devObj)
		ObDereferenceObject(devObj);
	}
	return status;
}
VOID DriverUnload(PDRIVER_OBJECT DriverObject)
{
	UNREFERENCED_PARAMETER(DriverObject);
	FltUnregisterFilter(g_filterHandle);
}
FLT_PREOP_CALLBACK_STATUS
SetDecodeBuffer(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
)
{
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);
	NTSTATUS status = FLT_PREOP_SUCCESS_WITH_CALLBACK;
	PVOLUME_CONTEXT volCtx = NULL;
	ULONG readlen = Data->Iopb->Parameters.Read.Length;
	PVOID newBuf = NULL;
	PMDL newMdl = NULL;
	PPRE_2_POST_CONTEXT p2pCtx = NULL;
	try {
		if (Data->Iopb->Parameters.Read.Length == 0)
			leave;
		//获取卷的上下文信息
		status = FltGetVolumeContext(FltObjects->Filter, FltObjects->Volume, &volCtx);
		if (!NT_SUCCESS(status))
		{
			DbgPrint("SetDecodeBuffer --- FltGetVolumeContext failed : %x\n", status);
			leave;
		}
		if (FlagOn(IRP_NOCACHE, Data->Iopb->IrpFlags))
		{
			readlen = (ULONG)ROUND_TO_SIZE(readlen, volCtx->SectorSize);
		}
		newBuf = FltAllocatePoolAlignedWithTag(
			FltObjects->Instance,
			NonPagedPool,
			(SIZE_T)readlen,
			'bdBs'
		);
		if (newBuf == NULL)
		{
			DbgPrint("SetDecodeBuffer ----- FltGetVolumeContext failed : %x\n", status);
			leave;
		}
		if (FlagOn(Data->Flags, FLTFL_CALLBACK_DATA_IRP_OPERATION))
		{
			//只针对IRP操作，进行MDL的申请
			newMdl = IoAllocateMdl(
				newBuf,
				readlen,
				FALSE,
				FALSE,
				NULL
			);
			if (newBuf == NULL)
			{
				leave;
			}
			MmBuildMdlForNonPagedPool(newMdl);
		}
		p2pCtx = ExAllocateFromNPagedLookasideList(&g_ContextList);
		if (p2pCtx == NULL)
		{
			leave;
		}
		//更新 新的buffer指针
		Data->Iopb->Parameters.Read.ReadBuffer = newBuf;
		Data->Iopb->Parameters.Read.MdlAddress = newMdl;
		FltSetCallbackDataDirty(Data);
		DbgPrint("SetDecodeBuffer ---- update information success \n");
		//保存申请的资源 信息
		p2pCtx->newBuffer = newBuf;
		p2pCtx->VolCtx = volCtx;
		*CompletionContext = p2pCtx;
		//返回值
		status = FLT_PREOP_SUCCESS_WITH_CALLBACK;
	}
	finally{
		if (status != FLT_PREOP_SUCCESS_WITH_CALLBACK)
		{
			if (newBuf != NULL)
			{
				FltFreePoolAlignedWithTag(FltObjects->Instance, newBuf, 'bdBs');
			}
			if (newMdl != NULL)
			{
				IoFreeMdl(newMdl);
			}
			if (volCtx != NULL)
			{
				FltReleaseContext(volCtx);
			}
		}
	}
	return status;
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
	NTSTATUS status = FLT_PREOP_SUCCESS_WITH_CALLBACK;
	PFLT_FILE_NAME_INFORMATION nameInfo;
	char nameExtension[20] = { "" };
	status = FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT, &nameInfo);
	if (!NT_SUCCESS(status))
	{
		return FLT_PREOP_SUCCESS_WITH_CALLBACK;
	}
	status = FltParseFileNameInformation(nameInfo);
	NPUnicodeStringToChar(&nameInfo->Extension, nameExtension);
	DbgPrint("name extension is : %s\n", nameExtension);
	if (strstr(nameExtension, ".TXT"))
	{
		DbgPrint("Swap buffer preRead file name : %wZ\n", &nameInfo->Name);
		//交换读取内存
		status = SetDecodeBuffer(Data, FltObjects, CompletionContext);
	}
	FltReleaseFileNameInformation(nameInfo);
	return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}
FLT_POSTOP_CALLBACK_STATUS
SwapPostBufferWhenSafe(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
)
{
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);
	UNREFERENCED_PARAMETER(Flags);

	PPRE_2_POST_CONTEXT p2pCtx = CompletionContext;
	PVOID origBuf = NULL;
	NTSTATUS status;
	//将用户空间 锁住
	status = FltLockUserBuffer(Data);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("SwapPostReadBuffersWhenSafe  ---- FltLockUserBuffer failed : 0x%x\n", status);
		Data->IoStatus.Status = status;
		Data->IoStatus.Information = 0;
	}
	else {
		//获取空间
		origBuf = MmGetSystemAddressForMdlSafe(Data->Iopb->Parameters.Read.MdlAddress, NormalPagePriority | MdlMappingNoExecute);
		if (origBuf == NULL)
		{
			DbgPrint("SwapPostReadBuffersWhenSafe  ---- MmGetSystemAddressForMdlSafe failed :\n");
			Data->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
			Data->IoStatus.Information = 0;
		}
		else {
			RtlCopyMemory(origBuf, p2pCtx->newBuffer, Data->IoStatus.Information);
		}
	}
	FltFreePoolAlignedWithTag(FltObjects->Instance, p2pCtx->newBuffer, 'bdBs');
	FltReleaseContext(p2pCtx->VolCtx);
	ExFreeToNPagedLookasideList(&g_ContextList, p2pCtx);
	return FLT_POSTOP_FINISHED_PROCESSING;
}
FLT_POSTOP_CALLBACK_STATUS
DecodeBuffer(
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

	FLT_POSTOP_CALLBACK_STATUS retValue = FLT_POSTOP_FINISHED_PROCESSING;
	PVOID origBuf = NULL;
	PPRE_2_POST_CONTEXT p2pCtx = CompletionContext;
	BOOLEAN cleanupAllocatedBuffer = TRUE;
	try
	{
		if (!NT_SUCCESS(Data->IoStatus.Status) || (Data->IoStatus.Information == 0))
		{
			leave;
		}
		if (Data->Iopb->Parameters.Read.MdlAddress != NULL)
		{
			origBuf = MmGetSystemAddressForMdlSafe(Data->Iopb->Parameters.Read.MdlAddress,
				NormalPagePriority | MdlMappingNoExecute
			);
			if (origBuf)
			{
				DbgPrint("DecodeBuffer --- MmGetSystemAddressForMdlSafe ");
				Data->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
				Data->IoStatus.Information = 0;
				leave;
			}
		}
		else if (FlagOn(Data->Flags, FLTFL_CALLBACK_DATA_SYSTEM_BUFFER) ||
			FlagOn(Data->Flags, FLTFL_CALLBACK_DATA_FAST_IO_OPERATION)) {
			origBuf = Data->Iopb->Parameters.Read.ReadBuffer;
		}
		else {
			if (FltDoCompletionProcessingWhenSafe(Data,
				FltObjects,
				CompletionContext,
				Flags,
				SwapPostBufferWhenSafe,
				&retValue))
			{
				cleanupAllocatedBuffer = TRUE;
			}
			else {
				Data->IoStatus.Status = STATUS_UNSUCCESSFUL;
				Data->IoStatus.Information = 0;
			}
			leave;
		}
		//交换读取到的数据
		try
		{
			RtlCopyMemory(origBuf, p2pCtx->newBuffer, Data->IoStatus.Information);
		}except(EXCEPTION_EXECUTE_HANDLER) {
			Data->IoStatus.Status = GetExceptionCode();
			Data->IoStatus.Information = 0;
		}
	}
	finally{
		if (cleanupAllocatedBuffer)
		{
			FltFreePoolAlignedWithTag(FltObjects->Instance, p2pCtx->newBuffer, 'bdBs');
			FltReleaseContext(p2pCtx->VolCtx);
			ExFreeToNPagedLookasideList(&g_ContextList, p2pCtx);
		}
	}
	return retValue;
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
	FLT_POSTOP_CALLBACK_STATUS status = FLT_POSTOP_FINISHED_PROCESSING;
	PFLT_FILE_NAME_INFORMATION nameInfo;
	char nameExtension[20] = { "" };
	status = FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT, &nameInfo);
	if (!NT_SUCCESS(status))
	{
		return FLT_PREOP_SUCCESS_WITH_CALLBACK;
	}
	FltParseFileNameInformation(nameInfo);
	NPUnicodeStringToChar(&nameInfo->Extension, nameExtension);
	DbgPrint("name extension is : %s\n", nameExtension);
	if (strstr(nameExtension, ".TXT"))
	{
		DbgPrint("decode file name : %wZ\n", &nameInfo->Name);
		//解密操作
		status = DecodeBuffer(Data, FltObjects, CompletionContext, Flags);
	}
	FltReleaseFileNameInformation(nameInfo);
	return FLT_POSTOP_FINISHED_PROCESSING;
}
FLT_POSTOP_CALLBACK_STATUS
SfPostWrite(
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
	if (CompletionContext != NULL)
	{
		/*释放掉在pre申请的空间内容*/
		PPRE_2_POST_CONTEXT p2pCtx = NULL;
		//PVOID pvoid = MmGetSystemAddressForMdlSafe(p2pCtx->newMDL, NormalPagePriority | MdlMappingNoExecute);  //失效的引用
		//UNREFERENCED_PARAMETER(pvoid);
		p2pCtx = (PPRE_2_POST_CONTEXT)CompletionContext;
		FltFreePoolAlignedWithTag(FltObjects->Instance, p2pCtx->newBuffer, 'abc');
		//IoFreeMdl(p2pCtx->newMDL);  //newMDL已经被释放掉了
		FltReleaseContext(p2pCtx->VolCtx);
		ExFreeToNPagedLookasideList(&g_ContextList, p2pCtx);
	}
	return FLT_POSTOP_FINISHED_PROCESSING;
}
/*swapBuffer 加密数据内容*/
FLT_PREOP_CALLBACK_STATUS	EncryptBuffer(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
)
{
	ULONG writeLen = Data->Iopb->Parameters.Write.Length;
	PVOLUME_CONTEXT volCtx = NULL;
	NTSTATUS status;
	PVOID newBuf = NULL;
	PMDL newMdl = NULL;
	PVOID origBuf = NULL;
	FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_SUCCESS_NO_CALLBACK;
	PPRE_2_POST_CONTEXT p2pCtx = NULL;
	try
	{
		if (writeLen == 0)
			leave;
		/*IRP_NOCACHE 这是一个非高速缓存的I/O，将readlen长度最大化为此设备的扇区大小*/
		status = FltGetVolumeContext(FltObjects->Filter, FltObjects->Volume, &volCtx);
		if (!NT_SUCCESS(status))
		{
			DbgPrint("FltGetVolumeContext failed : 0x%x \n ", status);
			leave;
		}
		if (FlagOn(IRP_NOCACHE, Data->Iopb->IrpFlags))
		{
			writeLen = (ULONG)ROUND_TO_SIZE(writeLen, volCtx->SectorSize);
		}
		newBuf = FltAllocatePoolAlignedWithTag(
			FltObjects->Instance,
			NonPagedPool,
			(SIZE_T)writeLen,
			'abc');
		if (newBuf == NULL)
		{
			DbgPrint("FltAllocatePoolAlignedWithTag failed ");
			leave;
		}

		/*判断Flags的值*/
		if (FlagOn(Data->Flags, FLTFL_CALLBACK_DATA_IRP_OPERATION))
		{
			newMdl = IoAllocateMdl(
				newBuf,
				writeLen,
				FALSE,
				FALSE,
				NULL
			);
			if (newBuf == NULL)
			{
				DbgPrint("IoAllocateMdl failed");
				leave;
			}
			MmBuildMdlForNonPagedPool(newMdl);
		}
		/*获取写入数据 的内存位置*/
		if (Data->Iopb->Parameters.Write.MdlAddress != NULL)
		{
			origBuf = MmGetSystemAddressForMdlSafe(Data->Iopb->Parameters.Write.MdlAddress, NormalPagePriority | MdlMappingNoExecute);
			if (origBuf == NULL)
			{
				DbgPrint("MmGetSystemAddressForMdlSafe failed \n");
				Data->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
				Data->IoStatus.Information = 0;
				retValue = FLT_PREOP_COMPLETE;
				leave;
			}
		}
		else {
			origBuf = Data->Iopb->Parameters.Write.WriteBuffer;
		}
		/*复制内存中的内容 ****/
		/*对newBuf中的数据进行,进行加密操作*/
		try
		{
			RtlCopyMemory(newBuf, origBuf, writeLen);
		}except(EXCEPTION_EXECUTE_HANDLER) {
			/*复制失败*/
			DbgPrint("RtlCopyMemory failed \n");
			Data->IoStatus.Status = GetExceptionCode();
			Data->IoStatus.Information = 0;
			retValue = FLT_PREOP_COMPLETE;
			leave;
		}

		p2pCtx = ExAllocateFromNPagedLookasideList(&g_ContextList);
		if (p2pCtx == NULL)
		{
			DbgPrint("ExAllocataFromNPagedLookasideList failed \n");
		}
////////////////*************操作newBuf中的内容，进行数据的加解密***********************************
		p2pCtx->newBuffer = newBuf;
		p2pCtx->VolCtx = volCtx;
		*CompletionContext = p2pCtx;   //传到post函数中
									   //操作newBufD的数据
		Data->Iopb->Parameters.Write.WriteBuffer = newBuf;
		Data->Iopb->Parameters.Write.MdlAddress = newMdl;
		FltSetCallbackDataDirty(Data);
		/*设置返回值*/

		retValue = FLT_PREOP_SUCCESS_WITH_CALLBACK;
	}
	finally{
		if (retValue != FLT_PREOP_SUCCESS_WITH_CALLBACK)
		{
			if (newBuf != NULL)
			{
				FltFreePoolAlignedWithTag(FltObjects->Instance,
					newBuf,
					'abc');
			}
			if (newMdl != NULL)
			{
				IoFreeMdl(newMdl);
			}
			if (volCtx != NULL)
			{
				FltReleaseContext(volCtx);
			}
		}
	}
	return retValue;
}
/*写操作的回调函数*/
FLT_PREOP_CALLBACK_STATUS
SfPreWrite(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
)
{
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);
	NTSTATUS retValue = FLT_PREOP_SUCCESS_NO_CALLBACK;
	PFLT_FILE_NAME_INFORMATION nameInfo;
	NTSTATUS status;
	char uName[20] = { "" };
	DbgPrint("Entry SfPreWrite \n");
	status = FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT, &nameInfo);
	if (!NT_SUCCESS(status))
	{
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}
	status = FltParseFileNameInformation(nameInfo);
	if (!NT_SUCCESS(status))
	{
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	nameInfo->Extension;
	NPUnicodeStringToChar(&nameInfo->Extension, uName);
	if (strstr(uName, ".TXT"))
	{
		DbgPrint("encrypt file name : %wZ\n", &nameInfo->Name);
		//数据进行加密，并放入到内存中
		retValue = EncryptBuffer(Data, FltObjects, CompletionContext);
	}

	FltReleaseFileNameInformation(nameInfo);
	return FLT_PREOP_SUCCESS_NO_CALLBACK;
}