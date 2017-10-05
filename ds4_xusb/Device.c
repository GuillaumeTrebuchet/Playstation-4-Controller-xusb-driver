/*++

Module Name:

    device.c - Device handling events for example driver.

Abstract:

   This file contains the device entry points and callbacks.
    
Environment:

    Kernel-mode Driver Framework

--*/


#include "driver.h"
#include "device.tmh"

#include "Device.h"
#include "Queue.h"

#include <usb.h>
#include <Wdfusb.h>
#include <usbdlib.h>

#include <usbiodef.h>

#include "xusb_protocol.h"

#include <Wdmguid.h>
#include <wdf.h>

#pragma warning(disable:4201)  // nameless struct/union
#pragma warning(disable:4214)  // bit field types other than int

#include <hidpddi.h>
#include <hidclass.h>

#pragma warning(default:4201)
#pragma warning(default:4214)

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, ds4_xusbCreateDevice)
#endif

// {F94BBB6E-0408-4895-9314-1A55788991EC}
DEFINE_GUID(GUID_DEVCLASS_DS4_CONTROLLER,
	0xf94bbb6e, 0x408, 0x4895, 0x93, 0x14, 0x1a, 0x55, 0x78, 0x89, 0x91, 0xec);

#include <ntstrsafe.h>


NTSTATUS ds4_xusbOpenHidIoTarget(WDFDEVICE device)
{
	PDEVICE_CONTEXT deviceContext = DeviceGetContext(device);

	//	Get PDO's name
	WDFMEMORY hMemory;
	NTSTATUS status = WdfDeviceAllocAndQueryProperty(device,
		DevicePropertyPhysicalDeviceObjectName,
		NonPagedPool,
		WDF_NO_OBJECT_ATTRIBUTES,
		&hMemory);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
			"%!FUNC!, WdfDeviceAllocAndQueryProperty failed, %!STATUS!\n",
			status);
		goto CLEANUP;
	}

	UNICODE_STRING PdoName;
	size_t bufferLength = 0;
	PdoName.Buffer = WdfMemoryGetBuffer(hMemory, &bufferLength);
	PdoName.MaximumLength = (USHORT)bufferLength;
	PdoName.Length = (USHORT)bufferLength - sizeof(UNICODE_NULL);

	if (PdoName.Buffer == NULL)
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
			"%!FUNC!, PdoName buffer is NULL");
		goto CLEANUP;
	}


	//	Open IO target
	WDF_IO_TARGET_OPEN_PARAMS openParams;
	WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(
		&openParams,
		&PdoName,
		FILE_READ_ACCESS | FILE_WRITE_ACCESS);

	openParams.ShareAccess = FILE_SHARE_WRITE | FILE_SHARE_READ;

	status = WdfIoTargetOpen(deviceContext->remoteIOTarget, &openParams);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
			"%!FUNC!, WdfIoTargetOpen failed, %!STATUS!\n",
			status);
		goto CLEANUP;
	}
	deviceContext->targetOpened = TRUE;

CLEANUP:
	//	Cleanup
	WdfObjectDelete(hMemory);

	return status;
}


DECLARE_CONST_UNICODE_STRING(ds4_usb_hardwareID, L"HID\\VID_054C&PID_05C4");
DECLARE_CONST_UNICODE_STRING(ds4_bluetooth_hardwareID, L"HID\\{00001124-0000-1000-8000-00805F9B34FB}_VID&0002054C_PID&05C4");

DECLARE_CONST_UNICODE_STRING(ds4_usb_touchpad_hardwareID, L"HID\\VID_054C&PID_05C4\\TOUCHPAD");
DECLARE_CONST_UNICODE_STRING(ds4_bluetooth_touchpad_hardwareID, L"HID\\{00001124-0000-1000-8000-00805F9B34FB}_VID&0002054C_PID&05C4\\TOUCHPAD");

