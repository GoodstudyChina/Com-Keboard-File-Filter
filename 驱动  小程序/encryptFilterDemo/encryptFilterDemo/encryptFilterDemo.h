/*
encryptFilterDemo 工程介绍

对.txt文件，进行简单的异或加密。

在读文件之后，进行解密；在写文件之间进行加密。

使用swap buffer进行缓冲区的加解密

*/

#include <fltKernel.h>
#include <dontuse.h>
#include <suppress.h>
//全局变量声明
extern CONST FLT_REGISTRATION FilterRegistration;
//函数声明
NTSTATUS InstanceSetup(PCFLT_RELATED_OBJECTS FltObjects, FLT_INSTANCE_SETUP_FLAGS Flags, DEVICE_TYPE VolumeDeviceType, FLT_FILESYSTEM_TYPE VolumeFilesystemType);
NTSTATUS
FilterUnload(
	_In_ FLT_FILTER_UNLOAD_FLAGS Flags
);
NTSTATUS
InstanceQueryTeardown(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
);
VOID DriverUnload(PDRIVER_OBJECT DriverObject);
VOID
CleanupVolumeContext(
	_In_ PFLT_CONTEXT Context,
	_In_ FLT_CONTEXT_TYPE ContextType
);
/********IRP_MJ_Write 的操作************/
FLT_PREOP_CALLBACK_STATUS
SfPreWrite(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
);
FLT_POSTOP_CALLBACK_STATUS
SfPostWrite(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_opt_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
);
/***********IRP_MJ_Read******************************/
FLT_PREOP_CALLBACK_STATUS
SfPreRead(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
);
FLT_POSTOP_CALLBACK_STATUS
SfPostRead(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_opt_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
);
/*volume 上下文*/
typedef struct _VOLUME_CONTEXT {
	UNICODE_STRING Name;
	ULONG SectorSize;
}VOLUME_CONTEXT, *PVOLUME_CONTEXT;
/*
文件过滤驱动的句柄，在卸载的时候 需要用到
*/
extern PFLT_FILTER  g_filterHandle;
/*
申请一块空间，在pre和post中进行参数的传递。
*/
extern NPAGED_LOOKASIDE_LIST g_ContextList;
typedef struct _PRE_2_POST_CONTEXT {
	PVOLUME_CONTEXT VolCtx;
	PVOID newBuffer;
	PMDL  newMDL;
}PRE_2_POST_CONTEXT, *PPRE_2_POST_CONTEXT;
