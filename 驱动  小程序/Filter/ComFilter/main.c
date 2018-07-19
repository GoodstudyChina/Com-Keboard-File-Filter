#pragma once
#include"comfilter.h"


NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING reg_path)
{
	UNREFERENCED_PARAMETER(reg_path);
	size_t i;

	DbgPrint("first: Hello , my salary !");
	for (i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
	{
		driver->MajorFunction[i] = ccpDispatch;
	}

	driver->MajorFunction[IRP_MJ_WRITE] = ccpDispatch;
	//driver->MajorFunction[IRP_MJ_READ];
	driver->DriverUnload = ccpUnload;
	ccpAttachAllComs(driver);
	return STATUS_SUCCESS;
}