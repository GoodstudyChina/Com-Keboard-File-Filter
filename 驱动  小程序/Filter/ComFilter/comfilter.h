#pragma once
#include<ntddk.h>
#include<ntdef.h>
#include<ntstrsafe.h>





void ccpAttachAllComs(PDRIVER_OBJECT driver);
NTSTATUS ccpDispatch(PDEVICE_OBJECT device, PIRP irp);
void ccpUnload(PDRIVER_OBJECT drv);