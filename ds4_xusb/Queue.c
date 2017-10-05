/*++

Module Name:

    queue.c

Abstract:

    This file contains the queue entry points and callbacks.

Environment:

    Kernel-mode Driver Framework

--*/

#include "xusb_protocol.h"

#include "driver.h"
#include "queue.tmh"

#include "Device.h"

//#include <usb.h>
//#include <Wdfusb.h>

#include <Hidclass.h>

#include "ds4_protocol.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, ds4_xusbQueueInitialize)
#endif


#define UCharToShort(v) ((SHORT)(((UCHAR)v) * 257 -32768))

#define DS4_XINPUT_VERSION	XINPUT_VERSION_1_3


VOID DS4UsbToXINPUTReport(PDS4_USB_INPUT_REPORT pDS4Report, PXINPUT_GET_GAMEPAD_STATE_BUFFER pXINPUTReport);
VOID DS4BthToXINPUTReport(PDS4_BTH_INPUT_REPORT pDS4Report, PXINPUT_GET_GAMEPAD_STATE_BUFFER pXINPUTReport);

NTSTATUS
ds4_xusbQueueInitialize(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:


     The I/O dispatch callbacks for the frameworks device object
     are configured in this function.

     A single default I/O Queue is configured for parallel request
     processing, and a driver context memory allocation is created
     to hold our structure QUEUE_CONTEXT.

Arguments:

    Device - Handle to a framework device object.

Return Value:

    VOID

--*/
{
	PAGED_CODE();

    WDFQUEUE queue;
    NTSTATUS status;
    WDF_IO_QUEUE_CONFIG    queueConfig;

    
    //
    // Configure a default queue so that requests that are not
    // configure-fowarded using WdfDeviceConfigureRequestDispatching to goto
    // other queues get dispatched here.
    //
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
         &queueConfig,
        WdfIoQueueDispatchParallel
        );

    queueConfig.EvtIoDeviceControl = ds4_xusbEvtIoDeviceControl;
    //queueConfig.EvtIoStop = ds4_xusbEvtIoStop;

    status = WdfIoQueueCreate(
                 Device,
                 &queueConfig,
                 WDF_NO_OBJECT_ATTRIBUTES,
                 &queue
                 );

    if( !NT_SUCCESS(status) )
	{
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "%!FUNC!, WdfIoQueueCreate failed %!STATUS!", status);
        return status;
    }

    return status;
}

NTSTATUS ds4_xusbBthSendOutputRequest(WDFIOTARGET ioTarget,
	UCHAR cmdType,
	UCHAR r,
	UCHAR g,
	UCHAR b,
	UCHAR rumbleLeft,
	UCHAR rumbleRight)
{
	//	Create usb write request
	/*WDF_OBJECT_ATTRIBUTES reqAttr;
	WDF_OBJECT_ATTRIBUTES_INIT(&reqAttr);
	reqAttr.ParentObject = ioTarget;

	WDFREQUEST request = NULL;
	NTSTATUS status = WdfRequestCreate(&reqAttr, ioTarget, &request);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"IOCTL_XINPUT_SET_GAMEPAD_STATE, WdfRequestCreate failed, %!STATUS!\n",
			status);

		goto CLEANUP;
	}*/
	
	UCHAR buffer[sizeof(DS4_BTH_OUTPUT_REPORT) + 1] = { 0 };

	//	Report ID 0x11
	buffer[0] = 0x11;

	PDS4_BTH_OUTPUT_REPORT pReport = (PDS4_BTH_OUTPUT_REPORT)(&buffer[1]);
	pReport->flags = 0x80;
	pReport->cmdType = cmdType;
	pReport->rumbleLeft = rumbleLeft;
	pReport->rumbleRight = rumbleRight;
	pReport->r = r;
	pReport->g = g;
	pReport->b = b;

	WDF_MEMORY_DESCRIPTOR memDesc;
	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(
		&memDesc,
		buffer,
		ARRAYSIZE(buffer)
		);

	WDF_REQUEST_SEND_OPTIONS sendOptions;
	WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions, WDF_REQUEST_SEND_OPTION_TIMEOUT);
	WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&sendOptions, -500000);	//	50ms

	NTSTATUS status = WdfIoTargetSendIoctlSynchronously(ioTarget, NULL, IOCTL_HID_SET_OUTPUT_REPORT, &memDesc, NULL, &sendOptions, NULL);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"%!FUNC!, WdfIoTargetSendIoctlSynchronously failed, %!STATUS!\n",
			status);

		goto CLEANUP;
	}

CLEANUP:
	/*if (request != NULL)
		WdfObjectDelete(request);*/

	return status;
}
NTSTATUS ds4_xusbUsbSendOutputRequest(WDFIOTARGET ioTarget,
	UCHAR cmdType,
	UCHAR r,
	UCHAR g,
	UCHAR b,
	UCHAR rumbleLeft,
	UCHAR rumbleRight)
{
	//	Create usb write request
	/*WDF_OBJECT_ATTRIBUTES reqAttr;
	WDF_OBJECT_ATTRIBUTES_INIT(&reqAttr);
	reqAttr.ParentObject = ioTarget;

	WDFREQUEST request = NULL;
	NTSTATUS status = WdfRequestCreate(&reqAttr, ioTarget, &request);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"IOCTL_XINPUT_SET_GAMEPAD_STATE, WdfRequestCreate failed, %!STATUS!\n",
			status);

		goto CLEANUP;
	}*/
	
	UCHAR buffer[sizeof(DS4_USB_OUTPUT_REPORT) + 1] = { 0 };

	//	Report ID 5
	buffer[0] = 5;

	PDS4_USB_OUTPUT_REPORT pReport = (PDS4_USB_OUTPUT_REPORT)(&buffer[1]);
	pReport->cmdType = cmdType;
	pReport->rumbleLeft = rumbleLeft;
	pReport->rumbleRight = rumbleRight;
	pReport->red = r;
	pReport->green = g;
	pReport->blue = b;

	WDF_MEMORY_DESCRIPTOR memDesc;
	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(
		&memDesc,
		buffer,
		ARRAYSIZE(buffer)
		);

	WDF_REQUEST_SEND_OPTIONS sendOptions;
	WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions, WDF_REQUEST_SEND_OPTION_TIMEOUT);
	WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&sendOptions, -500000);	//	50ms

	NTSTATUS status = WdfIoTargetSendWriteSynchronously(ioTarget, NULL, &memDesc, NULL, &sendOptions, NULL);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"%!FUNC!, WdfIoTargetSendWriteSynchronously failed, %!STATUS!\n",
			status);

		goto CLEANUP;
	}
	//	Send request
	/*WDF_REQUEST_SEND_OPTIONS sendOptions;
	WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions, 0);	//WDF_REQUEST_SEND_OPTION_TIMEOUT
	//WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&sendOptions, -100000);	//	10ms

	if (!WdfRequestSend(request, ioTarget, &sendOptions))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"IOCTL_XINPUT_SET_GAMEPAD_STATE, WdfRequestSend failed, %!STATUS!\n",
			status);

		status = WdfRequestGetStatus(request);
		return status;
	}*/