DECLARE_CONST_UNICODE_STRING(ds4_usb_touchpad_instanceID, L"HID\\VID_054C&PID_05C4\\TOUCHPAD01");
DECLARE_CONST_UNICODE_STRING(ds4_bluetooth_touchpad_instanceID, L"HID\\{00001124-0000-1000-8000-00805F9B34FB}_VID&0002054C_PID&05C4\\TOUCHPAD01");

DECLARE_CONST_UNICODE_STRING(ds4_usb_touchpad_desc, L"touchpad usb test desc");
DECLARE_CONST_UNICODE_STRING(ds4_bluetooth_touchpad_desc, L"touchpad bth test desc");

DECLARE_CONST_UNICODE_STRING(ds4_touchpad_location, L"DS4 Controller Bus 0");

NTSTATUS ds4_xusbInitReportSize(WDFDEVICE device, PDEVICE_CONTEXT deviceContext, WDFIOTARGET ioTarget)
{
	WDFMEMORY hMemory = NULL;
	PHIDP_PREPARSED_DATA pPreparsedData = NULL;
	HID_COLLECTION_INFORMATION collectionInfo;
	WDF_OBJECT_ATTRIBUTES memAttr;

	WDF_MEMORY_DESCRIPTOR desc;
	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&desc,
		&collectionInfo,
		sizeof(HID_COLLECTION_INFORMATION));

	//	Get collection information
	NTSTATUS status = WdfIoTargetSendIoctlSynchronously(ioTarget,
		NULL,
		IOCTL_HID_GET_COLLECTION_INFORMATION,
		NULL,
		&desc,
		NULL,
		NULL);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
			"%!FUNC! WdfIoTargetSendIoctlSynchronously failed, %!STATUS!", status);

		goto CLEANUP;
	}

	//	Allocate memory
	WDF_OBJECT_ATTRIBUTES_INIT(&memAttr);
	memAttr.ParentObject = device;

	status = WdfMemoryCreate(&memAttr, NonPagedPool, 0, collectionInfo.DescriptorSize, &hMemory, &pPreparsedData);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
			"%!FUNC! WdfMemoryCreate failed, %!STATUS!", status);

		goto CLEANUP;
	}


	//	Get preparsed data
	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&desc,
		pPreparsedData,
		collectionInfo.DescriptorSize);

	status = WdfIoTargetSendIoctlSynchronously(ioTarget,
		NULL,
		IOCTL_HID_GET_COLLECTION_DESCRIPTOR,
		NULL,
		&desc,
		NULL,
		NULL);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
			"%!FUNC! WdfIoTargetSendIoctlSynchronously failed, %!STATUS!", status);

		goto CLEANUP;
	}

	//	 Get caps
	HIDP_CAPS caps;
	status = HidP_GetCaps(pPreparsedData, &caps);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
			"%!FUNC! HidP_GetCaps failed, %!STATUS!", status);

		goto CLEANUP;
	}

	deviceContext->featureReportLength = caps.FeatureReportByteLength;
	deviceContext->inputReportLength = caps.InputReportByteLength;
	deviceContext->outputReportLength = caps.OutputReportByteLength;

CLEANUP:
	if (hMemory != NULL)
		WdfObjectDelete(hMemory);

	return status;
}

NTSTATUS ds4_xusbGetFeature(WDFIOTARGET ioTarget, WDFMEMORY hMemory)
{
	NTSTATUS status = STATUS_SUCCESS;

	/*WDF_OBJECT_ATTRIBUTES reqAttr;
	WDF_OBJECT_ATTRIBUTES_INIT(&reqAttr);
	reqAttr.ParentObject = ioTarget;

	WDFREQUEST request = NULL;
	status = WdfRequestCreate(&reqAttr, ioTarget, &request);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
			"%!FUNC!, WdfRequestCreate failed, %!STATUS!\n",
			status);

		goto CLEANUP;
	}*/

	WDF_MEMORY_DESCRIPTOR memDesc;
	WDF_MEMORY_DESCRIPTOR_INIT_HANDLE(
		&memDesc,
		hMemory,
		NULL
		);

	ULONG_PTR BytesRead = 0;
	status = WdfIoTargetSendIoctlSynchronously(ioTarget, NULL, IOCTL_HID_GET_FEATURE, &memDesc, &memDesc, NULL, &BytesRead);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
			"%!FUNC!, WdfIoTargetSendReadSynchronously failed, %!STATUS!\n",
			status);

		goto CLEANUP;
	}
	
