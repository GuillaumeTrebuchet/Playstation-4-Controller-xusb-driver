// test hid.cpp : Defines the entry point for the console application.
//
//#include <boost/crc.hpp>
//#include <boost/cstdint.hpp>
#include <algorithm>
#include <fstream>
#include <memory>
#include <vector>
#include <string>
#include <iostream>

#include <Windows.h>
#include <SetupAPI.h>
#include <hidsdi.h>

#include "winioctl.h"

#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)

#include <Hidpi.h>
//#include <C:\Program Files (x86)\Windows Kits\8.1\Include\km\hidport.h>

#define IOCTL_DS4_SET_BUTTON_MAPPING CTL_CODE(FILE_DEVICE_KEYBOARD, 0x801, METHOD_IN_DIRECT, FILE_ANY_ACCESS)

std::vector<const wchar_t*> hardwareIDList =
{
	L"HID\\VID_054C&PID_05C4",
	L"HID\\{00001124-0000-1000-8000-00805F9B34FB}_VID&0002054C_PID&05C4"
};

HANDLE OpenDS4Handle()
{
	HANDLE hDevice = NULL;
	DWORD requiredLength = 0;
	SP_DEVINFO_DATA devInfoData;
	SP_DEVICE_INTERFACE_DATA devInterfaceData;

	GUID hidGuid;
	HidD_GetHidGuid(&hidGuid);

	//
	// Open a handle to the plug and play dev node.
	//
	HDEVINFO hDevInfo = SetupDiGetClassDevsW(&hidGuid,
		NULL, // Define no enumerator (global)
		NULL, // Define no
		(DIGCF_PRESENT | // Only Devices present
			DIGCF_DEVICEINTERFACE)); // Function class devices.

	if (INVALID_HANDLE_VALUE == hDevInfo)
		return NULL;

	int i = 0;
	while (true)
	{
		devInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

		//	Enumerate HID devices
		BOOL b = SetupDiEnumDeviceInterfaces(hDevInfo,
			0, // No care about specific PDOs
			&hidGuid,
			i,
			&devInterfaceData);
		DWORD err = GetLastError();
		if (!b)
			break;


		//	Get instance ID
		devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
		SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData);

		WCHAR instanceID[MAX_PATH] = { 0 };
		SetupDiGetDeviceInstanceId(hDevInfo, &devInfoData, instanceID, MAX_PATH, &requiredLength);
		std::wstring sInstanceID = instanceID;

		//std::wcout << L"HID device found : " << sInstanceID << std::endl;

		//	Check instance ID
		for (auto it = hardwareIDList.begin(); it != hardwareIDList.end(); ++it)
		{
			if (sInstanceID.find(*it) == 0)	// test hardware ID
			{

				//	Get interface detail
				SetupDiGetDeviceInterfaceDetailW(
					hDevInfo,
					&devInterfaceData,
					NULL,
					0,
					&requiredLength,
					NULL);

				std::vector<unsigned char> pDetailDataBuffer(requiredLength);
				PSP_DEVICE_INTERFACE_DETAIL_DATA pDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)&pDetailDataBuffer[0];
				pDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

				BOOL b = SetupDiGetDeviceInterfaceDetailW(
					hDevInfo,
					&devInterfaceData,
					pDetailData,
					pDetailDataBuffer.size(),
					&requiredLength,
					NULL);


				//	Create handle
				hDevice = CreateFile(pDetailData->DevicePath,
					GENERIC_READ | GENERIC_WRITE,
					FILE_SHARE_READ | FILE_SHARE_WRITE,
					NULL,        // no SECURITY_ATTRIBUTES structure
					OPEN_EXISTING, // No special create flags
					0,   // Open device as non-overlapped so we can get data
					NULL);

				int qzdqd = GetLastError();
				break;
			}
		}

		if (hDevice != NULL)
			break;
		++i;
	}

	SetupDiDestroyDeviceInfoList(hDevInfo);

	return hDevice;
}
#pragma pack(push,1)

struct DS4_OutReport
{
	uint8_t bluetooth_hid_hdr[2];
	uint8_t unknown1[2];
	uint8_t rumbleEnable;
	uint8_t unknown2[2];
	uint8_t rumbleRight;
	uint8_t rumbleLeft;
	uint8_t RGB[3];
	uint8_t unknown3[63];
	uint32_t CRC32;
};
#pragma pack(pop)

/*
vibrationEnable :
1 => vibration
2 => light flash
4 => orange fade in fade out
*/

/*
unknown0 :
1 => led flash (light off?)
2 => led on

*/

enum class DS4_OutputCommandType
	: UCHAR
{
	Vibration = 0x1,
	Led = 0x2,
	LedFadeInOut = 0x4,
};
#pragma pack(push, 1)