CLEANUP:
	/*if (request != NULL)
		WdfObjectDelete(request);*/

	return status;
}

NTSTATUS ds4_xusbGetBatteryLevel(WDFIOTARGET ioTarget, PDEVICE_CONTEXT deviceContext, PUCHAR pBatteryLevel)
{
	NTSTATUS status = STATUS_SUCCESS;
	WDF_OBJECT_ATTRIBUTES reqAttr;
	//WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&reqAttr, XINPUT_GET_STATE_REQUEST_CONTEXT);
	WDF_OBJECT_ATTRIBUTES_INIT(&reqAttr);
	reqAttr.ParentObject = ioTarget;

	WDFREQUEST request = NULL;
	status = WdfRequestCreate(&reqAttr, ioTarget, &request);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"%!FUNC!, WdfRequestCreate failed, %!STATUS!\n",
			status);

		goto CLEANUP;
	}

	//	Create memory buffer
	WDF_OBJECT_ATTRIBUTES memAttr;
	WDF_OBJECT_ATTRIBUTES_INIT(&memAttr);
	memAttr.ParentObject = request;

	WDFMEMORY hMemory = NULL;
	PUCHAR pBuffer = NULL;
	status = WdfMemoryCreate(&memAttr, NonPagedPool, 0, deviceContext->inputReportLength, &hMemory, &pBuffer);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"%!FUNC!, WdfMemoryCreate failed, %!STATUS!\n",
			status);

		goto CLEANUP;
	}

	//	Report ID 0x11? 0x00? doesnt matter...
	*pBuffer = 0x11;

	PDS4_BTH_INPUT_REPORT pDS4Report = (PDS4_BTH_INPUT_REPORT)(pBuffer + 1);

	WDF_MEMORY_DESCRIPTOR memDesc;
	WDF_MEMORY_DESCRIPTOR_INIT_HANDLE(
		&memDesc,
		hMemory,
		NULL
		);

	ULONG_PTR BytesRead = 0;
	status = WdfIoTargetSendReadSynchronously(ioTarget, request, &memDesc, 0, NULL, &BytesRead);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"%!FUNC!, WdfIoTargetSendReadSynchronously failed, %!STATUS!\n",
			status);

		goto CLEANUP;
	}


	*pBatteryLevel = pDS4Report->batteryLevel;

CLEANUP:
	if (request != NULL)
		WdfObjectDelete(request);

	return status;
}

NTSTATUS ds4_xusbSendBthInputRequest(WDFIOTARGET ioTarget, PDEVICE_CONTEXT deviceContext, PXINPUT_GET_GAMEPAD_STATE_BUFFER pXInputBuffer)
{
	TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
		"%!FUNC!, ioTarget state = 0x%x",
		WdfIoTargetGetState(ioTarget));

	NTSTATUS status = STATUS_SUCCESS;
	WDF_OBJECT_ATTRIBUTES reqAttr;
	//WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&reqAttr, XINPUT_GET_STATE_REQUEST_CONTEXT);
	WDF_OBJECT_ATTRIBUTES_INIT(&reqAttr);
	reqAttr.ParentObject = ioTarget;

	WDFREQUEST request = NULL;
	status = WdfRequestCreate(&reqAttr, ioTarget, &request);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"%!FUNC!, WdfRequestCreate failed, %!STATUS!\n",
			status);

		goto CLEANUP;
	}

	//	Create memory buffer
	WDF_OBJECT_ATTRIBUTES memAttr;
	WDF_OBJECT_ATTRIBUTES_INIT(&memAttr);
	memAttr.ParentObject = request;

	WDFMEMORY hMemory = NULL;
	PUCHAR pBuffer = NULL;
	status = WdfMemoryCreate(&memAttr, NonPagedPool, 0, deviceContext->inputReportLength, &hMemory, &pBuffer);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"%!FUNC!, WdfMemoryCreate failed, %!STATUS!\n",
			status);

		goto CLEANUP;
	}

	//	Report ID 0x11? 0x00? doesnt matter...
	*pBuffer = 0x11;
	
	PDS4_BTH_INPUT_REPORT pDS4Report = (PDS4_BTH_INPUT_REPORT)(pBuffer + 1);

	WDF_MEMORY_DESCRIPTOR memDesc;
	WDF_MEMORY_DESCRIPTOR_INIT_HANDLE(
		&memDesc,
		hMemory,
		NULL
		);

	ULONG_PTR BytesRead = 0;
	status = WdfIoTargetSendReadSynchronously(ioTarget, request, &memDesc, 0, NULL, &BytesRead);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"%!FUNC!, WdfIoTargetSendReadSynchronously failed, %!STATUS!\n",
			status);

		goto CLEANUP;
	}

	DS4BthToXINPUTReport(pDS4Report, pXInputBuffer);

CLEANUP:
	if (request != NULL)
		WdfObjectDelete(request);

	return status;
}
NTSTATUS ds4_xusbSendUsbInputRequest(WDFIOTARGET ioTarget, PDEVICE_CONTEXT deviceContext, PXINPUT_GET_GAMEPAD_STATE_BUFFER pXInputBuffer)
{
	NTSTATUS status = STATUS_SUCCESS;
	WDF_OBJECT_ATTRIBUTES reqAttr;
	//WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&reqAttr, XINPUT_GET_STATE_REQUEST_CONTEXT);
	WDF_OBJECT_ATTRIBUTES_INIT(&reqAttr);
	reqAttr.ParentObject = ioTarget;

	WDFREQUEST request = NULL;
	status = WdfRequestCreate(&reqAttr, ioTarget, &request);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"%!FUNC!, WdfRequestCreate failed, %!STATUS!\n",
			status);

		goto CLEANUP;
	}

	//	Create memory buffer
	WDF_OBJECT_ATTRIBUTES memAttr;
	WDF_OBJECT_ATTRIBUTES_INIT(&memAttr);
	memAttr.ParentObject = request;

	WDFMEMORY hMemory = NULL;
	PUCHAR pBuffer = NULL;
	status = WdfMemoryCreate(&memAttr, NonPagedPool, 0, deviceContext->inputReportLength, &hMemory, &pBuffer);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"%!FUNC!, WdfMemoryCreate failed, %!STATUS!\n",
			status);

		goto CLEANUP;
	}

	//	Report ID 0x11? 0x00? doesnt matter...
	*pBuffer = 0;

	PDS4_USB_INPUT_REPORT pDS4Report = (PDS4_USB_INPUT_REPORT)(pBuffer + 1);

	WDF_MEMORY_DESCRIPTOR memDesc;
	WDF_MEMORY_DESCRIPTOR_INIT_HANDLE(
		&memDesc,
		hMemory,
		NULL
		);

	ULONG_PTR BytesRead = 0;
	status = WdfIoTargetSendReadSynchronously(ioTarget, request, &memDesc, 0, NULL, &BytesRead);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"%!FUNC!, WdfIoTargetSendReadSynchronously failed, %!STATUS!\n",
			status);

		goto CLEANUP;
	}

	DS4UsbToXINPUTReport(pDS4Report, pXInputBuffer);