CLEANUP:
	/*if (request != NULL)
		WdfObjectDelete(request);*/

	return status;
}

NTSTATUS ds4_xusbDeviceGetBusType(WDFDEVICE device, DS4_BUS_TYPE* busType)
{
	*busType = DS4_BUS_TYPE_UNKNOWN;
	NTSTATUS status = STATUS_SUCCESS;

	//PDEVICE_CONTEXT deviceContext = DeviceGetContext(device);

	//	Get PDO's name
	WDFMEMORY hMemory;
	status = WdfDeviceAllocAndQueryProperty(device,
		DevicePropertyHardwareID,
		NonPagedPool,
		WDF_NO_OBJECT_ATTRIBUTES,
		&hMemory);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
			"%!FUNC!, WdfDeviceAllocAndQueryProperty failed, %!STATUS!",
			status);
		goto CLEANUP;
	}

	UNICODE_STRING u_str;
	RtlInitUnicodeString(&u_str, WdfMemoryGetBuffer(hMemory, NULL));

	
	//RtlEqualUnicodeString
	if (RtlPrefixUnicodeString(&ds4_usb_hardwareID, &u_str, TRUE))
		*busType = DS4_BUS_TYPE_USB;
	else if (RtlPrefixUnicodeString(&ds4_bluetooth_hardwareID, &u_str, TRUE))
		*busType = DS4_BUS_TYPE_BLUETOOTH;
	else
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
			"%!FUNC!, unknown hardware ID  : %wZ", &u_str);
		status = STATUS_UNSUCCESSFUL;
	}

CLEANUP:
	//	Cleanup
	WdfObjectDelete(hMemory);

	return status;
}


NTSTATUS
ds4_xusbInterfaceChange(
_In_ PVOID NotificationStructure,
_Inout_opt_ PVOID Context
)
{
	NTSTATUS status = STATUS_SUCCESS;

	PDEVICE_INTERFACE_CHANGE_NOTIFICATION pNotification = (PDEVICE_INTERFACE_CHANGE_NOTIFICATION)NotificationStructure;
	WDFDEVICE device = (WDFDEVICE)Context;

	PDEVICE_CONTEXT deviceContext = DeviceGetContext(device);

	//	Acquire lock
	//WdfSpinLockAcquire(deviceContext->initializationLock);

	if (!memcmp(&pNotification->Event, &GUID_DEVICE_INTERFACE_ARRIVAL, sizeof(GUID)))
	{
		if (deviceContext->targetOpened == FALSE)
		{
			status = ds4_xusbOpenHidIoTarget(device);

			//	Get underlying bus type
			if (NT_SUCCESS(status))
				status = ds4_xusbDeviceGetBusType(device, &deviceContext->busType);

			//	Get reports size
			if (NT_SUCCESS(status))
				status = ds4_xusbInitReportSize(device, deviceContext, deviceContext->remoteIOTarget);
				
			//	Only needed for bluetooth devices, to get the full input report
			if (NT_SUCCESS(status) && deviceContext->busType == DS4_BUS_TYPE_BLUETOOTH)
			{
				//	Allocate memory
				WDFMEMORY hMemory = NULL;

				WDF_OBJECT_ATTRIBUTES memAttr;
				WDF_OBJECT_ATTRIBUTES_INIT(&memAttr);
				memAttr.ParentObject = device;

				PUCHAR buffer = NULL;
				status = WdfMemoryCreate(&memAttr, NonPagedPool, 0, deviceContext->featureReportLength, &hMemory, &buffer);
				if (NT_SUCCESS(status))
				{
					//	Feature report ID 2
					*buffer = 2;
					status = ds4_xusbGetFeature(deviceContext->remoteIOTarget, hMemory);
				}
			}

			if (NT_SUCCESS(status))
			{
				if (deviceContext->busType == DS4_BUS_TYPE_BLUETOOTH)
				{
					status = ds4_xusbBthSendOutputRequest(deviceContext->remoteIOTarget, DS4_OUT_CMD_TYPE_LED, deviceContext->ledR, deviceContext->ledG, deviceContext->ledB, 0, 0);
				}
				else
				{
					//	No need to check error since status succeeded
					status = ds4_xusbUsbSendOutputRequest(deviceContext->remoteIOTarget, DS4_OUT_CMD_TYPE_LED, deviceContext->ledR, deviceContext->ledG, deviceContext->ledB, 0, 0);
				}
			}
		}
		if (deviceContext->notificationEntry != NULL)
		{
			IoUnregisterPlugPlayNotificationEx(deviceContext->notificationEntry);
			deviceContext->notificationEntry = NULL;
		}
	}


	//	Notify of failure
	if (!NT_SUCCESS(status))
		WdfDeviceSetFailed(device, WdfDeviceFailedAttemptRestart);

	return status;
}

