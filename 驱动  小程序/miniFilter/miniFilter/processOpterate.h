#include<ntifs.h>

/*
/通过的进程的ID，返回进程的名字
/调用格式一般为：
/
UNICODE_STRING ParentName;
ParentName.Length = 0;
ParentName.MaximumLength = 0;
ParentName.Buffer = NULL;

if (GetProcessImagePath(&ParentName, CreateInfo->ParentProcessId) == STATUS_BUFFER_OVERFLOW)
{
ParentName.Buffer = ExAllocatePoolWithTag(NonPagedPool, ParentName.Length,'me');
ParentName.MaximumLength = ParentName.Length;
GetProcessImagePath(&ParentName, CreateInfo->ParentProcessId);
}
..........
ExFreePool(ParentName.Buffer);

返回值：
	如果返回值为 STATUS_BUFFER_OVERFLOW。则说明，ProcessImagePath的buffer不足，
	重新申请新的buffer.
*/
NTSTATUS
GetProcessImagePath(
	OUT PUNICODE_STRING ProcessImagePath,
	IN   HANDLE    dwProcessId
);