CLEANUP:
	if (request != NULL)
		WdfObjectDelete(request);

	return status;
}


NTSTATUS ds4_xusbSendBthInputSensorRequest(WDFIOTARGET ioTarget, PDEVICE_CONTEXT deviceContext, PDS4_GET_SENSOR_STATE_BUFFER pSensorBuffer)
{
	NTSTATUS status = STATUS_SUCCESS;
	WDF_OBJECT_ATTRIBUTES reqAttr;
	//WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&reqAttr, XINPUT_GET_STATE_REQUEST_CONTEXT);
	WDF_OBJECT_ATTRIBUTES_INIT(&reqAttr);
	reqAttr.ParentObject = ioTarget;

	WDFREQUEST request = NULL;
	status = WdfRequestCreate(&reqAttr, ioTarget, &request);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"%!FUNC!, WdfRequestCreate failed, %!STATUS!\n",
			status);

		goto CLEANUP;
	}

	//	Create memory buffer
	WDF_OBJECT_ATTRIBUTES memAttr;
	WDF_OBJECT_ATTRIBUTES_INIT(&memAttr);
	memAttr.ParentObject = request;

	WDFMEMORY hMemory = NULL;
	PUCHAR pBuffer = NULL;
	status = WdfMemoryCreate(&memAttr, NonPagedPool, 0, deviceContext->inputReportLength, &hMemory, &pBuffer);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"%!FUNC!, WdfMemoryCreate failed, %!STATUS!\n",
			status);

		goto CLEANUP;
	}

	//	Report ID 0x11? 0x00? doesnt matter...
	*pBuffer = 0x11;

	PDS4_BTH_INPUT_REPORT pDS4Report = (PDS4_BTH_INPUT_REPORT)(pBuffer + 1);

	WDF_MEMORY_DESCRIPTOR memDesc;
	WDF_MEMORY_DESCRIPTOR_INIT_HANDLE(
		&memDesc,
		hMemory,
		NULL
		);
	
	ULONG_PTR BytesRead = 0;
	status = WdfIoTargetSendReadSynchronously(ioTarget, request, &memDesc, 0, NULL, &BytesRead);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"%!FUNC!, WdfIoTargetSendReadSynchronously failed, %!STATUS!\n",
			status);

		goto CLEANUP;
	}

	pSensorBuffer->accelX = pDS4Report->accelX;
	pSensorBuffer->accelY = pDS4Report->accelY;
	pSensorBuffer->accelZ = pDS4Report->accelZ;
	pSensorBuffer->roll = pDS4Report->roll;
	pSensorBuffer->yaw = pDS4Report->yaw;
	pSensorBuffer->pitch = pDS4Report->pitch;

CLEANUP:
	if (request != NULL)
		WdfObjectDelete(request);

	return status;
}

/*
IOCTL_XINPUT_GET_LED_STATE request handler
*/
VOID ds4_xusbHandleGetLedStateRequest(PDEVICE_CONTEXT pDeviceContext, WDFREQUEST Request)
{
	//	Get input buffer
	PXINPUT_DEVICE_ID pInBuffer = NULL;
	NTSTATUS status = WdfRequestRetrieveInputBuffer(Request, sizeof(XINPUT_DEVICE_ID), &pInBuffer, NULL);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"%!FUNC!, WdfRequestRetrieveInputBuffer failed, %!STATUS!\n",
			status);
		WdfRequestComplete(Request, status);
		return;
	}

	if (pInBuffer->deviceNumber != 0)
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"%!FUNC!, invalid device number\n");
		WdfRequestComplete(Request, STATUS_INVALID_DEVICE_REQUEST);
		return;
	}

	//	Get output buffer
	PXINPUT_GET_LED_STATE_BUFFER pOutBuffer = NULL;
	status = WdfRequestRetrieveOutputBuffer(Request, sizeof(XINPUT_GET_LED_STATE_BUFFER), &pOutBuffer, NULL);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"%!FUNC!, WdfRequestRetrieveOutputBuffer failed, %!STATUS!\n",
			status);
		WdfRequestComplete(Request, status);
		return;
	}

	//	No mutex since its a UCHAR => atomic
	pOutBuffer->Version = DS4_XINPUT_VERSION;
	pOutBuffer->LedState = pDeviceContext->ledState;

	//	Complete with number of bytes copied.
	WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(XINPUT_GET_LED_STATE_BUFFER));
	return;
}