/*
NTSTATUS ds4_xusbCreateTouchpadDevice(WDFDEVICE parentDevice)
{
	NTSTATUS status = STATUS_SUCCESS;
	WDFDEVICE childDevice = NULL;

	PWDFDEVICE_INIT pDeviceInit = WdfPdoInitAllocate(parentDevice);
	if (pDeviceInit == NULL)
	{
		goto Cleanup;
	}

	status = WdfDeviceCreate(&pDeviceInit,
		WDF_NO_OBJECT_ATTRIBUTES,
		&childDevice);

	if (!NT_SUCCESS(status))
	{
		goto Cleanup;
	}
	
	//
	// Provide DeviceID, HardwareIDs, CompatibleIDs and InstanceId
	//

	status = WdfPdoInitAssignDeviceID(pDeviceInit, &ds4_usb_touchpad_hardwareID);
	if (!NT_SUCCESS(status))
	{
		goto Cleanup;
	}

	//
	// Note same string  is used to initialize hardware id too
	//
	status = WdfPdoInitAddHardwareID(pDeviceInit, &ds4_usb_touchpad_hardwareID);
	if (!NT_SUCCESS(status)) {
		goto Cleanup;
	}


	status = WdfPdoInitAssignInstanceID(pDeviceInit, &ds4_usb_touchpad_instanceID);
	if (!NT_SUCCESS(status))
	{
		goto Cleanup;
	}


	//
	// You can call WdfPdoInitAddDeviceText multiple times, adding device
	// text for multiple locales. When the system displays the text, it
	// chooses the text that matches the current locale, if available.
	// Otherwise it will use the string for the default locale.
	// The driver can specify the driver's default locale by calling
	// WdfPdoInitSetDefaultLocale.
	//
	status = WdfPdoInitAddDeviceText(pDeviceInit,
		&ds4_usb_touchpad_desc,
		&ds4_touchpad_location,
		0x409);
	if (!NT_SUCCESS(status))
	{
		goto Cleanup;
	}

	WdfPdoInitSetDefaultLocale(pDeviceInit, 0x409);

	//
	// Set some properties for the child device.
	//
	WDF_DEVICE_PNP_CAPABILITIES pnpCaps;
	WDF_DEVICE_PNP_CAPABILITIES_INIT(&pnpCaps);
	pnpCaps.Removable = WdfTrue;
	pnpCaps.EjectSupported = WdfFalse;
	pnpCaps.SurpriseRemovalOK = WdfTrue;

	pnpCaps.Address = 1;
	pnpCaps.UINumber = 1;

	WdfDeviceSetPnpCapabilities(childDevice, &pnpCaps);

	WDF_DEVICE_POWER_CAPABILITIES powerCaps;
	WDF_DEVICE_POWER_CAPABILITIES_INIT(&powerCaps);
	
	WdfDeviceSetPowerCapabilities(childDevice, &powerCaps);


	status = WdfFdoAddStaticChild(parentDevice, childDevice);
	if (!NT_SUCCESS(status))
	{
		goto Cleanup;
	}

	return status;

Cleanup:
	//
	// Call WdfDeviceInitFree if you encounter an error before the
	// device is created. Once the device is created, framework
	// NULLs the pDeviceInit value.
	//
	if (pDeviceInit != NULL) {
		WdfDeviceInitFree(pDeviceInit);
	}

	if (childDevice) {
		WdfObjectDelete(childDevice);
	}

	return status;
}*/

