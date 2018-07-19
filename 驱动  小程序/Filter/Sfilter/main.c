#pragma once
#include<ntifs.h>
#include<ntddk.h>
#include<Ntdddisk.h>
#include"sfFastio.h"
//
//  Holds pointer to the device object that represents this driver and is used
//  by external programs to access this driver.  This is also known as the
//  "control device object".
//
PDEVICE_OBJECT gSFilterControlDeviceObject;

//
//  Holds pointer to the driver object for this driver
//
PDRIVER_OBJECT gSFilterDriverObject = NULL;

//
//  Buffer size for local names on the stack
//
#define MAX_DEVNAME_LENGTH 64
//
//  This lock is used to synchronize our attaching to a given device object.
//  This lock fixes a race condition where we could accidently attach to the
//  same device object more then once.  This race condition only occurs if
//  a volume is being mounted at the same time as this filter is being loaded.
//  This problem will never occur if this filter is loaded at boot time before
//  any file systems are loaded.
//
//  This lock is used to atomically test if we are already attached to a given
//  device object and if not, do the attach.
//

FAST_MUTEX gSfilterAttachLock;


//
//  TAG identifying memory SFilter allocates
//

#define SFLT_POOL_TAG   'tlFS'


//
//  Macro to test if this is my device object
//

#define IS_MY_DEVICE_OBJECT(_devObj) \
    (((_devObj) != NULL) && \
     ((_devObj)->DriverObject == gSFilterDriverObject) && \
      ((_devObj)->DeviceExtension != NULL))// && \
	 // ((*(ULONG *)(_devObj)->DeviceExtension) == SFLT_POOL_TAG))


 //文件过滤系统驱动的设备扩展
typedef struct _SFILTER_DEVICE_EXTENSION {
	//绑定的文件系统设备（真实设备）
	PDEVICE_OBJECT AttachedToDeviceObject;
	//与文件系统设备相关的真实设备（磁盘）,这个在绑定时使用
	PDEVICE_OBJECT StorageStackDeviceObject;
	//如果绑定了一个卷，那么这是物理磁盘卷名;否则这是绑定的控制设备名
	UNICODE_STRING DeviceName;
	//用来保存名字字符串的缓冲区
	WCHAR DeviceNameBuffer[MAX_DEVNAME_LENGTH];
} SFILTER_DEVICE_EXTENSION, *PSFILTER_DEVICE_EXTENSION;
//磁盘文件系统、光盘（CD_ROM）和网络文件系统
#define IS_DESIRED_DEVICE_TYPE(_type) \
    (((_type) == FILE_DEVICE_DISK_FILE_SYSTEM) || \
     ((_type) == FILE_DEVICE_CD_ROM_FILE_SYSTEM) || \
     ((_type) == FILE_DEVICE_NETWORK_FILE_SYSTEM))




NTSTATUS SfPassThrough(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp
)
{
	PSFILTER_DEVICE_EXTENSION pevExt;
	pevExt = (PSFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(pevExt->AttachedToDeviceObject,Irp);
}

NTSTATUS sfAttachDeviceToDeviceStack(
	IN PDEVICE_OBJECT SourceDevice,
	IN PDEVICE_OBJECT TargetDevice,
	IN OUT PDEVICE_OBJECT *AttachedToDeviceObject
)
{
	//测试代码，测试这个函数是否可以运行在可页交换段
	PAGED_CODE();
	UNICODE_STRING usName;
	NTSTATUS status;
	RtlInitUnicodeString(&usName, L"IoAttachDeviceToDeviceStackSafe\n");
	status = IoAttachDeviceToDeviceStackSafe(SourceDevice, TargetDevice, AttachedToDeviceObject);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("IoAttachDeviceToDeviceStackSafe : 0x%x\n");
	}
	return status;
}



