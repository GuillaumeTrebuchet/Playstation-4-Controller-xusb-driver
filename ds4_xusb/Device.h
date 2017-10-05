#pragma once
/*++

Module Name:

    device.h

Abstract:

    This file contains the device definitions.

Environment:

    Kernel-mode Driver Framework

--*/

#include "public.h"

#include "ds4_protocol.h"
#include "xusb_protocol.h"


typedef enum _DS4_BUS_TYPE
{
	DS4_BUS_TYPE_UNKNOWN,
	DS4_BUS_TYPE_USB,
	DS4_BUS_TYPE_BLUETOOTH,
} DS4_BUS_TYPE;

//
// The device context performs the same job as
// a WDM device extension in the driver frameworks
//
//	Device context is non pageable
typedef struct _DEVICE_CONTEXT
{
	UCHAR				ledState;

	ULONG				reportCount;

	BOOLEAN				targetOpened;
	WDFIOTARGET			remoteIOTarget;

	PVOID				notificationEntry;
	DS4_BUS_TYPE		busType;

	ULONG				featureReportLength;
	ULONG				inputReportLength;
	ULONG				outputReportLength;

	UCHAR				ledR;
	UCHAR				ledG;
	UCHAR				ledB;

} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

//
// This macro will generate an inline function called DeviceGetContext
// which will be used to get a pointer to the device context memory
// in a type safe manner.
//
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceGetContext)

//
// Function to initialize the device and its callbacks
//
NTSTATUS
ds4_xusbCreateDevice(WDFDRIVER driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
    );