#define MAX_ID_LEN 80

#define BUSENUM_COMPATIBLE_IDS L"vcvcv{B85B7C50-6A01-11d2-B841-00C04FAD5171}\\MsCompatibleToaster\0"
#define BUSENUM_COMPATIBLE_IDS_LENGTH sizeof(BUSENUM_COMPATIBLE_IDS)

#define BUS_HARDWARE_IDS L"vcvcv{B85B7C50-6A01-11d2-B841-00C04FAD5171}\\MsToaster\0"
#define BUS_HARDWARE_IDS_LENGTH sizeof (BUS_HARDWARE_IDS)

NTSTATUS
Bus_CreatePdo(
_In_ WDFDEVICE  Device,
_In_ PWSTR      HardwareIds,
_In_ ULONG      SerialNo
)
{
	NTSTATUS                    status;
	PWDFDEVICE_INIT             pDeviceInit = NULL;
	//PPDO_DEVICE_DATA            pdoData = NULL;
	WDFDEVICE                   hChild = NULL;
	//WDF_QUERY_INTERFACE_CONFIG  qiConfig;
	//WDF_OBJECT_ATTRIBUTES       pdoAttributes;
	//WDF_DEVICE_PNP_CAPABILITIES pnpCaps;
	//WDF_DEVICE_POWER_CAPABILITIES powerCaps;
	//TOASTER_INTERFACE_STANDARD  ToasterInterface;
	DECLARE_CONST_UNICODE_STRING(compatId, BUSENUM_COMPATIBLE_IDS);
	DECLARE_CONST_UNICODE_STRING(deviceLocation, L"Toaster Bus 0");
	UNICODE_STRING deviceId;
	DECLARE_UNICODE_STRING_SIZE(buffer, MAX_ID_LEN);

	KdPrint(("BusEnum: Entered Bus_CreatePdo\n"));

	PAGED_CODE();

	//
	// Allocate a WDFDEVICE_INIT structure and set the properties
	// so that we can create a device object for the child.
	//
	pDeviceInit = WdfPdoInitAllocate(Device);

	if (pDeviceInit == NULL) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
			"%!FUNC!, WdfPdoInitAllocate failed");
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto Cleanup;
	}

	//
	// Set DeviceType
	//
	WdfDeviceInitSetDeviceType(pDeviceInit, FILE_DEVICE_BUS_EXTENDER);

	//
	// Provide DeviceID, HardwareIDs, CompatibleIDs and InstanceId
	//
	RtlInitUnicodeString(&deviceId, HardwareIds);

	status = WdfPdoInitAssignDeviceID(pDeviceInit, &deviceId);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
			"%!FUNC!, WdfPdoInitAssignDeviceID failed, %!STATUS!\n",
			status);
		goto Cleanup;
	}

	//
	// Note same string  is used to initialize hardware id too
	//
	status = WdfPdoInitAddHardwareID(pDeviceInit, &deviceId);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
			"%!FUNC!, WdfPdoInitAddHardwareID failed, %!STATUS!\n",
			status);
		goto Cleanup;
	}

	status = WdfPdoInitAddCompatibleID(pDeviceInit, &compatId);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
			"%!FUNC!, WdfPdoInitAddCompatibleID failed, %!STATUS!\n",
			status);
		goto Cleanup;
	}

	status = RtlUnicodeStringPrintf(&buffer, L"%02d", SerialNo);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
			"%!FUNC!, RtlUnicodeStringPrintf failed, %!STATUS!\n",
			status);
		goto Cleanup;
	}

	status = WdfPdoInitAssignInstanceID(pDeviceInit, &buffer);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
			"%!FUNC!, WdfPdoInitAssignInstanceID failed, %!STATUS!\n",
			status);
		goto Cleanup;
	}

	//
	// Provide a description about the device. This text is usually read from
	// the device. In the case of USB device, this text comes from the string
	// descriptor. This text is displayed momentarily by the PnP manager while
	// it's looking for a matching INF. If it finds one, it uses the Device
	// Description from the INF file or the friendly name created by
	// coinstallers to display in the device manager. FriendlyName takes
	// precedence over the DeviceDesc from the INF file.
	//
	status = RtlUnicodeStringPrintf(&buffer, L"Microsoft_Eliyas_Toaster_%02d", SerialNo);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
			"%!FUNC!, RtlUnicodeStringPrintf failed, %!STATUS!\n",
			status);
		goto Cleanup;
	}

	//
	// You can call WdfPdoInitAddDeviceText multiple times, adding device
	// text for multiple locales. When the system displays the text, it
	// chooses the text that matches the current locale, if available.
	// Otherwise it will use the string for the default locale.
	// The driver can specify the driver's default locale by calling
	// WdfPdoInitSetDefaultLocale.
	//
	status = WdfPdoInitAddDeviceText(pDeviceInit,
		&buffer,
		&deviceLocation,
		0x409);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
			"%!FUNC!, WdfPdoInitAddDeviceText failed, %!STATUS!\n",
			status);
		goto Cleanup;
	}

	WdfPdoInitSetDefaultLocale(pDeviceInit, 0x409);

	//
	// Initialize the attributes to specify the size of PDO device extension.
	// All the state information private to the PDO will be tracked here.
	//
	//WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&pdoAttributes, PDO_DEVICE_DATA);

	status = WdfDeviceCreate(&pDeviceInit, WDF_NO_OBJECT_ATTRIBUTES, &hChild);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
			"%!FUNC!, WdfDeviceCreate failed, %!STATUS!\n",
			status);
		goto Cleanup;

	}

	//
	// Once the device is created successfully, framework frees the
	// DeviceInit memory and sets the pDeviceInit to NULL. So don't
	// call any WdfDeviceInit functions after that.
	//
	// Get the device context.
	//
	//pdoData = PdoGetData(hChild);

	//pdoData->SerialNo = SerialNo;

	//
	// Set some properties for the child device.
	//
	/*WDF_DEVICE_PNP_CAPABILITIES_INIT(&pnpCaps);
	pnpCaps.Removable = WdfTrue;
	pnpCaps.EjectSupported = WdfTrue;
	pnpCaps.SurpriseRemovalOK = WdfTrue;

	pnpCaps.Address = SerialNo;
	pnpCaps.UINumber = SerialNo;

	WdfDeviceSetPnpCapabilities(hChild, &pnpCaps);

	WDF_DEVICE_POWER_CAPABILITIES_INIT(&powerCaps);

	powerCaps.DeviceD1 = WdfTrue;
	powerCaps.WakeFromD1 = WdfTrue;
	powerCaps.DeviceWake = PowerDeviceD1;

	powerCaps.DeviceState[PowerSystemWorking] = PowerDeviceD0;
	powerCaps.DeviceState[PowerSystemSleeping1] = PowerDeviceD1;
	powerCaps.DeviceState[PowerSystemSleeping2] = PowerDeviceD3;
	powerCaps.DeviceState[PowerSystemSleeping3] = PowerDeviceD3;
	powerCaps.DeviceState[PowerSystemHibernate] = PowerDeviceD3;
	powerCaps.DeviceState[PowerSystemShutdown] = PowerDeviceD3;

	WdfDeviceSetPowerCapabilities(hChild, &powerCaps);*/

	//
	// Create a custom interface so that other drivers can
	// query (IRP_MN_QUERY_INTERFACE) and use our callbacks directly.
	//
	/*RtlZeroMemory(&ToasterInterface, sizeof(ToasterInterface));

	ToasterInterface.InterfaceHeader.Size = sizeof(ToasterInterface);
	ToasterInterface.InterfaceHeader.Version = 1;
	ToasterInterface.InterfaceHeader.Context = (PVOID)hChild;

	//
	// Let the framework handle reference counting.
	//
	ToasterInterface.InterfaceHeader.InterfaceReference =
		WdfDeviceInterfaceReferenceNoOp;
	ToasterInterface.InterfaceHeader.InterfaceDereference =
		WdfDeviceInterfaceDereferenceNoOp;

	ToasterInterface.GetCrispinessLevel = Bus_GetCrispinessLevel;
	ToasterInterface.SetCrispinessLevel = Bus_SetCrispinessLevel;
	ToasterInterface.IsSafetyLockEnabled = Bus_IsSafetyLockEnabled;

	WDF_QUERY_INTERFACE_CONFIG_INIT(&qiConfig,
		(PINTERFACE)&ToasterInterface,
		&GUID_TOASTER_INTERFACE_STANDARD,
		NULL);
	//
	// If you have multiple interfaces, you can call WdfDeviceAddQueryInterface
	// multiple times to add additional interfaces.
	//
	status = WdfDeviceAddQueryInterface(hChild, &qiConfig);
	if (!NT_SUCCESS(status)) {
		goto Cleanup;

	}*/

	//
	// Add this device to the FDO's collection of children.
	// After the child device is added to the static collection successfully,
	// driver must call WdfPdoMarkMissing to get the device deleted. It
	// shouldn't delete the child device directly by calling WdfObjectDelete.
	//
	status = WdfFdoAddStaticChild(Device, hChild);
	if (!NT_SUCCESS(status)) {
		goto Cleanup;
	}

	return status;