/*
IOCTL_XINPUT_GET_BATTERY_INFORMATION request handler
*/
VOID ds4_xusbHandleGetBatteryInformationRequest(PDEVICE_CONTEXT pDeviceContext, WDFREQUEST Request)
{
	UNREFERENCED_PARAMETER(pDeviceContext);

	//	Get input buffer
	PXINPUT_GET_BATTERY_INFORMATION_IN_BUFFER pInBuffer = NULL;
	NTSTATUS status = WdfRequestRetrieveInputBuffer(Request, sizeof(XINPUT_GET_BATTERY_INFORMATION_IN_BUFFER), &pInBuffer, NULL);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"%!FUNC!, WdfRequestRetrieveInputBuffer failed, %!STATUS!\n",
			status);
		WdfRequestComplete(Request, status);
		return;
	}

	if (pInBuffer->DeviceNumber != 0 || pInBuffer->DeviceType != BATTERY_DEVTYPE_GAMEPAD)
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"%!FUNC!, invalid device number\n");
		WdfRequestComplete(Request, STATUS_INVALID_DEVICE_REQUEST);
		return;
	}

	//	Get output buffer
	PXINPUT_GET_BATTERY_INFORMATION_OUT_BUFFER pOutBuffer = NULL;
	status = WdfRequestRetrieveOutputBuffer(Request, sizeof(XINPUT_GET_BATTERY_INFORMATION_OUT_BUFFER), &pOutBuffer, NULL);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"%!FUNC!, WdfRequestRetrieveOutputBuffer failed, %!STATUS!\n",
			status);
		WdfRequestComplete(Request, status);
		return;
	}

	if (pDeviceContext->busType == DS4_BUS_TYPE_USB)
	{
		pOutBuffer->Version = DS4_XINPUT_VERSION;
		pOutBuffer->BatteryType = BATTERY_TYPE_WIRED;
		pOutBuffer->BatteryLevel = BATTERY_LEVEL_FULL;
	}
	else
	{
		UCHAR ds4BatteryLevel = 0;
		status = ds4_xusbGetBatteryLevel(pDeviceContext->remoteIOTarget, pDeviceContext, &ds4BatteryLevel);
		if (!NT_SUCCESS(status))
		{
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
				"%!FUNC!, ds4_xusbGetBatteryLevel failed, %!STATUS!\n",
				status);
			WdfRequestComplete(Request, status);
			return;
		}

		pOutBuffer->Version = DS4_XINPUT_VERSION;
		pOutBuffer->BatteryType = BATTERY_TYPE_UNKNOWN;

		if (ds4BatteryLevel >= 0xF8)
			pOutBuffer->BatteryLevel = BATTERY_LEVEL_FULL;
		else if (ds4BatteryLevel >= 0x55)
			pOutBuffer->BatteryLevel = BATTERY_LEVEL_MEDIUM;
		else if (ds4BatteryLevel >= 0x01)
			pOutBuffer->BatteryLevel = BATTERY_LEVEL_LOW;
		else
			pOutBuffer->BatteryLevel = BATTERY_LEVEL_EMPTY;
	}

	//	Complete with number of bytes copied.
	WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(XINPUT_GET_BATTERY_INFORMATION_OUT_BUFFER));
	return;
}

/*
IOCTL_XINPUT_GET_CAPABILITIES request handler
*/
VOID ds4_xusbHandleGetCapabilitiesRequest(PDEVICE_CONTEXT pDeviceContext, WDFREQUEST Request)
{
	UNREFERENCED_PARAMETER(pDeviceContext);

	//	Get input buffer
	PXINPUT_DEVICE_ID pInBuffer = NULL;
	NTSTATUS status = WdfRequestRetrieveInputBuffer(Request, sizeof(XINPUT_DEVICE_ID), &pInBuffer, NULL);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"%!FUNC!, WdfRequestRetrieveInputBuffer failed, %!STATUS!\n",
			status);
		WdfRequestComplete(Request, status);
		return;
	}

	if (pInBuffer->deviceNumber != 0)
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"%!FUNC!, invalid device number\n");
		WdfRequestComplete(Request, STATUS_INVALID_DEVICE_REQUEST);
		return;
	}

	//	Get output buffer
	PXINPUT_GET_CAPABILITIES_BUFFER pOutBuffer = NULL;
	status = WdfRequestRetrieveOutputBuffer(Request, sizeof(XINPUT_GET_CAPABILITIES_BUFFER), &pOutBuffer, NULL);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"%!FUNC!, WdfRequestRetrieveOutputBuffer failed, %!STATUS!\n",
			status);
		WdfRequestComplete(Request, status);
		return;
	}

	memset(pOutBuffer, 0, sizeof(XINPUT_GET_CAPABILITIES_BUFFER));

	pOutBuffer->Version = DS4_XINPUT_VERSION;

	pOutBuffer->Type = 0;
	pOutBuffer->SubType = XINPUT_DEVSUBTYPE_GAMEPAD;
	pOutBuffer->wButtons = 0xF7FF;
	pOutBuffer->bLeftTrigger = 0xFF;
	pOutBuffer->bRightTrigger = 0xFF;
	pOutBuffer->sThumbLX = 0xFFC0;
	pOutBuffer->sThumbLY = 0xFFC0;
	pOutBuffer->sThumbRX = 0xFFC0;
	pOutBuffer->sThumbRY = 0xFFC0;
	pOutBuffer->unknown16 = 0;
	pOutBuffer->unknown17 = 0;
	pOutBuffer->unknown18 = 0;
	pOutBuffer->unknown19 = 0;
	pOutBuffer->unknown20 = 0;
	pOutBuffer->unknown21 = 0;
	pOutBuffer->wLeftMotorSpeed = 0xFF;
	pOutBuffer->wRightMotorSpeed = 0xFF;

	//	Complete with number of bytes copied.
	WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(XINPUT_GET_CAPABILITIES_BUFFER));
	return;
}

/*
IOCTL_XINPUT_GET_INFORMATION request handler
*/
VOID ds4_xusbHandleGetInformationRequest(PDEVICE_CONTEXT pDeviceContext, WDFREQUEST Request)
{
	UNREFERENCED_PARAMETER(pDeviceContext);

	//	Get buffer
	PXINPUT_GET_INFORMATION_BUFFER pBuffer = NULL;
	NTSTATUS status = WdfRequestRetrieveOutputBuffer(Request, sizeof(XINPUT_GET_INFORMATION_BUFFER), &pBuffer, NULL);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"%!FUNC!, WdfRequestRetrieveOutputBuffer failed, %!STATUS!\n",
			status);
		WdfRequestComplete(Request, status);
		return;
	}

	pBuffer->Version = DS4_XINPUT_VERSION;
	pBuffer->deviceCount = 1;

	pBuffer->unknown4 = 0;
	pBuffer->unknown8 = 94;
	pBuffer->unknown9 = 4;
	pBuffer->unknown10 = 142;
	pBuffer->unknown11 = 2;

	//	Complete with number of bytes copied.
	WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(XINPUT_GET_INFORMATION_BUFFER));
	return;
}