//	Report ID 5
/*typedef struct _DS4_USB_OUTPUT_REPORT
{
	UCHAR	reportID;
	UCHAR				cmdType;		//	can be ORed
	UCHAR				ledEnable;
	UCHAR				unknown1;
	UCHAR				rumbleLeft;
	UCHAR				rumbleRight;
	UCHAR				red;
	UCHAR				green;
	UCHAR				blue;
	UCHAR				fadeInOutEnable;
	UCHAR				unknown[22];
} DS4_USB_OUTPUT_REPORT, * PDS4_USB_OUTPUT_REPORT;*/
typedef struct _DS4_USB_OUTPUT_REPORT
{
	UCHAR	reportID;
	UCHAR				cmdType;		//	can be ORed
	UCHAR				ledEnable;
	UCHAR				unknown1;
	UCHAR				rumbleLeft;
	UCHAR				rumbleRight;
	UCHAR				red;
	UCHAR				green;
	UCHAR				blue;
	UCHAR				fadeInOutEnable;
	UCHAR				unknown[22];
} DS4_USB_OUTPUT_REPORT, * PDS4_USB_OUTPUT_REPORT;

struct DS4_bth_outputReport
{
	UCHAR	reportID;
	UCHAR	flags;		// must be 0x80
	UCHAR	unknown0;
	UCHAR	cmdType;	// same as usb
	UCHAR	unknown2[2];
	UCHAR	rumbleRight;
	UCHAR	rumbleLeft;
	UCHAR	R;
	UCHAR	G;
	UCHAR	B;
	UCHAR	unknown1[67];
};
#pragma pack(pop)
/*
input :
report id 1
size : 547

id 17
size 547


output :
id unknown
length 78
8 = R
9 = G
10 = B
*/
#define TEST_VALUE 0x20
void testOutput(HANDLE hDevice)
{
	///!\ WORKING BLUETOOTH
	std::vector<UCHAR> buffer(547);
	//DS4_bth_outputReport& report = *(DS4_bth_outputReport*)&buffer[0];
	_DS4_USB_OUTPUT_REPORT report = { 0 };
	report.reportID = 0x5;

	//report.flags = 0x80;
	//report.unknown0 = 0xFF;
	report.cmdType = 3;

	report.rumbleRight = 0xFF;
	report.rumbleLeft = 0xFF;

	//for (int i = 0; i < 67; i++)
	//	report.unknown1[i] = 0xFF;

	report.red = 255;
	report.green = 0;
	report.blue = 0;

	//memset(&buffer[1], 0xFF, 546);

	DWORD BytesWritten = 0;
	BOOL b = WriteFile(hDevice, &report, sizeof(report), &BytesWritten, NULL);
	int err = GetLastError();
	//BOOLEAN b = HidD_SetOutputReport(hDevice, &report, 547);
	int qdqdoo = 0;

	//DS4_USB_OUTPUT_REPORT report = { 0 };
	/*UCHAR report[547] = { 0 };
	report[0] = 5;


	memset(report, 0xFF, sizeof(report));

	BOOLEAN b = HidD_SetOutputReport(hDevice, &report, sizeof(report));
	//DWORD BytesWritten = 0;
	//BOOL b = WriteFile(hDevice, &report, sizeof(report), &BytesWritten, NULL);

	int qzdqzd = 0;*/
	/*std::vector<BYTE> buffer =
	{
		0x52, 0x11,
		0xB0, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};

	DWORD BytesWritten = 0;
	BOOL b = WriteFile(hDevice, &buffer[0], buffer.size(), &BytesWritten, NULL);

	int qzdqd = 0;*/
	/*PHIDP_PREPARSED_DATA pPreparsedData = NULL;
	BOOL b = HidD_GetPreparsedData(hDevice, &pPreparsedData);

	HIDP_CAPS caps;
	NTSTATUS status = HidP_GetCaps(pPreparsedData, &caps);

	int qzdqd = 0;*/
	/*DS4_OutputReport report = { 0 };
	report.reportID = 5;

	//for (int i = 1; i < 22; ++i)
		//report.unknown[i] = TEST_VALUE;

	report.red = 0xFF;
	report.green = 0xFF;
	report.blue = 0xFF;

	//report.unknown1 = TEST_VALUE;

	report.fadeInOutEnable = 1;

	report.ledEnable = 2;
	report.cmdType = 0x2;
	report.rumbleRight = 0xff;
	report.rumbleLeft = 0xff;

	DWORD BytesWritten = 0;
	BOOL b = WriteFile(hDevice, &report, sizeof(DS4_OutputReport), &BytesWritten, NULL);

	int a = 0;
	while (true)
	{
		report.red = report.green = report.blue = 0;

		if (a % 3 == 0)
			report.red = 0xFF;
		else if (a % 3 == 1)
			report.green = 0xFF;
		else
			report.blue = 0xFF;

		b = WriteFile(hDevice, &report, sizeof(DS4_OutputReport), &BytesWritten, NULL);

		Sleep(50);
		++a;
	}*/

	int qdqd = 0;
}


struct DS4_FeatureReport4
{
	UCHAR reportID;
	UCHAR unknown[512];
};