Cleanup:
	TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
		"%!FUNC! Cleanup");

	//
	// Call WdfDeviceInitFree if you encounter an error before the
	// device is created. Once the device is created, framework
	// NULLs the pDeviceInit value.
	//
	if (pDeviceInit != NULL) {
		WdfDeviceInitFree(pDeviceInit);
	}

	if (hChild) {
		WdfObjectDelete(hChild);
	}

	return status;
}


NTSTATUS
ds4_xusbCreateDevice(WDFDRIVER driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
{
    WDF_OBJECT_ATTRIBUTES   deviceAttributes;
    PDEVICE_CONTEXT deviceContext;
    WDFDEVICE device;
    NTSTATUS status;

    PAGED_CODE();

	// Configure the device as a filter driver
	WdfFdoInitSetFilter(DeviceInit);

	//WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_BUS_EXTENDER);
	//WdfDeviceInitSetExclusive(DeviceInit, TRUE);


    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);
	//	no automatic synchronization
	deviceAttributes.SynchronizationScope = WdfSynchronizationScopeNone;

    status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);

    if (NT_SUCCESS(status))
	{
        //
        // Get a pointer to the device context structure that we just associated
        // with the device object. We define this structure in the device.h
        // header file. DeviceGetContext is an inline function generated by
        // using the WDF_DECLARE_CONTEXT_TYPE_WITH_NAME macro in device.h.
        // This function will do the type checking and return the device context.
        // If you pass a wrong object handle it will return NULL and assert if
        // run under framework verifier mode.
        //
        deviceContext = DeviceGetContext(device);

		memset(deviceContext, 0, sizeof(DEVICE_CONTEXT));

		deviceContext->ledState = XINPUT_LED_OFF;

		deviceContext->reportCount = 0;
		deviceContext->targetOpened = FALSE;
		deviceContext->remoteIOTarget = NULL;
		deviceContext->notificationEntry = NULL;
		deviceContext->busType = DS4_BUS_TYPE_UNKNOWN;

		//	Create IO target
		WDF_OBJECT_ATTRIBUTES ioTargetAttr;
		WDF_OBJECT_ATTRIBUTES_INIT(&ioTargetAttr);
		ioTargetAttr.ParentObject = device;

		status = WdfIoTargetCreate(device,
			WDF_NO_OBJECT_ATTRIBUTES,
			&deviceContext->remoteIOTarget);
		if (!NT_SUCCESS(status))
		{
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
				"%!FUNC!, WdfIoTargetCreate failed, %!STATUS!\n",
				status);
			return STATUS_UNSUCCESSFUL;
		}


		//	Get led color from registry or set default value if property doesnt exists
		ULONG requiredSize = 0;
		DEVPROPTYPE type;

		UCHAR ledColor[4] = { 0 };
				
		status = IoGetDevicePropertyData(WdfDeviceWdmGetPhysicalDevice(device), &DS4_XUSB_DEVPROP_LEDCOLOR, LOCALE_NEUTRAL, 0, 4, ledColor, &requiredSize, &type);
		if (status == STATUS_OBJECT_NAME_NOT_FOUND)
		{
			ledColor[0] = 0x20;
			ledColor[1] = 0;
			ledColor[2] = 0xFF;

			status = IoSetDevicePropertyData(WdfDeviceWdmGetPhysicalDevice(device), &DS4_XUSB_DEVPROP_LEDCOLOR, LOCALE_NEUTRAL, 0, DEVPROP_TYPE_UINT32, 4, ledColor);
		}

		deviceContext->ledR = ledColor[0];
		deviceContext->ledG = ledColor[1];
		deviceContext->ledB = ledColor[2];

		if (!NT_SUCCESS(status))
		{
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
				"%!FUNC!, IoGetDevicePropertyData/IoSetDevicePropertyData failed, %!STATUS!\n",
				status);
			return STATUS_UNSUCCESSFUL;
		}
		

		/*
		Register for interface change. So we get notified when the interface is available
		(that means the driver stack is fully started and we can open io target to the PDO)
		*/
		status = IoRegisterPlugPlayNotification(EventCategoryDeviceInterfaceChange,
			0,
			(PVOID)&XUSB_INTERFACE_CLASS_GUID,
			WdfDriverWdmGetDriverObject(driver),
			ds4_xusbInterfaceChange,
			device,
			&deviceContext->notificationEntry);
		if (!NT_SUCCESS(status))
		{
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
				"%!FUNC!, IoRegisterPlugPlayNotification failed, %!STATUS!\n",
				status);
			return STATUS_UNSUCCESSFUL;
		}

        //
        // Create a device interface so that applications can find and talk
        // to us.
        //
        status = WdfDeviceCreateDeviceInterface(
            device,
            &XUSB_INTERFACE_CLASS_GUID,
            NULL // ReferenceString
            );

        if (NT_SUCCESS(status)) {
            //
            // Initialize the I/O Package and any Queues
            //
            status = ds4_xusbQueueInitialize(device);
        }


		//	Initialize bus enumerator
		PNP_BUS_INFORMATION busInfo;
		busInfo.BusTypeGuid = GUID_DEVCLASS_DS4_CONTROLLER;
		busInfo.LegacyBusType = PNPBus;
		busInfo.BusNumber = 0;

		WdfDeviceSetBusInformationForChildren(device, &busInfo);

		//status = ds4_xusbCreateTouchpadDevice(device);
		status = Bus_CreatePdo(device, BUS_HARDWARE_IDS, 1);
		if (!NT_SUCCESS(status))
		{
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
				"%!FUNC!, Bus_CreatePdo failed, %!STATUS!\n",
				status);
			return STATUS_UNSUCCESSFUL;
		}
    }

    return status;
}

