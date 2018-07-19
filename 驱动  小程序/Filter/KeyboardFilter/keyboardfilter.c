#pragma once 
#include"keyboardfilter.h"
typedef struct _C2P_DEV_EXT
{
	//这个结构的大小
	ULONG NodeSize;
	//过滤设备对象
	PDEVICE_OBJECT pFilterDeviceObject;
	//同时调用时的保护锁
	KSPIN_LOCK IoRequestsSpinLock;
	//进程间同步处理
	KEVENT IoInProgressEvent;
	//绑定的设备对象
	PDEVICE_OBJECT TargetDeviceObject;
	//绑定前底层设备对象
	PDEVICE_OBJECT LowerDeviceObject;
	//保存等待的Irp  代理irp
	PIRP proxyIrp;
}C2P_DEV_EXT, *PC2P_DEV_EXT;

//这个函数是事实存在的，只是文档中没有公开
//声明一下就可以直接使用了
NTSTATUS ObReferenceObjectByName(
	PUNICODE_STRING ObjectName,
	ULONG Attributes,
	PACCESS_STATE AccessState,
	ACCESS_MASK DesiredAccess,
	POBJECT_TYPE ObjectType,
	KPROCESSOR_MODE AccessMode,
	PVOID ParseContext,
	PVOID *Object
);
#define KBD_DRIVER_NAME L"\\Driver\\kbdclass"
//IoDriverObjectType 实际上是一个全局变量，但是头文件中没有
//只要声明之后就可以使用了。本章后面的代码中有多次用到
extern POBJECT_TYPE *IoDriverObjectType;
//这个函数改造，能打开驱动对象KbdClass ,然后绑定它下面的所有设备
NTSTATUS keyAttachDevice(
	IN PDRIVER_OBJECT DriverObject,
	IN PUNICODE_STRING RegistryPath
)
{
	UNREFERENCED_PARAMETER(RegistryPath);
	NTSTATUS status;
	UNICODE_STRING unNtNameString;
	PDRIVER_OBJECT KbdDriverObject = NULL;
	PDEVICE_OBJECT pTargetDeviceObject = NULL;
	PDEVICE_OBJECT pKeyDeviceObject = NULL;
	PDEVICE_OBJECT pLowerDeviceObject = NULL;
	PC2P_DEV_EXT devExt;
	KdPrint(("Attach\n"));
	//初始化一个字符串，就是KdbClass驱动的名字
	RtlInitUnicodeString(&unNtNameString, KBD_DRIVER_NAME);
	

	//请参照前面打开设备对象的例子，只是这里打开的是驱动对象
	status = ObReferenceObjectByName(
		&unNtNameString,
		OBJ_CASE_INSENSITIVE,
		NULL,
		0,
		*IoDriverObjectType,
		KernelMode,
		NULL,
		(PVOID*)&KbdDriverObject
	);
	//如果失败了直接就返回
	if (!NT_SUCCESS(status))
	{
		DbgPrint("ObReferenceObjectByName: 0x%x", status);
		return status;
	}
	else {
		//调用obreferenceObjectName会导致对驱动对象的引用计数增加
		//必须相应地调用解析引用ObDereferenceObject
		ObDereferenceObject(KbdDriverObject);
	}
	//这是设备链的中的第一个设备
	pTargetDeviceObject = KbdDriverObject->DeviceObject;
	//开始遍历这个设备链
	while (pTargetDeviceObject)
	{
		//生成一个过滤设备
		status = IoCreateDevice(
			DriverObject,
			sizeof(C2P_DEV_EXT),
			NULL,
			pTargetDeviceObject->DeviceType,
			pTargetDeviceObject->Characteristics,
			FALSE,
			&pKeyDeviceObject
		);
		if (!NT_SUCCESS(status))
		{
			DbgPrint("IoCreateDevice : 0x%x \n", status);
			return status;
		}
		//绑定,pLowerDeviceObjcect 是绑定之后得到的下一个设备
		//也就是前面常常说的所谓真实设备
		status = IoAttachDeviceToDeviceStackSafe(pKeyDeviceObject, pTargetDeviceObject,&pLowerDeviceObject);
		//如果绑定失败，放弃操作退出
		if (!NT_SUCCESS(status))
		{
			DbgPrint("IoAttachDeviceToDeviceStachSafe : 0x%x\n", status);
			IoDeleteDevice(pKeyDeviceObject);
			pKeyDeviceObject = NULL;
			return status;
		}
		//设备扩展，下面要详细讲述设备扩展的应用
		devExt = (PC2P_DEV_EXT)(pKeyDeviceObject->DeviceExtension);
		memset(devExt, 0, sizeof(C2P_DEV_EXT));
		devExt->NodeSize = sizeof(C2P_DEV_EXT);
		devExt->pFilterDeviceObject = pKeyDeviceObject;
		devExt->TargetDeviceObject = pTargetDeviceObject;
		devExt->LowerDeviceObject = pLowerDeviceObject;
		//设备扩展
		pKeyDeviceObject->DeviceType = pLowerDeviceObject->DeviceType;
		pKeyDeviceObject->Characteristics = pLowerDeviceObject->Characteristics;
		pKeyDeviceObject->StackSize = pLowerDeviceObject->StackSize + 1;
		pKeyDeviceObject->Flags |= pLowerDeviceObject->Flags&(DO_BUFFERED_IO | DO_DIRECT_IO | DO_POWER_PAGABLE);
		//移动到下一个设备，继续遍历
		pTargetDeviceObject = pTargetDeviceObject->NextDevice;
	}
	if (NT_SUCCESS(status))
	{
		DbgPrint("Attach Device success \n");
	}
	return status;
}