void testFeature(HANDLE hDevice)
{
	for (int i = 1; i < 512; ++i)
	{
		DS4_FeatureReport4 report = { 0 };
		report.reportID = 8;

		//DWORD BytesRead = 0;
		//BOOL b = ReadFile(hDevice, &report, sizeof(DS4_FeatureReport4), &BytesRead, NULL);
		BOOL b = HidD_GetFeature(hDevice, &report, i);
		if (b)
		{
			int qzdqdqdz = 0;
		}
	}
	int qzdqd = 0;
}


struct DS4_BLUETOOTH_INPUT_REPORT
{
	UCHAR reportID;
	UCHAR X;
	UCHAR Y;
	UCHAR Z;
	UCHAR Rz;
	UCHAR Buttons[3];
	UCHAR LeftTrigger;
	UCHAR RightTrigger;
};
void testInput(HANDLE hDevice)
{

	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);

	double total = 0;

	//for (int i = 0; i < 1000; ++i)
	while (true)
	{
		//LARGE_INTEGER counter1;
		//QueryPerformanceCounter(&counter1);


		PHIDP_PREPARSED_DATA pPreparsedData = NULL;
		BOOL b = HidD_GetPreparsedData(hDevice, &pPreparsedData);

		HIDP_CAPS caps;
		NTSTATUS status = HidP_GetCaps(pPreparsedData, &caps);

		//std::vector<CHAR> reportBuffer(caps.InputReportByteLength);

		/*HIDP_DATA dataBuffer[500];
		ULONG length = 500;

		status = HidP_GetData(HidP_Input, &dataBuffer[0], &length, pPreparsedData, &reportBuffer[0], caps.InputReportByteLength);*/
		std::vector<CHAR> featureBuffer(caps.FeatureReportByteLength);
		ZeroMemory(&featureBuffer[0], featureBuffer.size());
		featureBuffer[0] = 0x2;

		b = HidD_GetFeature(hDevice, &featureBuffer[0], featureBuffer.size());


		std::vector<CHAR> reportBuffer(caps.InputReportByteLength);
		ZeroMemory(&reportBuffer[0], reportBuffer.size());
		reportBuffer[0] = 0x11;

		DWORD BytesRead = 0;
		//b = ReadFile(hDevice, &reportBuffer[0], reportBuffer.size() , &BytesRead, NULL);
		b = HidD_GetInputReport(hDevice, &reportBuffer[0], reportBuffer.size());

		//b = HidD_GetInputReport(hDevice, &reportBuffer[0], reportBuffer.size());
		/*int qzdqd = GetLastError();
		ULONG value = 0;
		status = HidP_GetUsageValue(HidP_Input, 0x0501, 0, 0x0930, &value, pPreparsedData, &reportBuffer[0], reportBuffer.size());
		int qdqhzdz = 0;*/
		std::cout << "button[2] : 0x" << std::hex << (UINT)((UCHAR)(reportBuffer[3])) << std::endl;

		//NTSTATUS e1 = HIDP_INVALID_REPORT_LENGTH;
		//NTSTATUS e1 = HIDP_INVALID_REPORT_TYPE;
		/*NTSTATUS e1 = HIDP_STATUS_INCOMPATIBLE_REPORT_ID;
		NTSTATUS e2 = HIDP_STATUS_INVALID_PREPARSED_DATA;
		NTSTATUS e3 = HIDP_STATUS_USAGE_NOT_FOUND;*/

		b = HidD_FreePreparsedData(pPreparsedData);

		/*UCHAR buffer[547] = { 0 };
		buffer[0] = 1;
		DWORD BytesRead = 0;
		BOOL b = ReadFile(hDevice, buffer, 547, &BytesRead, NULL);*/


		/*LARGE_INTEGER counter2;
		QueryPerformanceCounter(&counter2);

		LARGE_INTEGER elapsed;
		elapsed.QuadPart = counter2.QuadPart - counter1.QuadPart;

		double elapsedUs = elapsed.QuadPart / ((double)frequency.QuadPart / 1000000);
		//std::cout << "Elapsed " << elapsedUs << "us" << std::endl;

		total += elapsedUs / 1000;*/
	}

	std::cout << "total = " << total << std::endl;
	std::string s;
	std::cin >> s;
	int qzdqzd = 0;
}
#define HID_CTL_CODE(id)            CTL_CODE(FILE_DEVICE_KEYBOARD, (id), METHOD_NEITHER, FILE_ANY_ACCESS)
#define IOCTL_HID_GET_REPORT_DESCRIPTOR             HID_CTL_CODE(1)

class HID_Value
{
public:
	int index;
	int size;

	USAGE usage;
	USAGE usagePage;

	LONG logicalMin;
	LONG logicalMax;

	LONG physicalMin;
	LONG physicalMax;
};

#define HID_USAGE_PAGE_GENERIC_DESKTOP (0x01)

#define HID_USAGE_GENERIC_DESKTOP_X (0x30)
#define HID_USAGE_GENERIC_DESKTOP_Y (0x31)
#define HID_USAGE_GENERIC_DESKTOP_Z (0x32)