VOID SfGetObjectName(
	IN PVOID Object,
	IN OUT PUNICODE_STRING Name
)
{
	NTSTATUS status;
	CHAR nibuf[512];
	POBJECT_NAME_INFORMATION  nameInfo = (POBJECT_NAME_INFORMATION)nibuf;
	ULONG retLength;
	status = ObQueryNameString(Object, nameInfo, sizeof(nibuf), &retLength);
	Name->Length = 0;
	if (NT_SUCCESS(status))
	{
		RtlCopyUnicodeString(Name, &nameInfo->Name);
	}
	else {
		DbgPrint("ObQueryNameString : 0x%x \n", status);
	}
}
NTSTATUS
SfAttachToFileSystemDeivce(
	IN PDEVICE_OBJECT DeviceObject,
	IN PUNICODE_STRING DeviceName
)
{
	PDEVICE_OBJECT newDeviceObject;
	PSFILTER_DEVICE_EXTENSION devExt;
	UNICODE_STRING fsrecName;
	NTSTATUS status;
	UNICODE_STRING fsName;
	WCHAR tempNameBuffer[MAX_DEVNAME_LENGTH];
	PAGED_CODE();
	//检查设备类型
	if (!IS_DESIRED_DEVICE_TYPE(DeviceObject->DeviceType))
	{
		return STATUS_SUCCESS;
	}
	RtlInitEmptyUnicodeString(&fsName, tempNameBuffer, sizeof(tempNameBuffer));
	RtlInitUnicodeString(&fsrecName, L"\\FileSystem\\Fs_Rec");
	SfGetObjectName(DeviceObject->DriverObject, &fsName);
	//跳过文件识别器的绑定
	if (RtlCompareUnicodeString(&fsName, &fsrecName, TRUE) == 0)
	{
		return STATUS_SUCCESS;
	}
	//生成新的设备，准备绑定目标设备
	status = IoCreateDevice(
		gSFilterDriverObject,
		sizeof(SFILTER_DEVICE_EXTENSION),
		NULL,
		DeviceObject->DeviceType,
		0,
		FALSE,
		&newDeviceObject
	);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("IoCreateDevice : 0x%x \n", status);
	}
	//复制各种标志
	if (FlagOn(DeviceObject->Flags, DO_BUFFERED_IO))
		SetFlag(newDeviceObject->Flags, DO_BUFFERED_IO);
	if (FlagOn(DeviceObject->Flags, DO_DIRECT_IO))
		SetFlag(newDeviceObject->Flags, DO_DIRECT_IO);
	if (FlagOn(DeviceObject->Characteristics, FILE_DEVICE_SECURE_OPEN))
		SetFlag(newDeviceObject->Characteristics, FILE_DEVICE_SECURE_OPEN);

	devExt = (PSFILTER_DEVICE_EXTENSION)newDeviceObject->DeviceExtension;
	//使用上一节提供的函数进行绑定
	status = sfAttachDeviceToDeviceStack(newDeviceObject, DeviceObject, &devExt->AttachedToDeviceObject);
	RtlInitEmptyUnicodeString(&devExt->DeviceName, devExt->DeviceNameBuffer, sizeof(devExt->DeviceNameBuffer));
	RtlCopyUnicodeString(&devExt->DeviceName, DeviceName);
	ClearFlag(newDeviceObject->Flags, DO_DEVICE_INITIALIZING);
	//下面是不同版本的兼容性设计。当期望目标操作系统的版本大雨0x501时
	//windows 内核一定是EnumerateDeviceObjectList等函数。这时候可以枚举
	//所有的卷，诸葛绑定。如果期望的目标操作系统比这个小，那么这些函数根本不存在，我们无法绑定已经加载的卷
	//WINVER >= 0x0501 windows xp
	//status = SfEnumerateFileSystemVolumes(DevieObject,&faName);
	return status;
}


VOID
SfDetachFromFileSystemDevice(
	IN PDEVICE_OBJECT DeviceObject
)
{
	PDEVICE_OBJECT ourAttachedDevice;
	PSFILTER_DEVICE_EXTENSION devExt;

	PAGED_CODE();

	//DeviceObject->AttachedDevice
	ourAttachedDevice = DeviceObject->AttachedDevice;
	while (NULL!=ourAttachedDevice)
	{
		if (IS_MY_DEVICE_OBJECT(ourAttachedDevice))
		{
			devExt = ourAttachedDevice->DeviceExtension;
			IoDetachDevice(DeviceObject);
			IoDeleteDevice(DeviceObject);
			return;
		}
		DeviceObject = ourAttachedDevice;
		ourAttachedDevice = DeviceObject->AttachedDevice;
	}
}
//注册一个回调函数
VOID SfFsNotification(
	IN PDEVICE_OBJECT DeviceObject,
	IN BOOLEAN FsActive
)
{


	//FsActive 为TRUE 则表示文件系统的激活；如果为FALSE则表示文件系统的卸载
	PAGED_CODE();
	UNICODE_STRING name;
	WCHAR nameBuffer[MAX_DEVNAME_LENGTH];
	RtlInitEmptyUnicodeString(&name, nameBuffer, sizeof(nameBuffer));
	SfGetObjectName(DeviceObject, &name);
	DbgPrint("NAME : %wZ\n", name);
#if DBG
	_asm int 3;
#endif // DBG
	//到这里才是正题。如果是文件系统激活，那么绑定文件系统的控制设备
	//如果是注销，则解除绑定。
	if (FsActive)
	{
		SfAttachToFileSystemDeivce(DeviceObject, &name);
	}
	else {
		SfDetachFromFileSystemDevice(DeviceObject);
	}
}