/*
IOCTL_XINPUT_SET_GAMEPAD_STATE request handler
*/
VOID ds4_xusbHandleSetGamepadStateRequest(PDEVICE_CONTEXT pDeviceContext, WDFREQUEST Request)
{
	//	Retrieve input buffer
	PXINPUT_SET_GAMEPAD_STATE_BUFFER pBuffer = NULL;
	NTSTATUS status = WdfRequestRetrieveInputBuffer(Request, sizeof(XINPUT_SET_GAMEPAD_STATE_BUFFER), &pBuffer, NULL);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"%!FUNC!, WdfRequestRetrieveInputBuffer failed, %!STATUS!\n",
			status);
		WdfRequestComplete(Request, status);
		return;
	}

	//	Check device number
	if (pBuffer->DeviceNumber != 0)
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"%!FUNC!, invalid device number\n");
		WdfRequestComplete(Request, STATUS_INVALID_DEVICE_REQUEST);
		return;
	}

	//	Check command type
	if (pBuffer->CommandType == XINPUT_COMMAND_VIBRATION)
	{
		if (pDeviceContext->targetOpened == FALSE)
		{
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
				"%!FUNC!, IO target is not opened");
			WdfRequestComplete(Request, STATUS_UNSUCCESSFUL);
			return;
		}
		switch (pDeviceContext->busType)
		{
		case DS4_BUS_TYPE_USB:
		{
			status = ds4_xusbUsbSendOutputRequest(pDeviceContext->remoteIOTarget, DS4_OUT_CMD_TYPE_VIBRATION, 0, 0, 0, pBuffer->LeftMotorSpeed, pBuffer->RightMotorSpeed);
			if (!NT_SUCCESS(status))
			{
				TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
					"%!FUNC!, ds4_xusbUsbSendOutputRequest failed, %!STATUS!\n",
					status);
				WdfRequestComplete(Request, status);
				return;
			}
			break;
		}
		case DS4_BUS_TYPE_BLUETOOTH:
		{
			status = ds4_xusbBthSendOutputRequest(pDeviceContext->remoteIOTarget, DS4_OUT_CMD_TYPE_VIBRATION, 0, 0, 0, pBuffer->LeftMotorSpeed, pBuffer->RightMotorSpeed);
			if (!NT_SUCCESS(status))
			{
				TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
					"%!FUNC!, ds4_xusbBthSendOutputRequest failed, %!STATUS!\n",
					status);
				WdfRequestComplete(Request, status);
				return;
			}
			break;
		}
		default:
			WdfRequestComplete(Request, STATUS_UNSUCCESSFUL);
			return;
		}

		if (!NT_SUCCESS(status))
			WdfRequestComplete(Request, status);
		else
			WdfRequestComplete(Request, STATUS_SUCCESS);

		return;

	}
	else if (pBuffer->CommandType == XINPUT_COMMAND_LED)
	{
		pDeviceContext->ledState = pBuffer->LedState;
		WdfRequestComplete(Request, STATUS_SUCCESS);
		return;
	}
	else
	{
		WdfRequestComplete(Request, STATUS_INVALID_DEVICE_REQUEST);
		return;
	}
}

/*
IOCTL_XINPUT_GET_GAMEPAD_STATE request handler
*/
VOID ds4_xusbHandleGetGamepadStateRequest(size_t InputBufferLength, PDEVICE_CONTEXT pDeviceContext, WDFREQUEST Request)
{
	NTSTATUS status = STATUS_SUCCESS;

	if (InputBufferLength == sizeof(XINPUT_DEVICE_ID_V_0100))
	{
#pragma region XINPUT_9_1_0
		//	Get input buffer
		PXINPUT_DEVICE_ID_V_0100 pInBuffer = NULL;
		status = WdfRequestRetrieveInputBuffer(Request, sizeof(XINPUT_DEVICE_ID_V_0100), &pInBuffer, NULL);
		if (!NT_SUCCESS(status))
		{
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
				"%!FUNC!, WdfRequestRetrieveInputBuffer failed, %!STATUS!\n",
				status);
			WdfRequestComplete(Request, status);
			return;
		}

		if (pInBuffer->deviceNumber != 0)
		{
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
				"%!FUNC!, invalid device number\n");
			WdfRequestComplete(Request, STATUS_INVALID_DEVICE_REQUEST);
			return;
		}

		//	Check if IO target is ready to use
		if (pDeviceContext->targetOpened == FALSE)
		{
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
				"%!FUNC!, IO target is not opened");
			WdfRequestComplete(Request, STATUS_UNSUCCESSFUL);
			return;
		}

		//	Get buffer
		PXINPUT_GET_GAMEPAD_STATE_BUFFER_V_0100 pOutBuffer = NULL;
		WdfRequestRetrieveOutputBuffer(Request, sizeof(XINPUT_GET_GAMEPAD_STATE_BUFFER_V_0100), &pOutBuffer, NULL);
		if (!NT_SUCCESS(status))
		{
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
				"%!FUNC!, WdfRequestRetrieveOutputBuffer failed, %!STATUS!\n",
				status);
			WdfRequestComplete(Request, status);
			return;
		}

		//	Read report
		XINPUT_GET_GAMEPAD_STATE_BUFFER xinput_buffer;

		switch (pDeviceContext->busType)
		{
		case DS4_BUS_TYPE_USB:
		{
			status = ds4_xusbSendUsbInputRequest(pDeviceContext->remoteIOTarget, pDeviceContext, &xinput_buffer);
			if (!NT_SUCCESS(status))
			{
				TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
					"%!FUNC!, ds4_xusbSendUsbInputRequest failed, %!STATUS!\n",
					status);
				WdfRequestComplete(Request, status);
				return;
			}
			break;
		}
		case DS4_BUS_TYPE_BLUETOOTH:
		{
			status = ds4_xusbSendBthInputRequest(pDeviceContext->remoteIOTarget, pDeviceContext, &xinput_buffer);
			if (!NT_SUCCESS(status))
			{
				TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
					"%!FUNC!, ds4_xusbSendBluetoothInputRequest failed, %!STATUS!\n",
					status);
				WdfRequestComplete(Request, status);
				return;
			}
			break;
		}
		default:
			WdfRequestComplete(Request, STATUS_UNSUCCESSFUL);
			return;
		}

		//	Copy buffer content
		pOutBuffer->Success = TRUE;
		pOutBuffer->unknown = 0;
		pOutBuffer->unknown3 = 0xFF;
		pOutBuffer->PacketNumber = ++pDeviceContext->reportCount;
		pOutBuffer->wButtons = xinput_buffer.wButtons;
		pOutBuffer->LeftTrigger = xinput_buffer.LeftTrigger;
		pOutBuffer->RightTrigger = xinput_buffer.RightTrigger;
		pOutBuffer->ThumbLX = xinput_buffer.ThumbLX;
		pOutBuffer->ThumbLY = xinput_buffer.ThumbLY;
		pOutBuffer->ThumbRX = xinput_buffer.ThumbRX;
		pOutBuffer->ThumbRY = xinput_buffer.ThumbRY;
		pOutBuffer->unknown4 = 6;

		//	Complete with number of bytes copied.
		WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(XINPUT_GET_GAMEPAD_STATE_BUFFER_V_0100));
		return;