HID_Value FindUsage(USHORT usagePage, USHORT usageID, const std::vector<HIDP_VALUE_CAPS>& valueCaps)
{
	HID_Value value;

	for (int i = 0; i < valueCaps.size(); ++i)
	{
		const HIDP_VALUE_CAPS& caps = valueCaps[i];
		if (caps.IsRange == true)
			throw std::exception("Range not supported");

		if (caps.NotRange.Usage == usageID && caps.UsagePage == usagePage)
		{
			value.index = caps.NotRange.DataIndex;
			value.size = caps.BitSize;
			value.usage = usageID;
			value.usagePage = usagePage;

			value.logicalMin = caps.LogicalMin;
			value.logicalMax = caps.LogicalMax;

			value.physicalMin = caps.PhysicalMin;
			value.physicalMax = caps.PhysicalMax;

			return value;
		}
	}

	throw std::exception("Usage not found");
}

ULONG GetValue(const HID_Value& v, PHIDP_PREPARSED_DATA pPreparsedData, const std::vector<CHAR>& report)
{
	ULONG value = 0;
	NTSTATUS status = HidP_GetUsageValue(HIDP_REPORT_TYPE::HidP_Input, v.usagePage, 0, v.usage, &value, pPreparsedData, (PCHAR)&report[0], report.size());
	if (!NT_SUCCESS(status))
	{
		throw std::exception();
	}

	return value;
}

