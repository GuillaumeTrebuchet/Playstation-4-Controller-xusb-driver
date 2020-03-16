// test_xinput.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <array>

#include <Windows.h>
#include <Xinput.h>

/*
XINPUT9_1_0
*/
/*
IOCTL_XINPUT_GET_GAMEPAD_STATE = 0x8000E00C

InBuffer : size = 1, value = 0
OutBuffer : IOCTL_XINPUT_GET_GAMEPAD_STATE_Buffer
*/
struct IOCTL_XINPUT_GET_GAMEPAD_STATE_IN_BUFFER_V_0100
{
	UCHAR deviceIndexWeird;
};
struct IOCTL_XINPUT_GET_GAMEPAD_STATE_IN_BUFFER
{
	WORD	unknown0;	//	0x0101
	UCHAR	deviceIndexWeird;
};
struct IOCTL_XINPUT_GET_GAMEPAD_STATE_OUT_BUFFER_V_0100 //	sizeof = 20
{
	UCHAR	Success;
	UCHAR	unknown;
	UCHAR	unknown3;
	ULONG	PacketNumber;
	USHORT	wButtons;
	UCHAR	LeftTrigger;
	UCHAR	RightTrigger;
	SHORT	ThumbLX;
	SHORT	ThumbLY;
	SHORT	ThumbRX;
	SHORT	ThumbRY;
	UCHAR	unknown4;
};
struct IOCTL_XINPUT_GET_GAMEPAD_STATE_OUT_BUFFER //	sizeof = 20
{
	UCHAR	unknown0;
	UCHAR	unknown1;
	UCHAR	Success;
	UCHAR	unknown2;
	UCHAR	unknown3;
	ULONG	PacketNumber;
	UCHAR	unknown4;
	UCHAR	unknown5;
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
};
/*
IOCTL_XINPUT_SET_GAMEPAD_STATE = 8000A010h

InBuffer : size = 5
OutBuffer : size = 0
*/
struct IOCTL_XINPUT_SET_GAMEPAD_STATE_Buffer
{
	BYTE deviceNumber;
	BYTE unknown1;	//	some led param		6 => controller detected? (for wake up?), 0 => controller go to sleep? (when Xinput quit)
	BYTE leftMotorSpeed;	//	wLeftMotorSpeed >> 8
	BYTE rightMotorSpeed;	//	wRightMotorSpeed >> 8
	BYTE cmdType;	//	2 = vibration, 1 = led
};

/*
IOCTL_XINPUT_GET_INFORMATION = 80006000h

InBuffer : size = 0
OutBuffer : size = 12
*/
struct IOCTL_XINPUT_GET_INFORMATION_Buffer
{
	/*
	if 0x100 :
	IOCTL_XINPUT_GET_GAMEPAD_STATE :
	input buffer size = 1 and
	buffer content = DeviceIndexWeird( 0 > DeviceIndexWeird < (UCHAR)(unknown0 >> 8)
	output buffer size  = 20
	else
	input buffer size = 3 and
	buffer content[0, 1] = 0x0101 , [2] = DeviceIndexWeird
	output buffer size  = 29
	*/
	USHORT	version;	//	XINPUT_VERSION_XXX
	USHORT	deviceCount;
	UINT32	unknown4;	//	must not be (& 0x80) = TRUE
	BYTE	unknown8;
	BYTE	unknown9;
	BYTE	unknown10;
	BYTE	unknown11;
};

/*
IOCTL_XINPUT_GET_CAPABILITIES = 8000E004h
if version == 0x0100 :
{
	copy default static capabilities
}
else
send IOCTL_XINPUT_GET_CAPABILITIES with
{
	inBufferLength = 3
	inBuffer[0, 1] = (WORD)257
	inBuffer[2] = DeviceIndexWeird
}

*/
struct IOCTL_XINPUT_GET_CAPABILITIES_Buffer
{
	BYTE unknown0;
	BYTE unknown1;
	BYTE Type;
	BYTE SubType;
	WORD wButtons;	//	&= 0xF7FF
	BYTE bLeftTrigger;
	BYTE bRightTrigger;
	WORD sThumbLX;
	WORD sThumbLY;
	WORD sThumbRX;
	WORD sThumbRY;
	BYTE unknown16;
	BYTE unknown17;
	BYTE unknown18;
	BYTE unknown19;
	BYTE unknown20;
	BYTE unknown21;
	BYTE wLeftMotorSpeed;
	BYTE wRightMotorSpeed;
};