NTSTATUS keyDisaptchGeneral(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp
)
{
	//其他的分发函数，直接Skip，然后用IoCallDriver把IRP发送到真实设备的设备对象
	KdPrint(("keyDisaptchGeral\n"));
	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(((PC2P_DEV_EXT)(DeviceObject->DeviceExtension))->LowerDeviceObject, Irp);
}
//flags for keyboard status 
#define S_SHIFT 1
#define S_CAPS  2
#define S_NUM   4
//这是一个标记，用来保存当前键盘的状态。其中有3个位，分别表示
//Caps Lock 键、Num Lock 键和Shift 键是否按下了
static int kb_status = S_NUM;
unsigned char asciiTbl[] = {
	0x00, 0x1B, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x2D, 0x3D, 0x08, 0x09, //normal  
	0x71, 0x77, 0x65, 0x72, 0x74, 0x79, 0x75, 0x69, 0x6F, 0x70, 0x5B, 0x5D, 0x0D, 0x00, 0x61, 0x73,
	0x64, 0x66, 0x67, 0x68, 0x6A, 0x6B, 0x6C, 0x3B, 0x27, 0x60, 0x00, 0x5C, 0x7A, 0x78, 0x63, 0x76,
	0x62, 0x6E, 0x6D, 0x2C, 0x2E, 0x2F, 0x00, 0x2A, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x37, 0x38, 0x39, 0x2D, 0x34, 0x35, 0x36, 0x2B, 0x31,
	0x32, 0x33, 0x30, 0x2E,
	0x00, 0x1B, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x2D, 0x3D, 0x08, 0x09, //caps  
	0x51, 0x57, 0x45, 0x52, 0x54, 0x59, 0x55, 0x49, 0x4F, 0x50, 0x5B, 0x5D, 0x0D, 0x00, 0x41, 0x53,
	0x44, 0x46, 0x47, 0x48, 0x4A, 0x4B, 0x4C, 0x3B, 0x27, 0x60, 0x00, 0x5C, 0x5A, 0x58, 0x43, 0x56,
	0x42, 0x4E, 0x4D, 0x2C, 0x2E, 0x2F, 0x00, 0x2A, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x37, 0x38, 0x39, 0x2D, 0x34, 0x35, 0x36, 0x2B, 0x31,
	0x32, 0x33, 0x30, 0x2E,
	0x00, 0x1B, 0x21, 0x40, 0x23, 0x24, 0x25, 0x5E, 0x26, 0x2A, 0x28, 0x29, 0x5F, 0x2B, 0x08, 0x09, //shift  
	0x51, 0x57, 0x45, 0x52, 0x54, 0x59, 0x55, 0x49, 0x4F, 0x50, 0x7B, 0x7D, 0x0D, 0x00, 0x41, 0x53,
	0x44, 0x46, 0x47, 0x48, 0x4A, 0x4B, 0x4C, 0x3A, 0x22, 0x7E, 0x00, 0x7C, 0x5A, 0x58, 0x43, 0x56,
	0x42, 0x4E, 0x4D, 0x3C, 0x3E, 0x3F, 0x00, 0x2A, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x37, 0x38, 0x39, 0x2D, 0x34, 0x35, 0x36, 0x2B, 0x31,
	0x32, 0x33, 0x30, 0x2E,
	0x00, 0x1B, 0x21, 0x40, 0x23, 0x24, 0x25, 0x5E, 0x26, 0x2A, 0x28, 0x29, 0x5F, 0x2B, 0x08, 0x09, //caps + shift  
	0x71, 0x77, 0x65, 0x72, 0x74, 0x79, 0x75, 0x69, 0x6F, 0x70, 0x7B, 0x7D, 0x0D, 0x00, 0x61, 0x73,
	0x64, 0x66, 0x67, 0x68, 0x6A, 0x6B, 0x6C, 0x3A, 0x22, 0x7E, 0x00, 0x7C, 0x7A, 0x78, 0x63, 0x76,
	0x62, 0x6E, 0x6D, 0x3C, 0x3E, 0x3F, 0x00, 0x2A, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x37, 0x38, 0x39, 0x2D, 0x34, 0x35, 0x36, 0x2B, 0x31,
	0x32, 0x33, 0x30, 0x2E
};
VOID print_keystroke(UCHAR sch)
{
	UCHAR ch = 0;
	int off = 0;


	if ((sch & 0x80) == 0)  //如果是按下
	{
		//如果按下了字母或者数字等可见字符
		if ((sch < 0x47) || ((sch >= 0x47 && sch < 0x54) && (kb_status&S_NUM)))
		{
			//最终得到哪一个字符由Caps Lock 和Num Lock 及Shift这几个键的状态来决定
			if (((kb_status&S_CAPS) == S_CAPS) && ((kb_status&S_SHIFT) != S_SHIFT))
				off = 0x54;
			if (((kb_status&S_CAPS) != S_CAPS) && ((kb_status&S_SHIFT) == S_SHIFT))
				off = 0xA8;
			if (((kb_status&S_CAPS) == S_CAPS) && ((kb_status&S_SHIFT) == S_SHIFT))
				off = 0xFC;
			ch = asciiTbl[off + sch];
		}
		switch (sch)
		{
			//Caps Lock 键和Num Lock 键类似，都是“按下两次”等于没按过一样的“反复键”
			//所以这里用异或来设置标志。也就是说，按一次起作用，再按一次就不起作用了
		case 0x3A:
			kb_status ^= S_CAPS;
			break;
		case 0x2A:
		case 0x36:
			kb_status |= S_SHIFT;
			break;
			//Num Lock 键盘
		case 0x45:
			kb_status ^= S_NUM;
			break;
		default:
			break;
		}
	}
	else {
		if (sch == 0xAA || sch == 0xB6)
			kb_status &= S_SHIFT;
	}
	if (ch >= 0x20 && ch < 0x7F)
	{
		DbgPrint("the code is : %C\n", ch);
	}
}
//这是一个IRP完成回调函数的原型
NTSTATUS keyReadComplete(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp,
	IN PVOID context
)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(context);
	PKEYBOARD_INPUT_DATA KeyData;
	ULONG numKeys;
	size_t i;
	PC2P_DEV_EXT devExt;
	if (NT_SUCCESS(Irp->IoStatus.Status))
	{
		//获取读请求完成后的输出缓冲区
		KeyData =(PKEYBOARD_INPUT_DATA)Irp->AssociatedIrp.SystemBuffer;
		//获取这个缓冲区的长度
		numKeys = (Irp->IoStatus.Information) / sizeof(KEYBOARD_INPUT_DATA);
		for (i = 0; i < numKeys; i++)
		{
			DbgPrint(("%s\n", KeyData->Flags ? "UP" : "DOWN"));
			if (!KeyData->Flags)
				print_keystroke((UCHAR)KeyData->MakeCode);
		}
	}
	devExt = (PC2P_DEV_EXT)DeviceObject->DeviceExtension;
	devExt->proxyIrp = NULL;
	if (Irp->PendingReturned)
		IoMarkIrpPending(Irp);
	return Irp->IoStatus.Status;
}
NTSTATUS keyDisaptchRead(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp
)
{
	NTSTATUS status = STATUS_SUCCESS;
	PC2P_DEV_EXT devExt;
	PIO_STACK_LOCATION currentIrpStack;
	//判断是否到达了irp栈的最低端
	if (Irp->CurrentLocation == 1)
	{
		status = STATUS_INVALID_DEVICE_REQUEST;
		Irp->IoStatus.Status = status;
		Irp->IoStatus.Information = 0;
		return status;
	}
	devExt = (PC2P_DEV_EXT)DeviceObject->DeviceExtension;
	devExt->proxyIrp = Irp;
	//剩下的任务是等待读请求完成
	currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
	IoCopyCurrentIrpStackLocationToNext(Irp);
	IoSetCompletionRoutine(Irp, keyReadComplete, DeviceObject, TRUE, TRUE, TRUE);
	return IoCallDriver(devExt->LowerDeviceObject, Irp);
}