/*std::ostream& operator<< (std::ostream& s, bool b)
{
	if (b)
		s << "true";
	else
		s << "false";

	return s;
}*/
void DumpCapsData(const std::vector<HIDP_VALUE_CAPS>& valueCapsBuffer)
{
	std::ofstream myfile;
	myfile.open("bluetooth_caps_dump.txt");

	for (int i = 0; i < valueCapsBuffer.size(); ++i)
	{
		const HIDP_VALUE_CAPS& valueCaps = valueCapsBuffer[i];

		myfile << std::endl << "Value caps " << i << std::endl << std::endl;

		myfile << "UsagePage : " << (int)valueCaps.UsagePage << std::endl;
		myfile << "ReportID : " << (int)valueCaps.ReportID << std::endl;
		myfile << "IsAlias : " << std::boolalpha << ((bool)valueCaps.IsAlias) << std::endl;
		myfile << "BitField : " << valueCaps.BitField << std::endl;
		myfile << "LinkCollection : " << valueCaps.LinkCollection << std::endl;
		myfile << "LinkUsage : " << (int)valueCaps.LinkUsage << std::endl;
		myfile << "LinkUsagePage : " << valueCaps.LinkUsagePage << std::endl;
		myfile << "IsRange : " << ((bool)valueCaps.IsRange) << std::endl;
		myfile << "IsStringRange : " << ((bool)valueCaps.IsStringRange) << std::endl;
		myfile << "IsDesignatorRange : " << ((bool)valueCaps.IsDesignatorRange) << std::endl;
		myfile << "IsAbsolute : " << ((bool)valueCaps.IsAbsolute) << std::endl;
		myfile << "HasNull : " << valueCaps.HasNull << std::endl;
		myfile << "Reserved : " << valueCaps.Reserved << std::endl;
		myfile << "BitSize : " << valueCaps.BitSize << std::endl;
		myfile << "ReportCount : " << valueCaps.ReportCount << std::endl;
		myfile << "Reserved2 : " << valueCaps.Reserved2 << std::endl;
		myfile << "UnitsExp : " << valueCaps.UnitsExp << std::endl;
		myfile << "Units : " << valueCaps.Units << std::endl;
		myfile << "LogicalMin : " << valueCaps.LogicalMin << std::endl;
		myfile << "LogicalMax : " << valueCaps.LogicalMax << std::endl;
		myfile << "PhysicalMin : " << valueCaps.PhysicalMin << std::endl;
		myfile << "PhysicalMax : " << valueCaps.PhysicalMax << std::endl;

		myfile << "Range : " << std::endl;
		myfile << "    " << "UsageMin : " << valueCaps.Range.UsageMin << std::endl;
		myfile << "    " << "UsageMax : " << valueCaps.Range.UsageMax << std::endl;
		myfile << "    " << "StringMin : " << valueCaps.Range.StringMin << std::endl;
		myfile << "    " << "StringMax : " << valueCaps.Range.StringMax << std::endl;
		myfile << "    " << "DesignatorMin : " << valueCaps.Range.DesignatorMin << std::endl;
		myfile << "    " << "DesignatorMax : " << valueCaps.Range.DesignatorMax << std::endl;
		myfile << "    " << "DataIndexMin : " << valueCaps.Range.DataIndexMin << std::endl;
		myfile << "    " << "DataIndexMax : " << valueCaps.Range.DataIndexMax << std::endl;

		myfile << "NotRange : " << std::endl;
		myfile << "    " << "Usage : " << valueCaps.NotRange.Usage << std::endl;
		myfile << "    " << "Reserved1 : " << valueCaps.NotRange.Reserved1 << std::endl;
		myfile << "    " << "StringIndex : " << valueCaps.NotRange.StringIndex << std::endl;
		myfile << "    " << "Reserved2 : " << valueCaps.NotRange.Reserved2 << std::endl;
		myfile << "    " << "DesignatorIndex : " << valueCaps.NotRange.DesignatorIndex << std::endl;
		myfile << "    " << "Reserved3 : " << valueCaps.NotRange.Reserved3 << std::endl;
		myfile << "    " << "DataIndex : " << valueCaps.NotRange.DataIndex << std::endl;
		myfile << "    " << "Reserved4 : " << valueCaps.NotRange.Reserved4 << std::endl;
	}
	myfile.close();
}
/*void OriEmulation(HANDLE hDevice)
{
	std::vector<HIDP_VALUE_CAPS> valueCapsBuffer;
	std::vector<CHAR> reportBuffer;

	//	Get preparsed data
	PHIDP_PREPARSED_DATA pPreparsedData = NULL;
	BOOL b = HidD_GetPreparsedData(hDevice, &pPreparsedData);
	if (!b)
	{
		std::cout << "HidD_GetPreparsedData failed with code 0x" << std::hex << GetLastError() << std::endl;
		goto CLEANUP;
	}

	//	Get caps
	HIDP_CAPS caps;
	NTSTATUS status = HidP_GetCaps(pPreparsedData, &caps);
	if (!NT_SUCCESS(status))
	{
		std::cout << "HidP_GetCaps failed with code 0x" << std::hex << status << std::endl;
		goto CLEANUP;
	}

	reportBuffer.resize(caps.InputReportByteLength);

	//	Get value caps
	valueCapsBuffer.resize(caps.NumberInputValueCaps);

	USHORT valueCapsLength = valueCapsBuffer.size();
	status = HidP_GetValueCaps(HIDP_REPORT_TYPE::HidP_Input, &valueCapsBuffer[0], &valueCapsLength, pPreparsedData);
	if (!NT_SUCCESS(status))
	{
		std::cout << "HidP_GetValueCaps failed with code 0x" << std::hex << status << std::endl;
		goto CLEANUP;
	}

	DumpCapsData(valueCapsBuffer);

	HID_Value X;
	HID_Value Y;

	try
	{
		X = FindUsage(HID_USAGE_PAGE_GENERIC_DESKTOP, HID_USAGE_GENERIC_DESKTOP_X, valueCapsBuffer);
		Y = FindUsage(HID_USAGE_PAGE_GENERIC_DESKTOP, HID_USAGE_GENERIC_DESKTOP_Y, valueCapsBuffer);
	}
	catch (std::exception e)
	{
		std::cout << e.what() << std::endl;
		goto CLEANUP;
	}

	while (true)
	{
		//	Read report
		ZeroMemory(&reportBuffer[0], reportBuffer.size());
		reportBuffer[0] = 1;

		DWORD BytesRead = 0;
		b = ReadFile(hDevice, &reportBuffer[0], reportBuffer.size(), &BytesRead, NULL);
		if (!b)
		{
			std::cout << "ReadFile failed with code 0x" << std::hex << GetLastError() << std::endl;
			continue;
		}

		try
		{
			ULONG x_value = GetValue(X, pPreparsedData, reportBuffer);
			ULONG y_value = GetValue(Y, pPreparsedData, reportBuffer);

			std::cout << "X value = 0x" << std::hex << x_value;
			std::cout << "; Y value = 0x" << std::hex << y_value << std::endl;
		}
		catch (std::exception e)
		{
			std::cout << e.what() << std::endl;
			continue;
		}

		int qzdd = 0;
	}


CLEANUP:
	if (pPreparsedData != NULL)
	{
		b = HidD_FreePreparsedData(pPreparsedData);
		if (!b)
		{
			std::cout << "HidD_FreePreparsedData failed with code 0x" << std::hex << GetLastError() << std::endl;
			return;
		}
	}
}*/


#include <initguid.h>
// {D61CA365-5AF4-4486-998B-9DB4734C6CA3}
DEFINE_GUID(GUID_DEVINTERFACE_XNACOMPOSITE,
	0xd61ca365, 0x5af4, 0x4486, 0x99, 0x8b, 0x9d, 0xb4, 0x73, 0x4c, 0x6c, 0xa3);