/*
IOCTL_XINPUT_ACQUIRE_TID = 8000E004h
UNUSED
*/

/*
IOCTL_XINPUT_RELEASE_TID = 8000E008h
UNUSED
*/

/*

XINPUT1_3

*/

/*
IOCTL_XINPUT_GET_LED_STATE = 8000E008h
InBuffer : size = 3, InBuffer[0] = int16 => 257
OutBuffer : size = 3
*/

/*
IOCTL_XINPUT_GET_BATTERY_INFORMATION = 8000E018h
*/

struct IOCTL_XINPUT_GET_BATTERY_INFORMATION_Buffer
{
	BYTE unknown0;
	BYTE unknown1;
	BYTE BatteryType;
	BYTE BatteryLevel;
};

/*
IOCTL_XINPUT_POWER_DOWN_DEVICE = 8000A01Ch
InBuffer : size = 0
OutBuffer : size = 3
*/

struct IOCTL_XINPUT_POWER_DOWN_DEVICE_Buffer
{
	short unknown0;	// 258
	char unknown1;	// *((byte*)this + 12)
};

/*
IOCTL_XINPUT_WAIT_FOR_GUIDE_BUTTON = 8000E014h
*/

//E:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Lib\x86\Xinput.lib

struct XInputState
{
	XINPUT_STATE				state;
	XINPUT_BATTERY_INFORMATION	battery;
};
std::array<std::unique_ptr<XInputState>, XUSER_MAX_COUNT> g_states;