NTSTATUS keyPower(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp
)
{
	PC2P_DEV_EXT devExt;
	devExt = (PC2P_DEV_EXT)(DeviceObject->DeviceExtension);
	PoStartNextPowerIrp(Irp);
	IoSkipCurrentIrpStackLocation(Irp);
	return PoCallDriver(devExt->LowerDeviceObject, Irp);
}

NTSTATUS keyPnp(IN PDEVICE_OBJECT DevieObject, IN PIRP Irp)
{
	PC2P_DEV_EXT devExt;
	NTSTATUS status = STATUS_SUCCESS;
	PIO_STACK_LOCATION irpStack;

	devExt = (PC2P_DEV_EXT)DevieObject->DeviceExtension;
	irpStack = IoGetCurrentIrpStackLocation(Irp);
	switch (irpStack->MinorFunction)
	{
		//键盘拔掉之后
	case IRP_MN_REMOVE_DEVICE:
		KdPrint(("IRP_MN_REMOVE_DEVICE\n"));
		//首先把请求下发下去
		IoSkipCurrentIrpStackLocation(Irp);
		IoCallDriver(devExt->LowerDeviceObject, Irp);
		//然后解除绑定
		IoDetachDevice(devExt->LowerDeviceObject);
		//删除我们自己生成的虚拟设备
		IoDeleteDevice(DevieObject);
		status = STATUS_SUCCESS;
		break;
	default:
		//其他的IRP，全部直接下发
		IoSkipCurrentIrpStackLocation(Irp);
		status = IoCallDriver(devExt->LowerDeviceObject, Irp);
		break;
	}
	return status;
}
BOOLEAN CancelIrp(PIRP pIrp)
{
	if (pIrp == NULL)
	{
		DbgPrint("取消irp错误.../n");
		return FALSE;
	}
	if (pIrp->Cancel || pIrp->CancelRoutine == NULL)
	{
		DbgPrint("取消irp错误.../n");
		return FALSE;
	}
	if (FALSE == IoCancelIrp(pIrp))
	{
		DbgPrint("IoCancelIrp to irp错误.../n");
		return FALSE;
	}

	//取消后重设此例为空  
	IoSetCancelRoutine(pIrp, NULL);
	return TRUE;
}