void PrintPresentInterface(GUID interfaceGuid)
{
	HANDLE hDevice = NULL;
	DWORD requiredLength = 0;
	SP_DEVINFO_DATA devInfoData;
	SP_DEVICE_INTERFACE_DATA devInterfaceData;

	//
	// Open a handle to the plug and play dev node.
	//
	HDEVINFO hDevInfo = SetupDiGetClassDevsW(&interfaceGuid,
		NULL, // Define no enumerator (global)
		NULL, // Define no
		(DIGCF_PRESENT | // Only Devices present
			DIGCF_DEVICEINTERFACE)); // Function class devices.

	if (INVALID_HANDLE_VALUE == hDevInfo)
	{
		std::cout << "SetupDiGetClassDevs failed." << std::endl;
		return;
	}

	int i = 0;
	while (true)
	{
		devInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

		//	Enumerate HID devices
		BOOL b = SetupDiEnumDeviceInterfaces(hDevInfo,
			0, // No care about specific PDOs
			&interfaceGuid,
			i,
			&devInterfaceData);
		if (!b)
			break;


		//	Get instance ID
		devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
		SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData);

		WCHAR instanceID[MAX_PATH] = { 0 };
		SetupDiGetDeviceInstanceId(hDevInfo, &devInfoData, instanceID, MAX_PATH, &requiredLength);
		std::wstring sInstanceID = instanceID;

		std::wcout << L"Instance found : " << sInstanceID << std::endl;

		//	Check instance ID
		/*for (auto it = hardwareIDList.begin(); it != hardwareIDList.end(); ++it)
		{
			if (sInstanceID.find(*it) == 0)	// test hardware ID
			{

				//	Get interface detail
				SetupDiGetDeviceInterfaceDetailW(
					hDevInfo,
					&devInterfaceData,
					NULL,
					0,
					&requiredLength,
					NULL);

				std::vector<unsigned char> pDetailDataBuffer(requiredLength);
				PSP_DEVICE_INTERFACE_DETAIL_DATA pDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)&pDetailDataBuffer[0];
				pDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

				BOOL b = SetupDiGetDeviceInterfaceDetailW(
					hDevInfo,
					&devInterfaceData,
					pDetailData,
					pDetailDataBuffer.size(),
					&requiredLength,
					NULL);

				int qzdqd = GetLastError();

				//	Create handle
				hDevice = CreateFile(pDetailData->DevicePath,
					GENERIC_READ | GENERIC_WRITE,
					0,
					NULL,        // no SECURITY_ATTRIBUTES structure
					OPEN_EXISTING, // No special create flags
					0,   // Open device as non-overlapped so we can get data
					NULL);

				break;
			}
		}*/

		++i;
	}

	SetupDiDestroyDeviceInfoList(hDevInfo);

	return;
}


// {EC87F1E3-C13B-4100-B5F7-8B84D54260CB}
DEFINE_GUID(XUSB_INTERFACE_CLASS_GUID,
	0xEC87F1E3, 0xC13B, 0x4100, 0xB5, 0xF7, 0x8B, 0x84, 0xD5, 0x42, 0x60, 0xCB);

HANDLE OpenXInputDevice()
{
	HANDLE hDevice = NULL;
	DWORD requiredLength = 0;
	SP_DEVINFO_DATA devInfoData;
	SP_DEVICE_INTERFACE_DATA devInterfaceData = { 0 };
	std::vector<unsigned char> pDetailDataBuffer;

	//
	// Open a handle to the plug and play dev node.
	//
	HDEVINFO hDevInfo = SetupDiGetClassDevsW(&XUSB_INTERFACE_CLASS_GUID,
		NULL, // Define no enumerator (global)
		NULL, // Define no
		(DIGCF_PRESENT | // Only Devices present
			DIGCF_DEVICEINTERFACE)); // Function class devices.

	if (INVALID_HANDLE_VALUE == hDevInfo)
	{
		std::cout << "SetupDiGetClassDevs failed." << std::endl;
		return NULL;
	}

	/*int i = 0;
	while (true)
	{*/
	devInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	//	Enumerate HID devices
	BOOL b = SetupDiEnumDeviceInterfaces(hDevInfo,
		0, // No care about specific PDOs
		&XUSB_INTERFACE_CLASS_GUID,
		0,
		&devInterfaceData);
	if (b)
	{


		//	Get instance ID
		/*devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
		SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData);

		WCHAR instanceID[MAX_PATH] = { 0 };
		SetupDiGetDeviceInstanceId(hDevInfo, &devInfoData, instanceID, MAX_PATH, &requiredLength);
		std::wstring sInstanceID = instanceID;

		std::wcout << L"Instance found : " << sInstanceID << std::endl;

		//	Check instance ID
		for (auto it = hardwareIDList.begin(); it != hardwareIDList.end(); ++it)
		{
		if (sInstanceID.find(*it) == 0)	// test hardware ID
		{*/

		//	Get interface detail
		SetupDiGetDeviceInterfaceDetailW(
			hDevInfo,
			&devInterfaceData,
			NULL,
			0,
			&requiredLength,
			NULL);

		pDetailDataBuffer.resize(requiredLength);
		PSP_DEVICE_INTERFACE_DETAIL_DATA pDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)&pDetailDataBuffer[0];
		pDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

		b = SetupDiGetDeviceInterfaceDetailW(
			hDevInfo,
			&devInterfaceData,
			pDetailData,
			pDetailDataBuffer.size(),
			&requiredLength,
			NULL);


		//	Create handle
		hDevice = CreateFile(pDetailData->DevicePath,
			GENERIC_READ | GENERIC_WRITE,
			0,
			NULL,        // no SECURITY_ATTRIBUTES structure
			OPEN_EXISTING, // No special create flags
			0,   // Open device as non-overlapped so we can get data
			NULL);
	}
	/*++i;
}*/
	SetupDiDestroyDeviceInfoList(hDevInfo);

	return hDevice;
}
/*
IOCTL_XINPUT_GET_INFORMATION = 80006000h

InBuffer : size = 0
OutBuffer : size = 12
*/
#pragma pack(push,1)
struct IOCTL_XINPUT_GET_INFORMATION_Buffer
{
	BYTE unknown0;
	BYTE unknown1;
	BYTE unknown2;	//	something like device or interface count
	BYTE unknown3;
	BYTE unknown4;
	BYTE unknown5;
	BYTE unknown6;
	BYTE unknown7;
	BYTE unknown8;
	BYTE unknown9;
	BYTE unknown10;
	BYTE unknown11;
};
/*
2
1
1
0
0
0
0
0
94
4
142
2
*/