bool ButtonChanged(const XINPUT_STATE& oldState, const XINPUT_STATE& newState, WORD flag)
{
	if ((oldState.Gamepad.wButtons & flag) != (newState.Gamepad.wButtons & flag))
		return true;
	else
		return false;
}
bool IsButtonPushed(const XINPUT_STATE& newState, WORD flag)
{
	if (newState.Gamepad.wButtons & flag)
		return true;
	else
		return false;
}
void UpdateState(XINPUT_STATE& oldState, XINPUT_STATE& newState)
{
	if (oldState.dwPacketNumber < newState.dwPacketNumber)
	{
		if (ButtonChanged(oldState, newState, XINPUT_GAMEPAD_DPAD_UP))
			std::cout << "Up : " << (IsButtonPushed(newState, XINPUT_GAMEPAD_DPAD_UP) ? 1 : 0) << std::endl;
		if (ButtonChanged(oldState, newState, XINPUT_GAMEPAD_DPAD_DOWN))
			std::cout << "Down : " << (IsButtonPushed(newState, XINPUT_GAMEPAD_DPAD_DOWN) ? 1 : 0) << std::endl;
		if (ButtonChanged(oldState, newState, XINPUT_GAMEPAD_DPAD_LEFT))
			std::cout << "Left : " << (IsButtonPushed(newState, XINPUT_GAMEPAD_DPAD_LEFT) ? 1 : 0) << std::endl;
		if (ButtonChanged(oldState, newState, XINPUT_GAMEPAD_DPAD_RIGHT))
			std::cout << "Right : " << (IsButtonPushed(newState, XINPUT_GAMEPAD_DPAD_RIGHT) ? 1 : 0) << std::endl;
		if (ButtonChanged(oldState, newState, XINPUT_GAMEPAD_START))
			std::cout << "Start : " << (IsButtonPushed(newState, XINPUT_GAMEPAD_START) ? 1 : 0) << std::endl;
		if (ButtonChanged(oldState, newState, XINPUT_GAMEPAD_BACK))
			std::cout << "Back : " << (IsButtonPushed(newState, XINPUT_GAMEPAD_BACK) ? 1 : 0) << std::endl;
		if (ButtonChanged(oldState, newState, XINPUT_GAMEPAD_LEFT_THUMB))
			std::cout << "Left thumb : " << (IsButtonPushed(newState, XINPUT_GAMEPAD_LEFT_THUMB) ? 1 : 0) << std::endl;
		if (ButtonChanged(oldState, newState, XINPUT_GAMEPAD_RIGHT_THUMB))
			std::cout << "Right thumb : " << (IsButtonPushed(newState, XINPUT_GAMEPAD_RIGHT_THUMB) ? 1 : 0) << std::endl;
		if (ButtonChanged(oldState, newState, XINPUT_GAMEPAD_LEFT_SHOULDER))
			std::cout << "Left shoulder : " << (IsButtonPushed(newState, XINPUT_GAMEPAD_LEFT_SHOULDER) ? 1 : 0) << std::endl;
		if (ButtonChanged(oldState, newState, XINPUT_GAMEPAD_RIGHT_SHOULDER))
			std::cout << "Right shoulder : " << (IsButtonPushed(newState, XINPUT_GAMEPAD_RIGHT_SHOULDER) ? 1 : 0) << std::endl;
		if (ButtonChanged(oldState, newState, XINPUT_GAMEPAD_A))
			std::cout << "A : " << (IsButtonPushed(newState, XINPUT_GAMEPAD_A) ? 1 : 0) << std::endl;
		if (ButtonChanged(oldState, newState, XINPUT_GAMEPAD_B))
			std::cout << "B : " << (IsButtonPushed(newState, XINPUT_GAMEPAD_B) ? 1 : 0) << std::endl;
		if (ButtonChanged(oldState, newState, XINPUT_GAMEPAD_X))
			std::cout << "X : " << (IsButtonPushed(newState, XINPUT_GAMEPAD_X) ? 1 : 0) << std::endl;
		if (ButtonChanged(oldState, newState, XINPUT_GAMEPAD_Y))
			std::cout << "Y : " << (IsButtonPushed(newState, XINPUT_GAMEPAD_Y) ? 1 : 0) << std::endl;

		oldState = newState;
	}
}
void UpdateBatteryState(XINPUT_BATTERY_INFORMATION& oldState, XINPUT_BATTERY_INFORMATION& newState)
{
	if (oldState.BatteryLevel != newState.BatteryLevel)
	{
		std::cout << "Battery level : " << (newState.BatteryLevel * 100.0f / 255.0f) << "%" << std::endl;
	}
	oldState = newState;
}
int main()
{
	while (true)
	{
		DWORD dwResult;
		for (DWORD i = 0; i < XUSER_MAX_COUNT; i++)
		{
			/*XINPUT_STATE state;
			ZeroMemory(&state, sizeof(XINPUT_STATE));
			// Simply get the state of the controller from XInput.
			dwResult = XInputGetState(i, &state);*/

			/*XINPUT_CAPABILITIES caps;
			ZeroMemory(&caps, sizeof(XINPUT_CAPABILITIES));
			dwResult = XInputGetCapabilities(i, 0, &caps);
			*/
			XINPUT_STATE state;
			if (XInputGetState(i, &state) == ERROR_DEVICE_NOT_CONNECTED)
			{
				if (g_states[i])
				{
					g_states[i].reset();
					std::cout << "XInput device on port " << i << " disconnected." << std::endl;
				}
			}
			else
			{
				XINPUT_BATTERY_INFORMATION batteryInfo;
				dwResult = XInputGetBatteryInformation(i, BATTERY_DEVTYPE_GAMEPAD, &batteryInfo);

				if (!g_states[i])
				{
					g_states[i] = std::make_unique<XInputState>();
					std::cout << "XInput device connected on port " << i << "." << std::endl;
				}
				
				UpdateState(g_states[i]->state, state);
				UpdateBatteryState(g_states[i]->battery, batteryInfo);
			}
			
			/*if (dwResult == ERROR_SUCCESS)
			{
				// Controller is connected
				//std::cout << "Controller " << i << " connected." << std::endl;
				std::cout << "Vibration test..." << std::endl;

				XINPUT_VIBRATION vib = { 0 };
				vib.wLeftMotorSpeed = 0xFFFF;
				vib.wRightMotorSpeed = 0xFFFF;

				dwResult = XInputSetState(i, &vib);
				std::cout << "dwResult = " << dwResult << std::endl;
				Sleep(5000);

				vib.wLeftMotorSpeed = 0;
				vib.wRightMotorSpeed = 0;

				dwResult = XInputSetState(i, &vib);
				std::cout << "dwResult = " << dwResult << std::endl;
				std::cin >> std::string();
				//std::cout << "bLeftTrigger = " << (int)state.Gamepad.bLeftTrigger << std::endl;
			}
			else if (dwResult == ERROR_DEVICE_NOT_CONNECTED)
			{
				// Controller is not connected
				std::cout << "Controller " << i << " not connected." << std::endl;
			}
			else
			{
				std::cout << "XInputGetState failed." << std::endl;
			}*/
		}
		Sleep(500);
	}

	return 0;
}
