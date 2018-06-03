#include<ntddk.h>

VOID ProcessPre (
	IN HANDLE ParentId,
	IN HANDLE ProcessId,
	IN BOOLEAN Create
	)
{
	UNREFERENCED_PARAMETER(ParentId);
	UNREFERENCED_PARAMETER(ProcessId);
	if (Create == TRUE)  //创建进程
	{
		KdPrint(("create process\n"));
	}
}
VOID Unload(IN PDRIVER_OBJECT pDriverObject)
{
	UNREFERENCED_PARAMETER(pDriverObject);
	KdPrint(("unload driver \n"));
	PsSetCreateProcessNotifyRoutine(ProcessPre,TRUE);
}
NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObject,IN PUNICODE_STRING pRegistryPath)
{
	UNREFERENCED_PARAMETER(pDriverObject);
	UNREFERENCED_PARAMETER(pRegistryPath);

	NTSTATUS re = STATUS_SUCCESS;
	pDriverObject->DriverUnload = Unload;
	re = PsSetCreateProcessNotifyRoutine(ProcessPre,FALSE);
	if (!NT_SUCCESS(re))
	{
		KdPrint(("PsSetCreateProcessNotifyRoutine is error \n"));
	}
	KdPrint(("hello world\n"));
	return re;
}