typedef struct _XINPUT_DEVICE_ID_V_0100
{
	UCHAR	deviceNumber;
} XINPUT_DEVICE_ID_V_0100, * PXINPUT_DEVICE_ID_V_0100;

typedef struct _XINPUT_DEVICE_ID
{
	USHORT	Version;
	UCHAR	deviceNumber;
} XINPUT_DEVICE_ID, * PXINPUT_DEVICE_ID;

typedef struct _XINPUT_GET_GAMEPAD_STATE_BUFFER_V_0100 //	sizeof = 20
{
	UCHAR	Success;
	UCHAR	unknown;
	UCHAR	unknown3;	//	255
	ULONG	PacketNumber;
	USHORT	wButtons;
	UCHAR	LeftTrigger;
	UCHAR	RightTrigger;
	SHORT	ThumbLX;
	SHORT	ThumbLY;
	SHORT	ThumbRX;
	SHORT	ThumbRY;
	UCHAR	unknown4;	//	6
} XINPUT_GET_GAMEPAD_STATE_BUFFER_V_0100, * PXINPUT_GET_GAMEPAD_STATE_BUFFER_V_0100;

typedef struct _XINPUT_GET_GAMEPAD_STATE_BUFFER
{
	USHORT	Version;
	UCHAR	Success;
	UCHAR	unknown2;
	UCHAR	unknown3;	// 255
	ULONG	PacketNumber;
	UCHAR	unknown4;
	UCHAR	unknown5;	//	20
	USHORT	wButtons;
	UCHAR	LeftTrigger;
	UCHAR	RightTrigger;
	SHORT	ThumbLX;
	SHORT	ThumbLY;
	SHORT	ThumbRX;
	SHORT	ThumbRY;
	UCHAR	unknown6;
	UCHAR	unknown7;
	UCHAR	unknown8;
	UCHAR	unknown9;
	UCHAR	unknown10;
	UCHAR	unknown11;
} XINPUT_GET_GAMEPAD_STATE_BUFFER, * PXINPUT_GET_GAMEPAD_STATE_BUFFER;

typedef struct _XINPUT_GET_CAPABILITIES_BUFFER
{
	USHORT Version;
	UCHAR Type;
	UCHAR SubType;
	USHORT wButtons;	//	&= 0xF7FF
	UCHAR bLeftTrigger;
	UCHAR bRightTrigger;
	USHORT sThumbLX;
	USHORT sThumbLY;
	USHORT sThumbRX;
	USHORT sThumbRY;
	UCHAR unknown16;
	UCHAR unknown17;
	UCHAR unknown18;
	UCHAR unknown19;
	UCHAR unknown20;
	UCHAR unknown21;
	UCHAR wLeftMotorSpeed;
	UCHAR wRightMotorSpeed;
} XINPUT_GET_CAPABILITIES_BUFFER, * PXINPUT_GET_CAPABILITIES_BUFFER;
#pragma pack(pop)

