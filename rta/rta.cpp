/******************************************************************************
*
* Filename: rta.cpp
* Author: Stuart Harlan
*
******************************************************************************/

#include "stdafx.h"

#pragma comment(lib, "avrt.lib")
#pragma comment(lib, "Rtworkq.lib")

using namespace std;

#define reftimes_per_sec (UINT64)10000000
//#define frames_per_sec (UINT64)44100
//#define num_channels (UINT64)2
//#define bits_per_sample (UINT64)16

//WAVEFORMATEX MY_WAVE_FORMAT = {
//	WAVE_FORMAT_PCM,
//	num_channels,
//	frames_per_sec,
//	(bits_per_sample / (UINT64)8) * num_channels * frames_per_sec,
//	(bits_per_sample / (UINT64)8) * num_channels,
//	bits_per_sample,
//	(UINT64)0
//};

BOOL isDone = FALSE;
//short UpperClip = 10000;
//short LowerClip = -10000;
//float SampleMax = 32767;
//float SampleMin = -32767;
//float AmplificationMultiplier = 15.0f;
void MyDataHandler(BYTE* buffer, UINT32 frameCount, BOOL* Cancel)
{
	//if (_kbhit()) {
	//	int c = _getch();
	//	if (c == 0x2e) {
	//		AmplificationMultiplier += 0.1f;
	//		printf("\n0x%x - AmpMult %.1f\n", c, AmplificationMultiplier);
	//	} else if (c == 0x2c) {
	//		AmplificationMultiplier -= 0.1f;
	//		if (AmplificationMultiplier < 0.0) AmplificationMultiplier = 0.0f;
	//		printf("\n0x%x - AmpMult %.1f\n", c, AmplificationMultiplier);
	//	}
	//	else if (c == 0x3d) {
	//		UpperClip += (short)1000;
	//		LowerClip -= (short)1000;
	//		printf("\n0x%x - Clip %i, %i\n", c, LowerClip, UpperClip);
	//	}
	//	else if (c == 0x2d) {
	//		UpperClip -= (short)1000;
	//		if (UpperClip < (short)0) UpperClip = (short)0;
	//		LowerClip += (short)1000;
	//		if (LowerClip >(short)0) LowerClip = (short)0;
	//		printf("\n0x%x - Clip %i, %i\n", c, LowerClip, UpperClip);
	//	}
	//	else {
	//		printf("\n0x%x\n", c);
	//	}
	//}

	//FRAME* thisFrame = NULL;
	//FRAME* lastFrame = buffer + frameCount;
	//float fval;

	//// amplify
	//for (thisFrame = buffer; thisFrame < lastFrame; thisFrame++) {
	//	// amp
	//	fval = (float)(thisFrame->left) * AmplificationMultiplier;
	//	if (fval > SampleMax) fval = SampleMax;
	//	if (fval < SampleMin) fval = SampleMin;
	//	thisFrame->left = (short)fval;
	//	//fval /= 32767.0f;
	//	//fval -= (pow(fval, 3.0f) / 3.0f);
	//	//thisFrame->left = (short)(fval * 32767.0f);

	//	fval = (float)(thisFrame->right) * AmplificationMultiplier;
	//	if (fval > SampleMax) fval = SampleMax;
	//	if (fval < SampleMin) fval = SampleMin;
	//	thisFrame->right = (short)fval;
	//	//fval /= 32767.0f;
	//	//fval -= (pow(fval, 3.0f) / 3.0f);
	//	//thisFrame->right = (short)(fval * 32767.0f);

	//	// clip
	//	if (thisFrame->left > UpperClip) thisFrame->left = UpperClip;
	//	if (thisFrame->left < LowerClip) thisFrame->left = LowerClip;
	//	if (thisFrame->right > UpperClip) thisFrame->right = UpperClip;
	//	if (thisFrame->right < LowerClip) thisFrame->right = LowerClip;

	//}

	*Cancel = isDone;
}

BOOL WINAPI HandlerRoutine(DWORD dwCtrlType) {
	switch (dwCtrlType) {
	case CTRL_C_EVENT:
		RtaStopOutputWindow();
		isDone = TRUE;
		return TRUE;
	}
	return FALSE;
}

int main()
{

	printf("RTA (Real Time Audio)\n");
	printf("by Stuart Harlan, (c) 2018\n");
	printf("\n");

	SetConsoleCtrlHandler(HandlerRoutine, TRUE);

	CoInitialize(NULL);

	LPRTA_DEVICE_INFO CaptureDevice = NULL;
	LPRTA_DEVICE_INFO RenderDevice = NULL;
	LPRTA_DEVICE_INFO lpRenderDevices = NULL;
	LPRTA_DEVICE_INFO lpCaptureDevices = NULL;

	cout << "Capture Devices" << endl;
	cout << "===============" << endl;
	UINT32 count = rta_list_supporting_devices_2(&lpCaptureDevices);
	if (count < 1) {
		cout << "ERROR: No capture devices." << endl;
		SetConsoleCtrlHandler(HandlerRoutine, FALSE);
		CoUninitialize();
		return 0;
	} else if(lpCaptureDevices != NULL) {
		LPRTA_DEVICE_INFO lpThis = lpCaptureDevices;
		while (lpThis != NULL) {
			wcout << L"Name = " << lpThis->DeviceName << L"; ID = " << lpThis->DeviceId << endl;

			if (lpThis->pNext == NULL) {
				printf("\n!! This is the last one. !!\n");
			}
			printf("\nUse this device for capturing (y/n): ");
			int response = _getch();
			printf("\n\n");

			if (response == 0x79) {
				CaptureDevice = lpThis;
				break;
			}

			lpThis = (LPRTA_DEVICE_INFO)lpThis->pNext;
		}
	}

	cout << "Render Devices" << endl;
	cout << "==============" << endl;
	count = rta_list_supporting_devices_2(&lpRenderDevices, DEVICE_STATE_ACTIVE, eRender);
	if (lpRenderDevices != NULL) {
		LPRTA_DEVICE_INFO lpThis = lpRenderDevices;
		while (lpThis != NULL) {
			wcout << L"Name = " << lpThis->DeviceName << L"; ID = " << lpThis->DeviceId << endl;

			if (lpThis->pNext == NULL) {
				printf("\n!! This is the last one. !!\n");
			}
			printf("\nUse this device for rendering (y/n): ");
			int response = _getch();
			printf("\n\n");

			if (response == 0x79) {
				RenderDevice = lpThis;
				break;
			}

			lpThis = (LPRTA_DEVICE_INFO)lpThis->pNext;
		}
	}

	if (CaptureDevice != NULL) {
		wcout << endl << "Using this capture device " << CaptureDevice->DeviceName << endl;
		if (FALSE == rta_initialize_device_2(CaptureDevice, AUDCLNT_STREAMFLAGS_EVENTCALLBACK))
			CaptureDevice = NULL;
	}

	if (RenderDevice != NULL) {
		wcout << endl << "Using this render device " << RenderDevice->DeviceName << endl;
		if (FALSE == rta_initialize_device_2(RenderDevice, 0))
			RenderDevice = NULL;
	}

	rta_capture_frames_rtwq(CaptureDevice, RenderDevice, MyDataHandler);

	if(lpRenderDevices != NULL) 
		rta_free_device_list(lpRenderDevices);
	if(lpCaptureDevices != NULL) 
		rta_free_device_list(lpCaptureDevices);

	CoUninitialize();

	SetConsoleCtrlHandler(HandlerRoutine, FALSE);

	return 0;
}