#pragma endregion
	}
	else if (InputBufferLength == sizeof(XINPUT_DEVICE_ID))
	{
#pragma region XINPUT_1_3
		//	Get input buffer
		PXINPUT_DEVICE_ID pInBuffer = NULL;
		status = WdfRequestRetrieveInputBuffer(Request, sizeof(XINPUT_DEVICE_ID), &pInBuffer, NULL);
		if (!NT_SUCCESS(status))
		{
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
				"%!FUNC!, WdfRequestRetrieveInputBuffer failed, %!STATUS!\n",
				status);
			WdfRequestComplete(Request, status);
			return;
		}

		if (pInBuffer->deviceNumber != 0)
		{
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
				"%!FUNC!, invalid device number\n");
			WdfRequestComplete(Request, STATUS_INVALID_DEVICE_REQUEST);
			return;
		}

		//	Check if IO target is ready to use
		if (pDeviceContext->targetOpened == FALSE)
		{
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
				"%!FUNC!, IO target is not opened");
			WdfRequestComplete(Request, STATUS_UNSUCCESSFUL);
			return;
		}

		//	Get buffer
		PXINPUT_GET_GAMEPAD_STATE_BUFFER pOutBuffer = NULL;
		status = WdfRequestRetrieveOutputBuffer(Request, sizeof(XINPUT_GET_GAMEPAD_STATE_BUFFER), &pOutBuffer, NULL);
		if (!NT_SUCCESS(status))
		{
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
				"%!FUNC!, WdfRequestRetrieveOutputBuffer failed, %!STATUS!\n",
				status);
			WdfRequestComplete(Request, status);
			return;
		}

		//	Read report
		switch (pDeviceContext->busType)
		{
		case DS4_BUS_TYPE_USB:
		{
			status = ds4_xusbSendUsbInputRequest(pDeviceContext->remoteIOTarget, pDeviceContext, pOutBuffer);
			if (!NT_SUCCESS(status))
			{
				TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
					"%!FUNC!, ds4_xusbSendUsbInputRequest failed, %!STATUS!\n",
					status);
				WdfRequestComplete(Request, status);
				return;
			}
			break;
		}
		case DS4_BUS_TYPE_BLUETOOTH:
		{
			status = ds4_xusbSendBthInputRequest(pDeviceContext->remoteIOTarget, pDeviceContext, pOutBuffer);
			if (!NT_SUCCESS(status))
			{
				TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
					"%!FUNC!, ds4_xusbSendBluetoothInputRequest failed, %!STATUS!\n",
					status);
				WdfRequestComplete(Request, status);
				return;
			}
			break;
		}
		default:
			WdfRequestComplete(Request, STATUS_UNSUCCESSFUL);
			return;
		}

		pOutBuffer->Success = TRUE;
		pOutBuffer->Version = XINPUT_VERSION_1_3;
		pOutBuffer->PacketNumber = ++pDeviceContext->reportCount;


		//	Release mutex
		//WdfSpinLockRelease(pDeviceContext->reportBufferLock);


		//	Complete with number of bytes copied.
		WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(XINPUT_GET_GAMEPAD_STATE_BUFFER));
		return;
#pragma endregion
	}
	else
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"%!FUNC!, input buffer length invalid.\n");
		WdfRequestComplete(Request, STATUS_BUFFER_TOO_SMALL);
		return;
	}
}

/*
IOCTL_DS4_GET_LED_COLOR request handler
*/

VOID ds4_xusbHandleDS4GetLedColorRequest(WDFDEVICE pDevice, PDEVICE_CONTEXT pDeviceContext, WDFREQUEST Request)
{
	UNREFERENCED_PARAMETER(pDevice);

	//	Retrieve output buffer
	PDS4_GET_LED_COLOR_BUFFER pBuffer = NULL;
	NTSTATUS status = WdfRequestRetrieveOutputBuffer(Request, sizeof(DS4_GET_LED_COLOR_BUFFER), &pBuffer, NULL);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"%!FUNC!, WdfRequestRetrieveOutputBuffer failed, %!STATUS!\n",
			status);
		WdfRequestComplete(Request, status);
		return;
	}

	pBuffer->R = pDeviceContext->ledR;
	pBuffer->G = pDeviceContext->ledG;
	pBuffer->B = pDeviceContext->ledB;

	WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(DS4_GET_LED_COLOR_BUFFER));
}

/*
IOCTL_DS4_SET_LED_COLOR request handler
*/

VOID ds4_xusbHandleDS4SetLedColorRequest(WDFDEVICE pDevice, PDEVICE_CONTEXT pDeviceContext, WDFREQUEST Request)
{
	//	Retrieve input buffer
	PDS4_SET_LED_COLOR_BUFFER pBuffer = NULL;
	NTSTATUS status = WdfRequestRetrieveInputBuffer(Request, sizeof(DS4_SET_LED_COLOR_BUFFER), &pBuffer, NULL);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"%!FUNC!, WdfRequestRetrieveInputBuffer failed, %!STATUS!\n",
			status);
		WdfRequestComplete(Request, status);
		return;
	}

	status = ds4_xusbBthSendOutputRequest(pDeviceContext->remoteIOTarget, DS4_OUT_CMD_TYPE_LED, pBuffer->R, pBuffer->G, pBuffer->B, 0, 0);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"%!FUNC!, ds4_xusbBthSendOutputRequest failed, %!STATUS!\n",
			status);
		WdfRequestComplete(Request, status);
		return;
	}

	UCHAR data[4] = { 0 };
	data[0] = pDeviceContext->ledR = pBuffer->R;
	data[1] = pDeviceContext->ledG = pBuffer->G;
	data[2] = pDeviceContext->ledB = pBuffer->B;

	status = IoSetDevicePropertyData(WdfDeviceWdmGetPhysicalDevice(pDevice), &DS4_XUSB_DEVPROP_LEDCOLOR, LOCALE_NEUTRAL, 0, DEVPROP_TYPE_UINT32, 4, data);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"%!FUNC!, IoSetDevicePropertyData failed, %!STATUS!\n",
			status);
		WdfRequestComplete(Request, status);
		return;
	}

	WdfRequestComplete(Request, STATUS_SUCCESS);
}

/*
IOCTL_DS4_GET_BATTERY_STATE request handler
*/

VOID ds4_xusbHandleDS4GetBatteryStateRequest(PDEVICE_CONTEXT pDeviceContext, WDFREQUEST Request)
{
	//	Retrieve output buffer
	PDS4_GET_BATTERY_STATE_BUFFER pBuffer = NULL;
	NTSTATUS status = WdfRequestRetrieveOutputBuffer(Request, sizeof(DS4_GET_BATTERY_STATE_BUFFER), &pBuffer, NULL);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"%!FUNC!, WdfRequestRetrieveOutputBuffer failed, %!STATUS!\n",
			status);
		WdfRequestComplete(Request, status);
		return;
	}
	
	UCHAR ds4BatteryLevel = 0;
	status = ds4_xusbGetBatteryLevel(pDeviceContext->remoteIOTarget, pDeviceContext, &ds4BatteryLevel);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"%!FUNC!, ds4_xusbGetBatteryLevel failed, %!STATUS!\n",
			status);
		WdfRequestComplete(Request, status);
		return;
	}

	pBuffer->batteryLevel = ds4BatteryLevel;

	WdfRequestComplete(Request, STATUS_SUCCESS);
}