int main(int argc, WCHAR* argv[])
{
	/*DWORD dqdz = 0x800000;
	HANDLE hDevice = OpenXInputDevice();

	XINPUT_DEVICE_ID inBuffer = { 0 };
	inBuffer.deviceNumber = 0;
	inBuffer.Version = 0x0101;

	XINPUT_GET_CAPABILITIES_BUFFER buffer = { 0 };

	DWORD BytesReturned = 0;
	BOOL b = DeviceIoControl(hDevice, 0x8000E004, &inBuffer, sizeof(inBuffer), &buffer, sizeof(buffer), &BytesReturned, NULL);
	DWORD qzdzqd = GetLastError();
	int qzdqzdd = 0;*/
	HANDLE hDevice = OpenDS4Handle();
	testOutput(hDevice);
	//testInput(hDevice);

	//test(hDevice);
	/*UCHAR buffer[4096] = { 0 };

	DWORD BytesReturned = 0;
	BOOL b = DeviceIoControl(hDevice, IOCTL_HID_GET_REPORT_DESCRIPTOR, NULL, 0, buffer, 4096, &BytesReturned, NULL);
	int qqzdzqd = GetLastError();*/
	/*UCHAR buffer[2048] = { 0 };
	buffer[0] = 1;
	BOOL b = HidD_GetInputReport(hDevice, buffer, 2048);

	std::ofstream myfile;
	myfile.open("inReport.txt");
	for (int i = 0; i < 2048; ++i)
		myfile << "0x" << std::hex << (int)buffer[i] << " ";
	myfile.close();*/

	//OriEmulation(hDevice);
	/*UINT refreshRate = 1000;
	double frameLengthUs = (1.0f / refreshRate) * 1000000;

	float period = 1.0f;
	UCHAR color[3] = { 0xFF, 0xFF, 0xFF };

	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);

	while (true)
	{
		LARGE_INTEGER counter1;
		QueryPerformanceCounter(&counter1);

		DS4_OutputReport report = { 0 };
		report.reportID = 5;

		report.red = color[0];
		report.green = color[1];
		report.blue = color[2];

		report.ledEnable = 2;
		report.cmdType = 0x2;

		DWORD BytesWritten = 0;
		bool b = WriteFile(hDevice, &report, sizeof(DS4_OutputReport), &BytesWritten, NULL);

		LARGE_INTEGER counter2;
		QueryPerformanceCounter(&counter2);

		LARGE_INTEGER elapsed;
		elapsed.QuadPart = counter2.QuadPart - counter1.QuadPart;

		double elapsedUs = elapsed.QuadPart / ((double)frequency.QuadPart / 1000000);
		if (elapsedUs > frameLengthUs)
			Sleep(frameLengthUs - elapsedUs);
	}*/



	//while (true)
	//{
		/*UCHAR buttonMapping[12] = { 5, 0, 3, 1, 4, 2, 9, 8, 6, 7, 10, 11 };
		//UCHAR buttonMapping[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

		DWORD BytesReturned = 0;
		BOOL b = DeviceIoControl(hDevice, IOCTL_DS4_SET_BUTTON_MAPPING, buttonMapping, 12, NULL, 0, &BytesReturned, NULL);*/
		//testOutput(hDevice);
			/*HIDD_ATTRIBUTES strtAttrib;
			strtAttrib.Size = sizeof(HIDD_ATTRIBUTES);

			BOOL b = HidD_GetAttributes(hDevice, &strtAttrib);

			PHIDP_PREPARSED_DATA pData;
			b = HidD_GetPreparsedData(hDevice, &pData);
			int err = GetLastError();
			HidD_FreePreparsedData(pData);

			std::vector<BYTE> buffer(0x4000);
			buffer[0] = 1;
			b = HidD_GetInputReport(hDevice, &buffer[0], 0x4000);
			err = GetLastError();*/
			/*WCHAR buffer[MAX_PATH] = { 0 };
			BOOL b = HidD_GetProductString(hDevice, buffer, MAX_PATH);
			if (!b)
			{
				std::cout << "HidD_GetProductString failed." << std::endl;
			}
			else
				std::wcout << L"Product string : " << buffer << L"." << std::endl;
			DWORD BytesReturned = 0;
			BOOL b = DeviceIoControl(hDevice, IOCTL_DS4_TEST, NULL, 0, NULL, 0, &BytesReturned, NULL);
			std::cout << "DeviceIOControl returned " << b << std::endl;
			if (!b)
			{
				int err = GetLastError();
				std::cout << "GetLastError = " << err << std::endl;
			}*/
			//Sleep(2000);

			/*DWORD BytesWritten = 0;
			DS4_OutReport outReport;
			ZeroMemory(&outReport, sizeof(DS4_OutReport));

			outReport.bluetooth_hid_hdr[0] = 0xa2;
			outReport.bluetooth_hid_hdr[1] = 0x11;

			outReport.unknown1[0] = 0xc0;
			outReport.unknown1[1] = 0x20;
			outReport.rumbleEnable = 0xf0;
			outReport.unknown2[0] = 0x04;

			outReport.RGB[1] = 0xFF;

			outReport.unknown3[10] = 0x43;
			outReport.unknown3[11] = 0x43;
			outReport.unknown3[13] = 0x4d;
			outReport.unknown3[14] = 0x85;

			boost::crc_32_type result;
			result.process_bytes(&outReport, sizeof(DS4_OutReport) - 4);

			outReport.CRC32 = result.checksum();

			//BOOLEAN b = HidD_SetOutputReport(hDevice, &outReport.unknown1[0], sizeof(DS4_OutReport) - 2);
			BOOL b = WriteFile(hDevice, &outReport, sizeof(DS4_OutReport) , &BytesWritten, NULL);*/
			//int zqzd = 0;
		//}
		//CloseHandle(hDevice);

	std::string sss;
	std::cin >> sss;
	return 0;
}

