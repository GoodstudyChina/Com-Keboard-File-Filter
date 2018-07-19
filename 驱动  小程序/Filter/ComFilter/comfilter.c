#pragma once 
#include"comfilter.h"


//计算机上最多只有32个串口，这是笔者的假设
#define CCP_MAX_COM_ID 32
//保存所有过滤设备指针
static PDEVICE_OBJECT s_fltobj[CCP_MAX_COM_ID] = { 0 };
//保存所有真实设备指针
static PDEVICE_OBJECT s_nextobj[CCP_MAX_COM_ID] = { 0 };


//绑定一个端口信息
NTSTATUS ccpAttachDevice(
	PDRIVER_OBJECT driver,
	PDEVICE_OBJECT oldobj,
	PDEVICE_OBJECT *fltobj,
	PDEVICE_OBJECT *next
)
{
	NTSTATUS status = STATUS_SUCCESS;
	PDEVICE_OBJECT topdev = NULL;

	//生成设备，然后绑定
	status = IoCreateDevice(
		driver,
		0,
		NULL,
		oldobj->DeviceType,
		0,
		FALSE,
		fltobj);
	if (status != STATUS_SUCCESS)
		return status;

	//拷贝重要标志
	if (oldobj->Flags & DO_BUFFERED_IO)
		(*fltobj)->Flags |= DO_BUFFERED_IO;
	if (oldobj->Flags & DO_DIRECT_IO)
		(*fltobj)->Flags |= DO_DIRECT_IO;
	if (oldobj->Characteristics & FILE_DEVICE_SECURE_OPEN)
		(*fltobj)->Characteristics |= FILE_DEVICE_SECURE_OPEN;
	(*fltobj)->Flags |= DO_POWER_PAGABLE;

	//将一个设备绑定到另一个设备上
	topdev = IoAttachDeviceToDeviceStack(*fltobj, oldobj);
	if (topdev == NULL)
	{
		//如果绑定失败了，销毁设备，返回错误。
		IoDeleteDevice(*fltobj);
		status = STATUS_UNSUCCESSFUL;
		return status;
	}
	*next = topdev;

	//设置这个设备已经启动
	(*fltobj)->Flags = (*fltobj)->Flags & DO_DEVICE_INITIALIZING;

	return status;
}

//打开一个端口
PDEVICE_OBJECT ccpOpenCom(ULONG id, NTSTATUS *status)
{
	//外面输入的是串口的ID,这里会改写成字符串的形式
	UNICODE_STRING name_str;
	static WCHAR name[32] = { 0 };
	PFILE_OBJECT fileobj = NULL;
	PDEVICE_OBJECT devobj = NULL;
	//NTSTATUS status;
	//根据ID转换成串口的名字
	memset(name, 0, sizeof(WCHAR) * 32);
	RtlStringCchPrintfW(name, 32, L"\\Device\\Serial%d", id);
	RtlInitUnicodeString(&name_str, name);
	//打开设备对象
	*status = IoGetDeviceObjectPointer(
		&name_str,
		FILE_ALL_ACCESS,
		&fileobj, &devobj
	);
	//如果打开成功了，记得一定要把文件对象解除引用
	//总之，这一句不要忽视
	if (*status == STATUS_SUCCESS)
		ObReferenceObject(fileobj);
	//返回设备对象
	return devobj;
}
void ccpAttachAllComs(PDRIVER_OBJECT driver)
{
	ULONG i;
	PDEVICE_OBJECT com_ob = NULL;
	NTSTATUS status;
	for (i = 0; i < CCP_MAX_COM_ID; i++)
	{
		//获得object引用
		com_ob = ccpOpenCom(i, &status);
		if (com_ob == NULL)
			continue;
		//在这里绑定，并不管绑定是否成功
		ccpAttachDevice(driver, com_ob, &s_fltobj[i], &s_nextobj[i]);
	}
}

NTSTATUS ccpDispatch(PDEVICE_OBJECT device, PIRP irp)
{
	PIO_STACK_LOCATION irpsp = IoGetCurrentIrpStackLocation(irp);
	ULONG i, j;

	//首先要知道发送给了哪个设备。设备最多一共有CCP_MAX_COM_ID
	//个，是前面的代码保存好的，都在s_fltobj中
	for (i = 0; i < CCP_MAX_COM_ID; i++)
	{

		if (s_fltobj[i] == device)
		{
			//所有电源操作，全部直接放过
			if (irpsp->MajorFunction == IRP_MJ_POWER)
			{
				//直接发送，然后返回说已经被处理了
				PoStartNextPowerIrp(irp);
				IoSkipCurrentIrpStackLocation(irp);
				return PoCallDriver(s_nextobj[i], irp);
			}
			//此外我们只过滤写请求。写请求，获得缓冲区及其长度
			//然后打印
			if (irpsp->MajorFunction == IRP_MJ_WRITE)
			{
				//如果是写，先获得长度
				ULONG len = irpsp->Parameters.Write.Length;
				//然后获得缓冲区
				PUCHAR buf = NULL;
				if (irp->MdlAddress != NULL)
					buf = (PUCHAR)MmGetSystemAddressForMdlSafe(irp->MdlAddress, NormalPagePriority);
				else
					buf = (PUCHAR)irp->UserBuffer;
				if (buf == NULL)
					buf = (PUCHAR)irp->AssociatedIrp.SystemBuffer;
				//打印内容
				for (j = 0; j < len; ++j)
				{
					DbgPrint("comcap:Send Data : %2x \r\n", buf[j]);
				}
			}
			//这些请求直接下发执行即可，我们并不禁止或者改变它
			IoSkipCurrentIrpStackLocation(irp);
			return IoCallDriver(s_nextobj[i], irp);
		}
	}
	//如果根本就不在被绑定的设备中，那是有问题的，直接返回参数错误
	irp->IoStatus.Information = 0;
	irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}


#define DELAY_ONE_MICROSECOND (-10)
#define DELAY_ONE_MILLISECOND (DELAY_ONE_MICROSECOND*1000)
#define DELAY_ONE_SECOND  (DELAY_ONE_MILLISECOND*1000)

void ccpUnload(PDRIVER_OBJECT drv)
{
	UNREFERENCED_PARAMETER(drv);
	ULONG i;
	LARGE_INTEGER interval;
	//首先解除绑定
	for (i = 0; i < CCP_MAX_COM_ID; i++)
	{
		if (s_nextobj[i] != NULL)
			IoDetachDevice(s_nextobj[i]);
		//睡眠5秒。等待所有IRP处理结束
		interval.QuadPart = (5 * 1000 * DELAY_ONE_MILLISECOND);
		KeDelayExecutionThread(KernelMode, FALSE, &interval);
		//删除这些设备
		for (i = 0; i < CCP_MAX_COM_ID; i++)
		{
			if (s_fltobj[i] != NULL)
				IoDeleteDevice(s_fltobj[i]);
		}
	}
}