//注册IRP完成，回掉函数
NTSTATUS SfFsControlCompletion(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp,
	IN PVOID Context
)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(Irp);
	KeSetEvent((PKEVENT)Context, IO_NO_INCREMENT, FALSE);
	return STATUS_MORE_PROCESSING_REQUIRED;
}
//文件系统识别器接触绑定
NTSTATUS SfFsControlLoadFileSystem(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp
)
{
	PSFILTER_DEVICE_EXTENSION devExt;
	NTSTATUS status;
	KEVENT waitEvent;
	KeInitializeEvent(&waitEvent, NotificationEvent, FALSE);
	IoCopyCurrentIrpStackLocationToNext(Irp);
	
	IoSetCompletionRoutine(
		Irp,
		SfFsControlCompletion,
		&waitEvent,
		TRUE,
		TRUE,
		TRUE
		);
	devExt = (PSFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	status = IoCallDriver(devExt->AttachedToDeviceObject, Irp);
	if (status == STATUS_PENDING)
		status = KeWaitForSingleObject(&waitEvent, Executive, KernelMode, FALSE, NULL);
	//删除系统识别器的过滤
	IoDetachDevice(DeviceObject);
	IoDeleteDevice(DeviceObject);
	status = Irp->IoStatus.Status;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}
NTSTATUS
SfIsShadowCopyVolume(
	IN PDEVICE_OBJECT StorageStackDeviceObject,
	OUT PBOOLEAN IsShadowCopy
)
{
	PAGED_CODE();
	*IsShadowCopy = FALSE;
	PIRP irp;
	KEVENT event;
	IO_STATUS_BLOCK iosb;
	NTSTATUS status;

	if (FILE_DEVICE_VIRTUAL_DISK != StorageStackDeviceObject->DeviceType) {

		return STATUS_SUCCESS;
	}
	
	KeInitializeEvent(&event, NotificationEvent, FALSE);
	irp = IoBuildDeviceIoControlRequest(IOCTL_DISK_IS_WRITABLE,
		StorageStackDeviceObject,
		NULL,
		0,
		NULL,
		0,
		FALSE,
		&event,
		&iosb);
	
	if (irp == NULL) {

		return STATUS_INSUFFICIENT_RESOURCES;
	}
	status = IoCallDriver(StorageStackDeviceObject, irp);
	if (status == STATUS_PENDING) {

	(VOID)KeWaitForSingleObject(&event,
		Executive,
		KernelMode,
		FALSE,
		NULL);

	status = iosb.Status;
	}
	if (STATUS_MEDIA_WRITE_PROTECTED == status) {
		*IsShadowCopy = TRUE;
		status = STATUS_SUCCESS;
	}
	return status;
}
BOOLEAN
SfIsAttachedToDeviceWXPAndLater(
	PDEVICE_OBJECT DeviceObject,
	PDEVICE_OBJECT *AttachedDeviceObject OPTIONAL
)
{
	PDEVICE_OBJECT currentDevObj;
	PDEVICE_OBJECT nextDevObj;
	PAGED_CODE();
	//
	//  Get the device object at the TOP of the attachment chain
	//
	//
	//RtlInitUnicodeString(&functionName, L"IoGetAttachedDeviceReference");
	//gSfDynamicFunctions.GetAttachedDeviceReference = MmGetSystemRoutineAddress(&functionName);
	//PSF_GET_ATTACHED_DEVICE_REFERENCE 
	//ASSERT(NULL != gSfDynamicFunctions.GetAttachedDeviceReference);
	currentDevObj = IoGetAttachedDeviceReference(DeviceObject);
	//
	//  Scan down the list to find our device object.
	//
	do {

		if (IS_MY_DEVICE_OBJECT(currentDevObj)) {
			//
			//  We have found that we are already attached.  If we are
			//  returning the device object, leave it referenced else remove
			//  the reference.
			//
			if (ARGUMENT_PRESENT(AttachedDeviceObject)) {
				*AttachedDeviceObject = currentDevObj;
			}
			else {
				ObDereferenceObject(currentDevObj);
			}
			return TRUE;
		}
		//
		//  Get the next attached object.  This puts a reference on 
		//  the device object.
		//
		//RtlInitUnicodeString( &functionName, L"IoGetLowerDeviceObject" );
		//gSfDynamicFunctions.GetLowerDeviceObject = MmGetSystemRoutineAddress(&functionName);
		//ASSERT(NULL != gSfDynamicFunctions.GetLowerDeviceObject);
		nextDevObj = IoGetLowerDeviceObject(currentDevObj);
		//
		//  Dereference our current device object, before
		//  moving to the next one.
		//
		ObDereferenceObject(currentDevObj);
		currentDevObj = nextDevObj;
	} while (NULL != currentDevObj);

	if (ARGUMENT_PRESENT(AttachedDeviceObject)) {

		*AttachedDeviceObject = NULL;
	}
	return FALSE;
}
NTSTATUS SfAttachToMountedDevice(
	IN  PDEVICE_OBJECT DeviceObject,
	IN PDEVICE_OBJECT SFilterDeviceObject
)
{
	PSFILTER_DEVICE_EXTENSION newDevExt = (PSFILTER_DEVICE_EXTENSION)SFilterDeviceObject->DeviceExtension;
	NTSTATUS status;
	ULONG i;
	PAGED_CODE();
	//设备标记的复制
	if (FlagOn(DeviceObject->Flags, DO_BUFFERED_IO))
		SetFlag(SFilterDeviceObject->Flags, DO_BUFFERED_IO);
	if (FlagOn(DeviceObject->Flags, DO_DIRECT_IO))
		SetFlag(SFilterDeviceObject->Flags, DO_DIRECT_IO);
	//循环尝试绑定，
	for (i = 0; i < 8; i++)
	{
		LARGE_INTEGER interval;
		status = sfAttachDeviceToDeviceStack(SFilterDeviceObject, DeviceObject, &newDevExt->AttachedToDeviceObject);
		if (!NT_SUCCESS(status))
		{
			DbgPrint("sfAttacheDeviceToDeviceStack faile : 0x%x\n",status);
			return status;
		}
		//延迟500ms后再继续
		interval.QuadPart = (500*1000);
		KeDelayExecutionThread(KernelMode, FALSE, &interval);
	}
	return status;
}
//绑定卷的实现
NTSTATUS SfFsControlMountVolumeComplete(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp,
	IN PDEVICE_OBJECT NewDeviceObject
)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	PVPB vpb;
	PSFILTER_DEVICE_EXTENSION newDevExt;
	PIO_STACK_LOCATION irpSp;
	PDEVICE_OBJECT attachedDeviceObject;
	NTSTATUS status;
	//获取Vpb
	newDevExt = (PSFILTER_DEVICE_EXTENSION)NewDeviceObject->DeviceExtension;
	vpb = newDevExt->StorageStackDeviceObject->Vpb;
	irpSp = IoGetCurrentIrpStackLocation(Irp);
	if (NT_SUCCESS(Irp->IoStatus.Status))
	{
		//获得一个互斥体
		ExAcquireFastMutex(&gSfilterAttachLock);
		//判断是否绑定过
		
		if (!SfIsAttachedToDeviceWXPAndLater(vpb->DeviceObject, &attachedDeviceObject))
		{
			//绑定
			status = SfAttachToMountedDevice(vpb->DeviceObject,NewDeviceObject);
			if (!NT_SUCCESS(status))
			{
				DbgPrint("SfAttachToMountedDevice fail : 0x%x\n", status);
			}
		}
		else {
			//已经绑定过了

			IoDeleteDevice(NewDeviceObject);
			ObDereferenceObject((PVOID)attachedDeviceObject);
		}
		ExReleaseFastMutex(&gSfilterAttachLock);
	}
	else {
		IoDeleteDevice(NewDeviceObject);
	}
	//完成请求
	status = Irp->IoStatus.Status;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}