VOID  keyUnload(
	IN PDRIVER_OBJECT DriverObject
)
{
	UNREFERENCED_PARAMETER(DriverObject);
	KdPrint(("DriverEntry unloading ....\n"));
	PDEVICE_OBJECT DeviceObject;
	//PDEVICE_OBJECT OldDeviceObject;
	PRKTHREAD CurrentThread;
	CurrentThread = KeGetCurrentThread();
	//把当前线程设置为低实时模式，以便让它的运行尽量少影响其他程序
	KeSetPriorityThread(CurrentThread, LOW_REALTIME_PRIORITY);
	//遍历所有设备并一律解除绑定
	DeviceObject = DriverObject->DeviceObject;

	while (DeviceObject)
	{
		//解除绑定并删除所有的设备
		//c2pDetach(DeviceObject);
		PC2P_DEV_EXT devExt;
		devExt = (PC2P_DEV_EXT)DeviceObject->DeviceExtension;
		if (devExt->LowerDeviceObject != NULL)
			IoDetachDevice(devExt->LowerDeviceObject);
		devExt->LowerDeviceObject = NULL;
		if (devExt->pFilterDeviceObject != NULL)
			IoDeleteDevice(devExt->pFilterDeviceObject);
		devExt->pFilterDeviceObject = NULL;
		if (devExt->proxyIrp != NULL)
		{
			//取消当前IRP
			CancelIrp(devExt->proxyIrp);
		}
		DeviceObject = DeviceObject->NextDevice;
	}
	KdPrint(("DriverEntry unlLoad OK!\n"));
	return;
}