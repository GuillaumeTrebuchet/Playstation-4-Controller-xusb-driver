
#pragma once
#include <ntddk.h>


#define DS4_OUT_CMD_TYPE_VIBRATION	0x1
#define DS4_OUT_CMD_TYPE_LED		0x2
#define DS4_OUT_CMD_TYPE_LED_FADE	0x4

#define DS4_LED_CMD_ON	0x0
#define DS4_LED_CMD_OFF	0x1


#pragma pack(push, 1)

/*
Report ID 0x11
*/
typedef struct _DS4_BTH_OUTPUT_REPORT
{
	UCHAR	flags;		// must be 0x80
	UCHAR	unknown0;
	UCHAR	cmdType;
	UCHAR	unknown1[2];
	UCHAR	rumbleRight;
	UCHAR	rumbleLeft;
	UCHAR	r;
	UCHAR	g;
	UCHAR	b;
	UCHAR	unknown2[67];
} DS4_BTH_OUTPUT_REPORT, *PDS4_BTH_OUTPUT_REPORT;

typedef struct _DS4_BTH_INPUT_TRACKPAD_PACKET
{
	UCHAR	packetCounter;
	UCHAR	finger1ID;		//	b7: active low, b6-0: finger ID
	UCHAR	finger1Coord[3];
	UCHAR	finger2ID;
	UCHAR	finger2Coord[3];
} DS4_BTH_INPUT_TRACKPAD_PACKET, *PDS4_BTH_INPUT_TRACKPAD_PACKET;

/*
Only sent after get feature report 0x02
must read 547 bytes
*/
typedef struct _DS4_BTH_INPUT_REPORT
{
	UCHAR	unknown0;	//	0xc0
	UCHAR	reportID;	//	0x00
	UCHAR	X;
	UCHAR	Y;
	UCHAR	Z;
	UCHAR	Rz;
	UCHAR	Buttons[3];
	UCHAR	Rx;
	UCHAR	Ry;
	USHORT	timestamp;
	UCHAR	batteryLevel;
	USHORT	accelX;
	USHORT	accelY;
	USHORT	accelZ;
	USHORT	roll;
	USHORT	yaw;
	USHORT	pitch;
	UCHAR	unknown1[5];
	UCHAR	flags;			//	b6 : phone, b5: mic, b4: usb, b3-2-1-0: battery
	UCHAR	unknown2[2];
	UCHAR	nTrackpadPackets;
	DS4_BTH_INPUT_TRACKPAD_PACKET	packets[4];
	UCHAR	unknown3[2];	//	0x00 0x01, or 0x00, 0x00
	UCHAR	crc32[4];
	// 66
} DS4_BTH_INPUT_REPORT, *PDS4_BTH_INPUT_REPORT;

//	Report ID 5
typedef struct _DS4_USB_OUTPUT_REPORT
{
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
} DS4_USB_OUTPUT_REPORT, *PDS4_USB_OUTPUT_REPORT;

#pragma pack(pop)


#pragma pack(push, 1)

//	Report ID 1
typedef struct _DS4_USB_INPUT_REPORT
{
	UCHAR	X;
	UCHAR	Y;
	UCHAR	Z;
	UCHAR	Rz;
	UCHAR	Buttons[3];	//	: 4 HatSwitch(0 = top, 1 = top right, 2 = right, 3 = bot right, etc... 8 = none), : 12 buttons, : 6 vendor defined
	UCHAR	Rx;
	UCHAR	Ry;
	UCHAR	unknown[54];	// vendor defined
} DS4_USB_INPUT_REPORT, *PDS4_USB_INPUT_REPORT;

#pragma pack(pop)