/*
IOCTL_DS4_GET_SENSOR_STATE request handler
*/

VOID ds4_xusbHandleDS4GetSensorStateRequest(PDEVICE_CONTEXT pDeviceContext, WDFREQUEST Request)
{
	//	Retrieve output buffer
	PDS4_GET_SENSOR_STATE_BUFFER pSensorBuffer = NULL;
	NTSTATUS status = WdfRequestRetrieveOutputBuffer(Request, sizeof(DS4_GET_SENSOR_STATE_BUFFER), &pSensorBuffer, NULL);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"%!FUNC!, WdfRequestRetrieveOutputBuffer failed, %!STATUS!\n",
			status);
		WdfRequestComplete(Request, status);
		return;
	}


	//	Read report
	switch (pDeviceContext->busType)
	{
	case DS4_BUS_TYPE_USB:
	{
		//!\ have to find a way to get sensor data in USB mode.
		memset(pSensorBuffer, 0, sizeof(PDS4_GET_SENSOR_STATE_BUFFER));
		break;
	}
	case DS4_BUS_TYPE_BLUETOOTH:
	{
		status = ds4_xusbSendBthInputSensorRequest(pDeviceContext->remoteIOTarget, pDeviceContext, pSensorBuffer);
		if (!NT_SUCCESS(status))
		{
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
				"%!FUNC!, ds4_xusbSendBluetoothInputRequest failed, %!STATUS!\n",
				status);
			WdfRequestComplete(Request, status);
			return;
		}
		break;
	}
	default:
		WdfRequestComplete(Request, STATUS_UNSUCCESSFUL);
		return;
	}


	WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(DS4_GET_SENSOR_STATE_BUFFER));
}

VOID
ds4_xusbEvtIoDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
{
	WDFDEVICE device = WdfIoQueueGetDevice(Queue);
	PDEVICE_CONTEXT pDeviceContext = DeviceGetContext(device);

    /*TraceEvents(TRACE_LEVEL_INFORMATION, 
				TRACE_QUEUE,
                "!FUNC! Queue 0x%p, Request 0x%p OutputBufferLength %d InputBufferLength %d IoControlCode %d", 
                Queue, Request, (int) OutputBufferLength, (int) InputBufferLength, IoControlCode);*/

	ULONG DeviceType = IoControlCode >> 16;

	if (DeviceType == FILE_DEVICE_XUSB)
	{
		switch (IoControlCode)
		{
		/*
		DS4 specific ioctls
		*/
		case IOCTL_DS4_GET_LED_COLOR:
		{
			ds4_xusbHandleDS4GetLedColorRequest(device, pDeviceContext, Request);
			return;
		}
		case IOCTL_DS4_SET_LED_COLOR:
		{
			ds4_xusbHandleDS4SetLedColorRequest(device, pDeviceContext, Request);
			return;
		}
		case IOCTL_DS4_GET_BATTERY_STATE:
		{
			ds4_xusbHandleDS4GetBatteryStateRequest(pDeviceContext, Request);
			return;
		}
		case IOCTL_DS4_GET_SENSOR_STATE:
		{
			ds4_xusbHandleDS4GetSensorStateRequest(pDeviceContext, Request);
			return;
		}
		/*
		XINPUT ioctls
		*/
		case IOCTL_XINPUT_GET_LED_STATE:
		{
			ds4_xusbHandleGetLedStateRequest(pDeviceContext, Request);
			return;
		}
		case IOCTL_XINPUT_GET_BATTERY_INFORMATION:
		{
			ds4_xusbHandleGetBatteryInformationRequest(pDeviceContext, Request);
			return;
		}
		case IOCTL_XINPUT_GET_CAPABILITIES:
		{
			ds4_xusbHandleGetCapabilitiesRequest(pDeviceContext, Request);
			return;
		}
		case IOCTL_XINPUT_GET_INFORMATION:
		{
			ds4_xusbHandleGetInformationRequest(pDeviceContext, Request);
			return;
		}
		case IOCTL_XINPUT_SET_GAMEPAD_STATE:
		{
			ds4_xusbHandleSetGamepadStateRequest(pDeviceContext, Request);
			return;
		}
		case IOCTL_XINPUT_GET_GAMEPAD_STATE:
		{
			ds4_xusbHandleGetGamepadStateRequest(InputBufferLength, pDeviceContext, Request);
			return;
		}
		default:
			//	Unknown ioctl for XUSB device
			TraceEvents(TRACE_LEVEL_ERROR,
				TRACE_QUEUE,
				"UNKNOWN IOCTL Queue 0x%p, Request 0x%p OutputBufferLength %d InputBufferLength %d IoControlCode %x",
				Queue, Request, (int)OutputBufferLength, (int)InputBufferLength, IoControlCode);
			WdfRequestComplete(Request, STATUS_INVALID_DEVICE_REQUEST);
			//WdfRequestComplete(Request, STATUS_SUCCESS);
			return;
		}
	}
	else
	{
		/*
		Unknown device type (probably HID)
		Just forward the request to next driver
		*/

		//	Format the request
		WdfRequestFormatRequestUsingCurrentType(Request);

		//	Then just send it and forget
		WDF_REQUEST_SEND_OPTIONS sendOptions;
		WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions, WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);

		BOOLEAN b = WdfRequestSend(Request, WdfDeviceGetIoTarget(WdfIoQueueGetDevice(Queue)), &sendOptions);
		if (b == FALSE)
		{
			NTSTATUS status = WdfRequestGetStatus(Request);
			WdfRequestComplete(Request, status);
		}
		return;
	}

    return;
}

BOOLEAN ReportCompare(PXINPUT_GET_GAMEPAD_STATE_BUFFER pReport1, PXINPUT_GET_GAMEPAD_STATE_BUFFER pReport2)
{
	if (pReport1->wButtons != pReport2->wButtons)
		return TRUE;
	if (pReport1->LeftTrigger != pReport2->LeftTrigger)
		return TRUE;
	if (pReport1->RightTrigger != pReport2->RightTrigger)
		return TRUE;
	if (pReport1->ThumbLX != pReport2->ThumbLX)
		return TRUE;
	if (pReport1->ThumbLY != pReport2->ThumbLY)
		return TRUE;
	if (pReport1->ThumbRX != pReport2->ThumbRX)
		return TRUE;
	if (pReport1->ThumbRY != pReport2->ThumbRY)
		return TRUE;

	return FALSE;
}


