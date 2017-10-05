#pragma once

/*++

Module Name:

    queue.h

Abstract:

    This file contains the queue definitions.

Environment:

    Kernel-mode Driver Framework

--*/

//
// This is the context that can be placed per queue
// and would contain per queue information.
//
typedef struct _QUEUE_CONTEXT {

    ULONG PrivateDeviceData;  // just a placeholder

} QUEUE_CONTEXT, *PQUEUE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(QUEUE_CONTEXT, QueueGetContext)


typedef struct _XINPUT_GET_STATE_REQUEST_CONTEXT
{
	WDFREQUEST request;
	WDFREQUEST hidRequest;

} XINPUT_GET_STATE_REQUEST_CONTEXT, *PXINPUT_GET_STATE_REQUEST_CONTEXT;

//
// This macro will generate an inline function called DeviceGetContext
// which will be used to get a pointer to the device context memory
// in a type safe manner.
//
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(XINPUT_GET_STATE_REQUEST_CONTEXT, XInputGetStateRequestGetContext)

NTSTATUS
ds4_xusbQueueInitialize(
    _In_ WDFDEVICE hDevice
    );

//
// Events from the IoQueue object
//
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL ds4_xusbEvtIoDeviceControl;
EVT_WDF_IO_QUEUE_IO_STOP ds4_xusbEvtIoStop;


NTSTATUS ds4_xusbBthSendOutputRequest(WDFIOTARGET ioTarget,
	UCHAR cmdType,
	UCHAR r,
	UCHAR g,
	UCHAR b,
	UCHAR rumbleLeft,
	UCHAR rumbleRight);
NTSTATUS ds4_xusbUsbSendOutputRequest(WDFIOTARGET ioTarget,
	UCHAR cmdType,
	UCHAR r,
	UCHAR g,
	UCHAR b,
	UCHAR rumbleLeft,
	UCHAR rumbleRight);