//挂载卷
NTSTATUS SfFsControlMountVolume(
	PDEVICE_OBJECT DeviceObject,
	PIRP Irp
)
{
	PAGED_CODE();
	PSFILTER_DEVICE_EXTENSION devExt = DeviceObject->DeviceExtension;
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
	PDEVICE_OBJECT newDeviceObject;
	PDEVICE_OBJECT storageStackDeviceObject;
	NTSTATUS status;
	BOOLEAN isShadowCopyVoulume;
	PSFILTER_DEVICE_EXTENSION newDevExt;
	//保存Vpb->RealDevice
	storageStackDeviceObject = irpSp->Parameters.MountVolume.Vpb->RealDevice;
	//判断是否是卷影
	status = SfIsShadowCopyVolume(storageStackDeviceObject, &isShadowCopyVoulume);
	//不绑定卷影
	if (NT_SUCCESS(status) && isShadowCopyVoulume)
	{
		IoSkipCurrentIrpStackLocation(Irp);
		return IoCallDriver(devExt->AttachedToDeviceObject, Irp);
	}
	//预先生成过滤设备
	status = IoCreateDevice(
		gSFilterDriverObject,
		sizeof(SFILTER_DEVICE_EXTENSION),
		NULL,
		DeviceObject->DeviceType,
		0,
		FALSE,
		&newDeviceObject
	);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("IoCreateDevie faile: 0x%x \n", status);
		Irp->IoStatus.Information = 0;
		Irp->IoStatus.Status = status;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return status;
	}
	newDevExt = (PSFILTER_DEVICE_EXTENSION)newDeviceObject->DeviceExtension;
	newDevExt->StorageStackDeviceObject = storageStackDeviceObject;
	RtlInitEmptyUnicodeString(&newDevExt->DeviceName, newDevExt->DeviceNameBuffer, sizeof(newDevExt->DeviceNameBuffer));
	SfGetObjectName(storageStackDeviceObject, &newDevExt->DeviceName);

	if (1)
	{
		KEVENT waitEvent;
		KeInitializeEvent(&waitEvent, NotificationEvent, FALSE);
		IoCopyCurrentIrpStackLocationToNext(Irp);
		IoSetCompletionRoutine(
			Irp,SfFsControlCompletion,
			&waitEvent,
			TRUE, TRUE, TRUE);
		status = IoCallDriver(devExt->AttachedToDeviceObject, Irp);
		if (STATUS_PENDING == status)
			status = KeWaitForSingleObject(&waitEvent, Executive, KernelMode, FALSE, NULL);
	}
	//到这里请求完成，调用函数绑定卷
	status = SfFsControlMountVolumeComplete(DeviceObject, Irp, newDeviceObject);
	return status;
}
//
NTSTATUS SfFsControl(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp
)
{
	//NTSTATUS status;
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
	PAGED_CODE();
	switch (irpSp->MinorFunction)
	{
		case IRP_MN_MOUNT_VOLUME:  //说明一个卷被挂载
			return SfFsControlMountVolume(DeviceObject, Irp);
		case IRP_MN_LOAD_FILE_SYSTEM: 
			SfFsControlLoadFileSystem(DeviceObject, Irp);
			break;
		case IRP_MN_USER_FS_REQUEST:
		{
			switch (irpSp->Parameters.FileSystemControl.FsControlCode)
			{
				case FSCTL_DISMOUNT_VOLUME:
				{
					//PSFILTER_DEVICE_EXTENSION devExt = (PSFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension;  //执行卸载操作
					break;
				}
			}
			break;
		}
	}
	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(((PSFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->AttachedToDeviceObject, Irp);
}

NTSTATUS DriverEntry(
	IN PDRIVER_OBJECT DriverObject,
	IN PUNICODE_STRING RegistryPath
)
{
	UNREFERENCED_PARAMETER(RegistryPath);
	NTSTATUS status;
	ULONG i;
	//定义一个Unicode字符串
	UNICODE_STRING nameString;
	RtlInitUnicodeString(&nameString, L"\\FileSystem\\Filters\\SFilter");
	//生成控制设备
	status = IoCreateDevice(
		DriverObject,
		0,
		&nameString,
		FILE_DEVICE_DISK_FILE_SYSTEM,
		FILE_DEVICE_SECURE_OPEN,
		FALSE,
		&gSFilterControlDeviceObject
		);
	if (status == STATUS_OBJECT_PATH_NOT_FOUND)
	{
		//这是因为一些低版本的操作系统没有\FileSystem\Filters\这个目录
		//如果没有，则改变位置，生成到\FileSystem下
		RtlInitUnicodeString(&nameString, L"\\FileSystem\\SFilterCDO");
		status = IoCreateDevice(
			DriverObject,
			0, &nameString,
			FILE_DEVICE_DISK_FILE_SYSTEM,
			FILE_DEVICE_SECURE_OPEN,
			FALSE,
			&gSFilterControlDeviceObject
		);
	}
	if (!NT_SUCCESS(status))
	{
		DbgPrint("IoCreateDevice : 0x%x", status);
	}

	//-----------------普通分发函数-------------------
	for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
		DriverObject->MajorFunction[i] = SfPassThrough;


	DriverObject->MajorFunction[IRP_MJ_FILE_SYSTEM_CONTROL] = SfFsControl;

	//--------------------------------------快速分发函数----------------------
	PFAST_IO_DISPATCH fastIoDispatch;
	fastIoDispatch = ExAllocatePoolWithTag(
		NonPagedPool, sizeof(FAST_IO_DISPATCH), 'g'
	);
	if (!fastIoDispatch)
	{
		//分配失败的情况，删除先生成的控制设备
		IoDeleteDevice(gSFilterControlDeviceObject);
		DbgPrint("ExAllocatePoolWithTag  error \n");
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	//内存清零
	RtlZeroMemory(fastIoDispatch, sizeof(FAST_IO_DISPATCH));
	fastIoDispatch->SizeOfFastIoDispatch = sizeof(FAST_IO_DISPATCH);
	fastIoDispatch->FastIoCheckIfPossible = SfFastIoCheckIfPossible;
	fastIoDispatch->FastIoRead = SfFastIoRead;
	fastIoDispatch->FastIoWrite = SfFastIoWrite;
	fastIoDispatch->FastIoQueryBasicInfo = SfFastIoQueryBasicInfo;
	fastIoDispatch->FastIoQueryStandardInfo = SfFastIoQueryStandardInfo;
	fastIoDispatch->FastIoLock = SfFastIoLock;
	fastIoDispatch->FastIoUnlockSingle = SfFastIoUnlockSingle;
	fastIoDispatch->FastIoUnlockAll = SfFastIoUnlockAll;
	fastIoDispatch->FastIoUnlockAllByKey = SfFastIoUnlockAllByKey;
	fastIoDispatch->FastIoDeviceControl = SfFastIoDeviceControl;
	fastIoDispatch->FastIoDetachDevice = SfFastIoDetachDevice;
	fastIoDispatch->FastIoQueryNetworkOpenInfo = SfFastIoQueryNetworkOpenInfo;
	fastIoDispatch->MdlRead = SfFastIoMdlRead;
	fastIoDispatch->MdlReadComplete = SfFastIoMdlReadComplete;
	fastIoDispatch->PrepareMdlWrite = SfFastIoPrepareMdlWrite;
	fastIoDispatch->MdlWriteComplete = SfFastIoMdlWriteComplete;
	fastIoDispatch->FastIoReadCompressed = SfFastIoReadCompressed;
	fastIoDispatch->FastIoWriteCompressed = SfFastIoWriteCompressed;
	fastIoDispatch->MdlReadCompleteCompressed = SfFastIoMdlReadCompleteCompressed;
	fastIoDispatch->MdlWriteCompleteCompressed = SfFastIoMdlWriteCompleteCompressed;
	fastIoDispatch->FastIoQueryOpen = SfFastIoQueryOpen;
	//最后制定DriverObject
	DriverObject->FastIoDispatch = fastIoDispatch;
	//-----------------------------------------------------------------------------------


	//--------------注册文件变动回掉函数---再windows xp之后，会遍历已经存在的文件系统--------------
	status = IoRegisterFsRegistrationChange(DriverObject, SfFsNotification);
	if (!NT_SUCCESS(status))
	{
		//万一失败了，前面分配的FastIo分发函数就没用了，直接释放掉
		DbgPrint("IoRegisterFsRegistrationChange : 0x%x\n", status);
		DriverObject->FastIoDispatch = NULL;
		ExFreePool(fastIoDispatch);
		//前面生成的控制设备也无意义了，删除推出
		IoDeleteDevice(gSFilterControlDeviceObject);
		return status;
	}
	return status;
}