USHORT PS4ToXInputDPad(UCHAR value)
{
	switch (value)
	{
	case 0:
		return XINPUT_GAMEPAD_DPAD_UP;
	case 1:
		return XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_RIGHT;
	case 2:
		return XINPUT_GAMEPAD_DPAD_RIGHT;
	case 3:
		return XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_DPAD_DOWN;
	case 4:
		return XINPUT_GAMEPAD_DPAD_DOWN;
	case 5:
		return XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_DPAD_LEFT;
	case 6:
		return XINPUT_GAMEPAD_DPAD_LEFT;
	case 7:
		return XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_UP;
	default:
		return 0;
	}
}

/*
Does not change the packet number
*/
VOID DS4UsbToXINPUTReport(PDS4_USB_INPUT_REPORT pDS4Report, PXINPUT_GET_GAMEPAD_STATE_BUFFER pXINPUTReport)
{
	pXINPUTReport->Version = DS4_XINPUT_VERSION;

	pXINPUTReport->Success = TRUE;

	pXINPUTReport->unknown2 = 0;
	pXINPUTReport->unknown3 = 0xFF;
	pXINPUTReport->PacketNumber = 0;
	pXINPUTReport->unknown4 = 0;
	pXINPUTReport->unknown5 = 20;

	pXINPUTReport->wButtons = PS4ToXInputDPad(pDS4Report->Buttons[0] & 0xF);

	if (pDS4Report->Buttons[0] & 0x10)
		pXINPUTReport->wButtons |= XINPUT_GAMEPAD_X;

	if (pDS4Report->Buttons[0] & 0x20)
		pXINPUTReport->wButtons |= XINPUT_GAMEPAD_A;

	if (pDS4Report->Buttons[0] & 0x40)
		pXINPUTReport->wButtons |= XINPUT_GAMEPAD_B;

	if (pDS4Report->Buttons[0] & 0x80)
		pXINPUTReport->wButtons |= XINPUT_GAMEPAD_Y;

	if (pDS4Report->Buttons[1] & 0x01)
		pXINPUTReport->wButtons |= XINPUT_GAMEPAD_LEFT_SHOULDER;

	if (pDS4Report->Buttons[1] & 0x02)
		pXINPUTReport->wButtons |= XINPUT_GAMEPAD_RIGHT_SHOULDER;

	if (pDS4Report->Buttons[1] & 0x10)
		pXINPUTReport->wButtons |= XINPUT_GAMEPAD_BACK;

	if (pDS4Report->Buttons[1] & 0x20)
		pXINPUTReport->wButtons |= XINPUT_GAMEPAD_START;

	if (pDS4Report->Buttons[1] & 0x40)
		pXINPUTReport->wButtons |= XINPUT_GAMEPAD_LEFT_THUMB;

	if (pDS4Report->Buttons[1] & 0x80)
		pXINPUTReport->wButtons |= XINPUT_GAMEPAD_RIGHT_THUMB;

	pXINPUTReport->LeftTrigger = pDS4Report->Rx;
	pXINPUTReport->RightTrigger = pDS4Report->Ry;
	pXINPUTReport->ThumbLX = UCharToShort(pDS4Report->X);
	pXINPUTReport->ThumbLY = -UCharToShort(pDS4Report->Y);
	pXINPUTReport->ThumbRX = UCharToShort(pDS4Report->Z);
	pXINPUTReport->ThumbRY = -UCharToShort(pDS4Report->Rz);

	pXINPUTReport->unknown6 = 0;
	pXINPUTReport->unknown7 = 0;
	pXINPUTReport->unknown8 = 0;
	pXINPUTReport->unknown9 = 0;
	pXINPUTReport->unknown10 = 0;
	pXINPUTReport->unknown11 = 0;
}
VOID DS4BthToXINPUTReport(PDS4_BTH_INPUT_REPORT pDS4Report, PXINPUT_GET_GAMEPAD_STATE_BUFFER pXINPUTReport)
{
	pXINPUTReport->Version = DS4_XINPUT_VERSION;

	pXINPUTReport->Success = TRUE;

	pXINPUTReport->unknown2 = 0;
	pXINPUTReport->unknown3 = 0xFF;
	pXINPUTReport->PacketNumber = 0;
	pXINPUTReport->unknown4 = 0;
	pXINPUTReport->unknown5 = 20;

	pXINPUTReport->wButtons = PS4ToXInputDPad(pDS4Report->Buttons[0] & 0xF);

	if (pDS4Report->Buttons[0] & 0x10)
		pXINPUTReport->wButtons |= XINPUT_GAMEPAD_X;

	if (pDS4Report->Buttons[0] & 0x20)
		pXINPUTReport->wButtons |= XINPUT_GAMEPAD_A;

	if (pDS4Report->Buttons[0] & 0x40)
		pXINPUTReport->wButtons |= XINPUT_GAMEPAD_B;

	if (pDS4Report->Buttons[0] & 0x80)
		pXINPUTReport->wButtons |= XINPUT_GAMEPAD_Y;

	if (pDS4Report->Buttons[1] & 0x01)
		pXINPUTReport->wButtons |= XINPUT_GAMEPAD_LEFT_SHOULDER;

	if (pDS4Report->Buttons[1] & 0x02)
		pXINPUTReport->wButtons |= XINPUT_GAMEPAD_RIGHT_SHOULDER;

	if (pDS4Report->Buttons[1] & 0x10)
		pXINPUTReport->wButtons |= XINPUT_GAMEPAD_BACK;

	if (pDS4Report->Buttons[1] & 0x20)
		pXINPUTReport->wButtons |= XINPUT_GAMEPAD_START;

	if (pDS4Report->Buttons[1] & 0x40)
		pXINPUTReport->wButtons |= XINPUT_GAMEPAD_LEFT_THUMB;

	if (pDS4Report->Buttons[1] & 0x80)
		pXINPUTReport->wButtons |= XINPUT_GAMEPAD_RIGHT_THUMB;

	pXINPUTReport->LeftTrigger = pDS4Report->Rx;
	pXINPUTReport->RightTrigger = pDS4Report->Ry;
	pXINPUTReport->ThumbLX = UCharToShort(pDS4Report->X);
	pXINPUTReport->ThumbLY = -UCharToShort(pDS4Report->Y);
	pXINPUTReport->ThumbRX = UCharToShort(pDS4Report->Z);
	pXINPUTReport->ThumbRY = -UCharToShort(pDS4Report->Rz);

	pXINPUTReport->unknown6 = 0;
	pXINPUTReport->unknown7 = 0;
	pXINPUTReport->unknown8 = 0;
	pXINPUTReport->unknown9 = 0;
	pXINPUTReport->unknown10 = 0;
	pXINPUTReport->unknown11 = 0;
}
