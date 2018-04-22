
#include "stdafx.h"

#define CHKERR(HR,E) if (FAILED(HR)) { last_error = E; goto done; }

HANDLE g_RtwqStop = NULL;
DWORD g_RtwqId = 0;

const char* ERROR_1 = "Failed to create instance of device enum";
const char* ERROR_2 = "Failed to enum active capture endpoints";
const char* ERROR_3 = "Failed to get device count";
const char* ERROR_4 = "Failed to get device from id";
const char* ERROR_5 = "Failed to activate audio client";
const char* ERROR_6 = "Failed to get device period";
const char* ERROR_7 = "Failed to get buffer size";
const char* ERROR_8 = "Failed to initialize";
const char* ERROR_9 = "Failed to create buffer event";
const char* ERROR_10 = "Failed to set event handle";
const char* ERROR_11 = "Failed to get capture service";
const char* ERROR_12 = "Failed to set thread characteristics";
const char* ERROR_13 = "Failed to start capture";
const char* ERROR_14 = "Failed to get buffer";
const char* ERROR_15 = "Failed to release buffer";
const char* ERROR_16 = "Failed to stop audio capture";
const char* ERROR_17 = "Failed to revert thread char";
const char* ERROR_18 = "Failed to get render service";
const char* ERROR_19 = "Failed to start render";
const char* ERROR_20 = "Failed to stop audio render";
const char* ERROR_21 = "Failed to start rtwq";
const char* ERROR_22 = "Failed to lock shared work queue";
const char* ERROR_23 = "Failed to create audio handler";
const char* ERROR_24 = "Failed to QI for async callback";
const char* ERROR_25 = "Failed to create async result";
const char* ERROR_26 = "Failed to put waiting work item";
const char* ERROR_27 = "Failed to set audio client props";
const char* ERROR_28 = "Failed to get native format";
const char* ERROR_29 = "Failed to get shared mode engine period";

const char* last_error = NULL;

const char* rta_get_last_error()
{
	return last_error;
}

static HANDLE HeapHandle = INVALID_HANDLE_VALUE;

LPVOID rta_alloc(SIZE_T size) {
	LPVOID result = NULL;
	if (HeapHandle == INVALID_HANDLE_VALUE) {
		HeapHandle = GetProcessHeap();
	}
	if (HeapHandle != INVALID_HANDLE_VALUE) {
		result = HeapAlloc(HeapHandle, HEAP_ZERO_MEMORY, size);
	}
	return result;
}

void rta_free(LPVOID pvoid) {
	LPVOID result = NULL;
	if (HeapHandle == INVALID_HANDLE_VALUE) {
		HeapHandle = GetProcessHeap();
	}
	if (HeapHandle != INVALID_HANDLE_VALUE) {
		HeapFree(HeapHandle, 0, pvoid);
	}
}

UINT32 rta_list_supporting_devices_2(RTA_DEVICE_INFO** lppDeviceInfo,
	DWORD StateMask, EDataFlow DataFlow, AUDCLNT_SHAREMODE ShareMode)
{

	IMMDeviceEnumerator *pMMDeviceEnumerator = NULL;
	IMMDeviceCollection *pMMDeviceCollection = NULL;
	UINT32 count = 0;

	if (lppDeviceInfo == NULL) return count;
	*lppDeviceInfo = NULL;
	LPRTA_DEVICE_INFO current = NULL;

	HRESULT result = CoCreateInstance(__uuidof(MMDeviceEnumerator), (LPUNKNOWN)NULL,
		CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (LPVOID*)&pMMDeviceEnumerator);
	CHKERR(result, ERROR_1);

	result = pMMDeviceEnumerator->EnumAudioEndpoints(DataFlow, StateMask, &pMMDeviceCollection);
	CHKERR(result, ERROR_2);

	UINT pcDevices = 0;
	result = pMMDeviceCollection->GetCount(&pcDevices);
	CHKERR(result, ERROR_3);

	for (UINT pcDeviceId = 0; pcDeviceId < pcDevices; pcDeviceId++) {

		// get the current device by id
		IMMDevice *pMMDevice = NULL;
		result = pMMDeviceCollection->Item(pcDeviceId, &pMMDevice);

		if (SUCCEEDED(result)) {

			// activate it
			IAudioClient3 *pAudioClient3 = NULL;
			result = pMMDevice->Activate(__uuidof(IAudioClient3), CLSCTX_ALL, (PROPVARIANT*)0, (void**)&pAudioClient3);

			if (SUCCEEDED(result)) {

				// get device id; do not free when done
				LPWSTR lpwstrDeviceId = NULL;
				result = pMMDevice->GetId(&lpwstrDeviceId);

				if (SUCCEEDED(result)) {

					// open prop store
					IPropertyStore *pPropertyStore = NULL;
					result = pMMDevice->OpenPropertyStore(STGM_READ, &pPropertyStore);

					if (SUCCEEDED(result)) {

						// get name
						PROPVARIANT varName;
						PropVariantInit(&varName);
						result = pPropertyStore->GetValue(
							PKEY_Device_FriendlyName, &varName);

						if (SUCCEEDED(result)) {

							PROPVARIANT varIsRaw;
							PropVariantInit(&varIsRaw);
							result = pPropertyStore->GetValue(
								PKEY_Devices_AudioDevice_RawProcessingSupported, &varIsRaw);

							if (SUCCEEDED(result)) {

								// create data structure
								LPRTA_DEVICE_INFO lpdi = (LPRTA_DEVICE_INFO)rta_alloc(sizeof(RTA_DEVICE_INFO));
								lpdi->DeviceId = lpwstrDeviceId;
								lpdi->DeviceName = _wcsdup(varName.pwszVal);
								//lpdi->ShareMode = ShareMode;
								//lpdi->lpWaveFormatEx = lpWaveFormatEx;
								lpdi->IsRawSupported = varIsRaw.boolVal;

								//printf("==========\n");
								//wprintf(L"%s\n", varName.pwszVal);
								//printf("channels %i\n", defFmt->nChannels);
								//printf("samples per sec %i\n", defFmt->nSamplesPerSec);
								//printf("bits per samples %i\n", defFmt->wBitsPerSample);
								//printf("RAW is %ssupported.\n",
								//	(varIsRaw.boolVal == VARIANT_FALSE ? "NOT " : ""));
								//printf("==========\n");

								if (*lppDeviceInfo == NULL) *lppDeviceInfo = lpdi;
								if (current != NULL) current->pNext = lpdi;
								current = lpdi;
								count++;

								PropVariantClear(&varIsRaw);

							}

							PropVariantClear(&varName);

						}

						pPropertyStore->Release();
						pPropertyStore = NULL;
					}
				}
				pAudioClient3->Release();
				pAudioClient3 = NULL;
			}
			pMMDevice->Release();
			pMMDevice = NULL;
		}
	}

done:
	if (pMMDeviceEnumerator != NULL) pMMDeviceEnumerator->Release();
	if (pMMDeviceCollection != NULL) pMMDeviceCollection->Release();
	return count;
}

/*
UINT32 rta_list_supporting_devices(RTA_DEVICE_INFO** lppDeviceInfo, WAVEFORMATEX* lpWaveFormatEx,
	DWORD StateMask, EDataFlow DataFlow, AUDCLNT_SHAREMODE ShareMode) 
{

	IMMDeviceEnumerator *pMMDeviceEnumerator = NULL;
	IMMDeviceCollection *pMMDeviceCollection = NULL;
	UINT32 count = 0;

	if (lppDeviceInfo == NULL) return count;
	*lppDeviceInfo = NULL;
	LPRTA_DEVICE_INFO current = NULL;

	HRESULT result = CoCreateInstance(__uuidof(MMDeviceEnumerator), (LPUNKNOWN)NULL,
		CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (LPVOID*)&pMMDeviceEnumerator);
	CHKERR(result, ERROR_1);

	result = pMMDeviceEnumerator->EnumAudioEndpoints(DataFlow, StateMask, &pMMDeviceCollection);
	CHKERR(result, ERROR_2);

	UINT pcDevices = 0;
	result = pMMDeviceCollection->GetCount(&pcDevices);
	CHKERR(result, ERROR_3);

	for (UINT pcDeviceId = 0; pcDeviceId < pcDevices; pcDeviceId++) {

		// get the current device by id
		IMMDevice *pMMDevice = NULL;
		result = pMMDeviceCollection->Item(pcDeviceId, &pMMDevice);

		if (SUCCEEDED(result)) {

			// activate it
			IAudioClient *pAudioClient = NULL;
			result = pMMDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, (PROPVARIANT*)0, (void**)&pAudioClient);

			LPWAVEFORMATEX defFmt = NULL;
			pAudioClient->GetMixFormat(&defFmt);

			if (SUCCEEDED(result)) {

				// check if format supported
				result = pAudioClient->IsFormatSupported(ShareMode, lpWaveFormatEx, NULL);

				if (result == S_OK) {

					// get device id; do not free when done
					LPWSTR lpwstrDeviceId = NULL;
					result = pMMDevice->GetId(&lpwstrDeviceId);

					if (SUCCEEDED(result)) {

						// open prop store
						IPropertyStore *pPropertyStore = NULL;
						result = pMMDevice->OpenPropertyStore(STGM_READ, &pPropertyStore);

						if (SUCCEEDED(result)) {

							// get name
							PROPVARIANT varName;
							PropVariantInit(&varName);
							result = pPropertyStore->GetValue(PKEY_Device_FriendlyName, &varName);

							if (SUCCEEDED(result)) {

								// create data structure
								LPRTA_DEVICE_INFO lpdi = (LPRTA_DEVICE_INFO)rta_alloc(sizeof(RTA_DEVICE_INFO));
								lpdi->DeviceId = lpwstrDeviceId;
								lpdi->DeviceName = _wcsdup(varName.pwszVal);
								lpdi->ShareMode = ShareMode;
								lpdi->lpWaveFormatEx = lpWaveFormatEx;

								printf("==========\n");
								wprintf(L"%s\n", varName.pwszVal);
								printf("channels %i\n", defFmt->nChannels);
								printf("samples per sec %i\n", defFmt->nSamplesPerSec);
								printf("bits per samples %i\n", defFmt->wBitsPerSample);
								printf("==========\n");

								if (*lppDeviceInfo == NULL) *lppDeviceInfo = lpdi;
								if (current != NULL) current->pNext = lpdi;
								current = lpdi;
								count++;

								PropVariantClear(&varName);
							}
							pPropertyStore->Release();
							pPropertyStore = NULL;
						}
					}
				}
				pAudioClient->Release();
				pAudioClient = NULL;
			}
			pMMDevice->Release();
			pMMDevice = NULL;
		}
	}

done:
	if (pMMDeviceEnumerator != NULL) pMMDeviceEnumerator->Release();
	if (pMMDeviceCollection != NULL) pMMDeviceCollection->Release();
	return count;
}
*/

void rta_free_device_list(LPRTA_DEVICE_INFO lpDeviceInfo)
{
	if (lpDeviceInfo == NULL) return;
	LPRTA_DEVICE_INFO pThis = lpDeviceInfo;
	LPRTA_DEVICE_INFO pNext = NULL;
	while (pThis != NULL) {
		pNext = (LPRTA_DEVICE_INFO)(pThis->pNext);
		if (pThis->DeviceName != NULL) free(pThis->DeviceName);
		if (pThis->DeviceId != NULL) CoTaskMemFree(pThis->DeviceId);
		if (pThis->FrameBuffer != NULL) rta_free(pThis->FrameBuffer);
		if (pThis->pAudioClient != NULL) pThis->pAudioClient->Release();
		if (pThis->pMMDevice != NULL) pThis->pMMDevice->Release();
		rta_free(pThis);
		pThis = pNext;
	}
}

/*
BOOL rta_initialize_device(LPRTA_DEVICE_INFO lpDeviceInfo, DWORD StreamFlags, UINT32 BufferSizeFrames) {

	if (lpDeviceInfo == NULL) return FALSE;

	printf("Init'ing device...\n");

	BOOL retval = FALSE;

	IMMDeviceEnumerator *pMMDeviceEnumerator = NULL;
	REFERENCE_TIME requested = 0;
	REFERENCE_TIME def;
	REFERENCE_TIME min;
	//BOOL firstTry = TRUE;

	if (BufferSizeFrames > 0) {
		printf("INIT: Trying with %i frames...\n", BufferSizeFrames);
		requested = (REFERENCE_TIME)((10000.0 * 1000 / lpDeviceInfo->lpWaveFormatEx->nSamplesPerSec * BufferSizeFrames) + 0.5);
		lpDeviceInfo->BufferSizeFrames = BufferSizeFrames;
	}
	else {
		lpDeviceInfo->BufferSizeFrames = 0;
	}

	if (lpDeviceInfo == NULL) return FALSE;

	HRESULT result = CoCreateInstance(__uuidof(MMDeviceEnumerator), (LPUNKNOWN)NULL,
		CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (LPVOID*)&pMMDeviceEnumerator);
	CHKERR(result, ERROR_1);

start_again:

	// get device
	result = pMMDeviceEnumerator->GetDevice(lpDeviceInfo->DeviceId, &(lpDeviceInfo->pMMDevice));
	CHKERR(result, ERROR_4);

	result = lpDeviceInfo->pMMDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, 0, (void**)&(lpDeviceInfo->pAudioClient));
	CHKERR(result, ERROR_5);

	if (requested == 0) {
		result = lpDeviceInfo->pAudioClient->GetDevicePeriod(&def, &min);
		CHKERR(result, ERROR_6);
		lpDeviceInfo->BufferSizeFrames =
			(lpDeviceInfo->lpWaveFormatEx->nSamplesPerSec *	(UINT32)min) / 10000000;
		printf("!Calculated buffer size = %i\n", lpDeviceInfo->BufferSizeFrames);
	}
	else {
		min = requested;
	}

	printf("Trying to init with reftime = %lli\n", min);
	
	result = lpDeviceInfo->pAudioClient->Initialize(
		lpDeviceInfo->ShareMode,
		StreamFlags,
		min, min, lpDeviceInfo->lpWaveFormatEx, NULL);
	//if(result == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED || TRUE == firstTry)
	if (result == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED)
	{
		//firstTry = FALSE;

		printf("ERROR: Not aligned...try again\n");

		UINT32 bufferSize = 0;
		result = lpDeviceInfo->pAudioClient->GetBufferSize(&bufferSize);
		CHKERR(result, ERROR_7);
		requested = (REFERENCE_TIME)((10000.0 * 1000 / lpDeviceInfo->lpWaveFormatEx->nSamplesPerSec * bufferSize) + 0.5);
		lpDeviceInfo->BufferSizeFrames = bufferSize;

		lpDeviceInfo->pAudioClient->Release();
		lpDeviceInfo->pAudioClient = NULL;

		lpDeviceInfo->pMMDevice->Release();
		lpDeviceInfo->pMMDevice = NULL;

		goto start_again;
	}
	else if (FAILED(result))
	{
		lpDeviceInfo->BufferSizeFrames = 0;
		last_error = ERROR_8;
		goto done;
	}
	else 
	{
		//if (lpDeviceInfo->BufferSizeFrames == 0) {
		//	printf("ERROR: Buffer size is zero\n");
		//	UINT32 TempBufferSize = 0;
		//	lpDeviceInfo->pAudioClient->GetBufferSize(&TempBufferSize);
		//	printf("Using %i as buffer size (from Audio Client)\n", TempBufferSize);
		//	lpDeviceInfo->BufferSizeFrames = TempBufferSize;
		//}

		lpDeviceInfo->SizeOfFrame = lpDeviceInfo->lpWaveFormatEx->nChannels *
			(lpDeviceInfo->lpWaveFormatEx->wBitsPerSample / 8);

		lpDeviceInfo->FrameBuffer = (BYTE*)rta_alloc(
			lpDeviceInfo->BufferSizeFrames * lpDeviceInfo->SizeOfFrame);

		UINT32 TempBufferSize = 0;
		lpDeviceInfo->pAudioClient->GetBufferSize(&TempBufferSize);
		printf("INIT: Success; buffer size is %i [ %i ]\n", 
			lpDeviceInfo->BufferSizeFrames, TempBufferSize);
		if (TempBufferSize != lpDeviceInfo->BufferSizeFrames) {
			printf("WARNING! Buffer sizes do not match!\n");
		}

		REFERENCE_TIME latency = 0;
		lpDeviceInfo->pAudioClient->GetStreamLatency(&latency);
		printf("Latency = %lli\n", latency);
	}

	// done
	retval = TRUE;

done:
	if (pMMDeviceEnumerator != NULL) pMMDeviceEnumerator->Release();
	return retval;
}
*/

BOOL rta_initialize_device_2(LPRTA_DEVICE_INFO lpDeviceInfo, DWORD StreamFlags) {

	last_error = NULL;

	if (lpDeviceInfo == NULL) return FALSE;

	printf("Init'ing device...\n");

	BOOL retval = FALSE;

	IMMDeviceEnumerator *pMMDeviceEnumerator = NULL;
	REFERENCE_TIME requested = 0;

	if (lpDeviceInfo == NULL) return FALSE;

	HRESULT result = CoCreateInstance(__uuidof(MMDeviceEnumerator), (LPUNKNOWN)NULL,
		CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (LPVOID*)&pMMDeviceEnumerator);
	CHKERR(result, ERROR_1);

	// get device
	result = pMMDeviceEnumerator->GetDevice(lpDeviceInfo->DeviceId, &(lpDeviceInfo->pMMDevice));
	CHKERR(result, ERROR_4);

	result = lpDeviceInfo->pMMDevice->Activate(__uuidof(IAudioClient), 
		CLSCTX_ALL, 0, (void**)&(lpDeviceInfo->pAudioClient));
	CHKERR(result, ERROR_5);

	// get reference to audio client 3
	IAudioClient3 *pAudioClient3 = NULL;
	lpDeviceInfo->pAudioClient->QueryInterface(__uuidof(IAudioClient3), (void**)&pAudioClient3);
	CHKERR(result, ERROR_5);

	if (VARIANT_TRUE == lpDeviceInfo->IsRawSupported) {
		AudioClientProperties audioProps = { 0 };
		audioProps.cbSize = sizeof(AudioClientProperties);
		audioProps.eCategory = AudioCategory_Media;
		audioProps.Options |= AUDCLNT_STREAMOPTIONS_RAW;
		audioProps.Options |= AUDCLNT_STREAMOPTIONS_MATCH_FORMAT;
		result = pAudioClient3->SetClientProperties(&audioProps);
		CHKERR(result, ERROR_27);
		printf("NOTE: Successfully set raw properties.\n");
	}
	else {
		printf("NOTE: RAW is not supported for this device.\n");
	}

	WAVEFORMATEX *nativeFormat = NULL;
	result = pAudioClient3->GetMixFormat(&nativeFormat);
	CHKERR(result, ERROR_28);

	printf("Native Samples Per Sec %i\n", nativeFormat->nSamplesPerSec);
	printf("Native Sample Size %i\n", nativeFormat->wBitsPerSample);
	printf("Native Channels %i\n", nativeFormat->nChannels);
	printf("Wave Format Tag %x\n", nativeFormat->wFormatTag);
	printf("Extra Data Size %i\n", nativeFormat->cbSize);
	if (nativeFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {

		WAVEFORMATEXTENSIBLE* lpwfex = (WAVEFORMATEXTENSIBLE*)nativeFormat;

		printf("EXT Channel Mask %i; Channel Configuration:\n", lpwfex->dwChannelMask);
		for (DWORD chid = 1; chid < 0x40000; chid *= 2) {
			if (lpwfex->dwChannelMask & chid) {
				switch (chid) {
				case 0x01: printf("\tSpeaker Front Left\n"); break;
				case 0x02: printf("\tSpeaker Front Right\n"); break;
				default: printf("\tOther Channel: %x\n", chid); break;
				}				
			}
		}		
		
		printf("EXT Valid Bits Per Sample %i\n", lpwfex->Samples.wValidBitsPerSample);
		if (IsEqualGUID(lpwfex->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
			printf("EXT is ieee float\n");
		}
		else {
			printf("ERROR: EXT Other format. This program is written to only support IEEE Float data.\n");
			goto done;
		}
	}
	else {
		printf("ERROR: EXT Other format. This program is written to only support IEEE Float data.\n");
		goto done;
	}

	UINT32 DefPeriodInFrames, p2, MinPeriodInFrames, p4;
	result = pAudioClient3->GetSharedModeEnginePeriod(
		nativeFormat, &DefPeriodInFrames, &p2, &MinPeriodInFrames, &p4);
	CHKERR(result, ERROR_29);
	printf("Def Period in Frames %i\n", DefPeriodInFrames);
	printf("Fnd Period in Frames %i\n", p2);
	printf("Min Period in Frames %i\n", MinPeriodInFrames);
	printf("Max Period in Frames %i\n", p4);

	UINT32 PeriodToUse = DefPeriodInFrames;

	result = pAudioClient3->InitializeSharedAudioStream(
		StreamFlags,
		PeriodToUse,
		nativeFormat,
		nullptr);

	if (FAILED(result))
	{
		lpDeviceInfo->BufferSizeFrames = 0;
		last_error = ERROR_8;
		goto done;
	}
	else
	{
		printf("NOTE: Init is successful\n");

		lpDeviceInfo->BufferSizeFrames = PeriodToUse;
		lpDeviceInfo->SizeOfFrame = nativeFormat->nChannels *
			(nativeFormat->wBitsPerSample / 8);
		lpDeviceInfo->nChannels = nativeFormat->nChannels;

		lpDeviceInfo->FrameBuffer = (BYTE*)rta_alloc(
			lpDeviceInfo->BufferSizeFrames * lpDeviceInfo->SizeOfFrame);

		UINT32 RealBufferSizeFrames = 0;
		lpDeviceInfo->pAudioClient->GetBufferSize(&RealBufferSizeFrames);
		printf("INIT: Success; buffer size is %i [ %i ]\n",
			lpDeviceInfo->BufferSizeFrames, RealBufferSizeFrames);
		if (RealBufferSizeFrames != lpDeviceInfo->BufferSizeFrames) {
			printf("WARNING! Buffer sizes do not match!\n");
		}
		lpDeviceInfo->RealBufferSizeFrames = RealBufferSizeFrames;

		REFERENCE_TIME latency = 0;
		lpDeviceInfo->pAudioClient->GetStreamLatency(&latency);
		printf("Latency = %lli\n", latency);
	}

	// done
	retval = TRUE;

done:
	if (pMMDeviceEnumerator != NULL) pMMDeviceEnumerator->Release();
	if (last_error != NULL)
	{
		printf("LAST ERROR: %s\n", last_error);
	}
	return retval;
}

/*
void rta_capture_frames(LPRTA_DEVICE_INFO lpCaptureDeviceInfo, CAPTURE_DATA_HANDLER pHandler)
{
	HANDLE BufferEvent = NULL;
	IAudioCaptureClient *pAudioCaptureClient = NULL;
	HANDLE hTask = NULL;
	UINT32 FrameCount = 0;
	DWORD flags = 0;
	BYTE* pCapBuffer = NULL;
	BOOL handlerResult = FALSE;

	BufferEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (BufferEvent == NULL) {
		last_error = ERROR_9;
		goto done;
	}

	HRESULT hr = lpCaptureDeviceInfo->pAudioClient->SetEventHandle(BufferEvent);
	CHKERR(hr, ERROR_10);

	hr = lpCaptureDeviceInfo->pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pAudioCaptureClient);
	CHKERR(hr, ERROR_11);

	DWORD dwTaskIndex = 0;
	hTask = AvSetMmThreadCharacteristics(L"Pro Audio", &dwTaskIndex);
	if (hTask == NULL) {
		last_error = ERROR_12;
		goto done;
	}

	//lpRenderDeviceInfo->pAudioClient->GetBufferSize(&FrameCount);
	//pAudioRenderClient->GetBuffer(FrameCount, &pRenBuffer);
	//pAudioRenderClient->ReleaseBuffer(FrameCount, AUDCLNT_BUFFERFLAGS_SILENT);
	//printf("silent %i frames\n", FrameCount);

	printf("\nPress any key...\n");
	_getch();

	hr = lpCaptureDeviceInfo->pAudioClient->Start();
	CHKERR(hr, ERROR_13);

	LARGE_INTEGER perff;
	LARGE_INTEGER count1;
	LARGE_INTEGER count2;
	LARGE_INTEGER count3;

	QueryPerformanceFrequency(&perff);

	printf("Counts per second %lli\n", perff.QuadPart);
	//LONGLONG CountsPerMilli = perff.QuadPart / (LONGLONG)1000;

	DWORD WaitResult = 0;

	for (;;) {

		QueryPerformanceCounter(&count1);

		// wait for buffer event
		WaitResult = WaitForSingleObject(BufferEvent, 2000);
		if (WaitResult != WAIT_OBJECT_0) {
			if (WaitResult == WAIT_TIMEOUT) {
				printf("\n\nERROR: Timeout elapsed on wait for capture buffer\n");
			}
			else {
				printf("\n\nERROR: WaitForSingleObject -> 0x%08x\n", WaitResult);
			}
			break;
		}

		QueryPerformanceCounter(&count2);

		// get capture buffer
		hr = pAudioCaptureClient->GetBuffer(&pCapBuffer, &FrameCount, &flags, NULL, NULL);
		if (FAILED(hr)) {
			printf("\n\nERORR: pAudioCaptureClient->GetBuffer\n");
			break;
		}

		// copy data to frame buffer
		memcpy(lpCaptureDeviceInfo->FrameBuffer, pCapBuffer, 
			FrameCount * lpCaptureDeviceInfo->SizeOfFrame);

		// release capture buffer
		hr = pAudioCaptureClient->ReleaseBuffer(FrameCount);
		if (FAILED(hr)) {
			printf("\n\nERORR: pAudioCaptureClient->ReleaseBuffer\n");
			break;
		}

		// run data through handler
		if (pHandler != NULL)
			pHandler(lpCaptureDeviceInfo->FrameBuffer, FrameCount, &handlerResult);

		// find peaks
		short peak = 0;
		//for (FRAME* pFrame = lpCaptureDeviceInfo->FrameBuffer; pFrame < lpCaptureDeviceInfo->FrameBuffer + FrameCount; pFrame++) {
			//if (pFrame->left > peak) peak = pFrame->left;
		//}

		// break if done
		if (TRUE == handlerResult) break;

		QueryPerformanceCounter(&count3);

		float deltaSeconds = ((float)(count3.QuadPart - count1.QuadPart) * 1000.0f) / ((float)perff.QuadPart);
		printf("Wait = %5lli cts; Proc = %5lli cts; Total = %5lli cts; Total = %6.3f ms, Peak = %.5i\n",
			(count2.QuadPart - count1.QuadPart),
			(count3.QuadPart - count2.QuadPart),
			(count3.QuadPart - count1.QuadPart), deltaSeconds, peak);

	}

	printf("\n");

	hr = lpCaptureDeviceInfo->pAudioClient->Stop();
	CHKERR(hr, ERROR_16);

	BOOL bResult = AvRevertMmThreadCharacteristics(hTask);
	if (bResult == 0) {
		last_error = ERROR_17;
	}

done:
	if (BufferEvent != NULL) CloseHandle(BufferEvent);
	if (pAudioCaptureClient != NULL) pAudioCaptureClient->Release();

}
*/

/*
void rta_capture_and_render_frames(LPRTA_DEVICE_INFO lpCaptureDeviceInfo, 
	LPRTA_DEVICE_INFO lpRenderDeviceInfo, CAPTURE_DATA_HANDLER pHandler)
{
	HANDLE BufferEvent = NULL;
	IAudioCaptureClient *pAudioCaptureClient = NULL;
	IAudioRenderClient *pAudioRenderClient = NULL;
	HANDLE hTask = NULL;
	UINT32 FrameCount = 0;
	DWORD flags = 0;
	BYTE* pCapBuffer = NULL;
	BYTE* pRenBuffer = NULL;
	BOOL handlerResult = FALSE;

	BufferEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (BufferEvent == NULL) {
		last_error = ERROR_9;
		goto done;
	}

	HRESULT hr = lpCaptureDeviceInfo->pAudioClient->SetEventHandle(BufferEvent);
	CHKERR(hr, ERROR_10);

	hr = lpCaptureDeviceInfo->pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pAudioCaptureClient);
	CHKERR(hr, ERROR_11);

	hr = lpRenderDeviceInfo->pAudioClient->GetService(__uuidof(IAudioRenderClient), (void**)&pAudioRenderClient);
	CHKERR(hr, ERROR_18);

	DWORD dwTaskIndex = 0;
	hTask = AvSetMmThreadCharacteristics(L"Pro Audio", &dwTaskIndex);
	if (hTask == NULL) {
		last_error = ERROR_12;
		goto done;
	}

	//lpRenderDeviceInfo->pAudioClient->GetBufferSize(&FrameCount);
	//pAudioRenderClient->GetBuffer(FrameCount, &pRenBuffer);
	//pAudioRenderClient->ReleaseBuffer(FrameCount, AUDCLNT_BUFFERFLAGS_SILENT);
	//printf("silent %i frames\n", FrameCount);

	printf("\nPress any key...\n");
	_getch();

	hr = lpCaptureDeviceInfo->pAudioClient->Start();
	CHKERR(hr, ERROR_13);

	hr = lpRenderDeviceInfo->pAudioClient->Start();
	CHKERR(hr, ERROR_19);

	LARGE_INTEGER perff;
	LARGE_INTEGER count1;
	LARGE_INTEGER count2;
	LARGE_INTEGER count3;

	QueryPerformanceFrequency(&perff);

	printf("Counts per second %lli\n", perff.QuadPart);
	//LONGLONG CountsPerMilli = perff.QuadPart / (LONGLONG)1000;

	DWORD WaitResult = 0;

	for (;;) {

		QueryPerformanceCounter(&count1);

		// wait for buffer event
		WaitResult = WaitForSingleObject(BufferEvent, 2000);
		if (WaitResult != WAIT_OBJECT_0) {
			if (WaitResult == WAIT_TIMEOUT) {
				printf("\n\nERROR: Timeout elapsed on wait for capture buffer\n");
			}
			else {
				printf("\n\nERROR: WaitForSingleObject -> 0x%08x\n", WaitResult);
			}
			break;
		}

		QueryPerformanceCounter(&count2);

		// get capture buffer
		hr = pAudioCaptureClient->GetBuffer(&pCapBuffer, &FrameCount, &flags, NULL, NULL);
		if (FAILED(hr)) {
			printf("\n\nERORR: pAudioCaptureClient->GetBuffer\n");
			break;
		}

		// copy data to frame buffer
		memcpy(lpCaptureDeviceInfo->FrameBuffer, pCapBuffer, 
			FrameCount * lpCaptureDeviceInfo->SizeOfFrame);

		// release capture buffer
		hr = pAudioCaptureClient->ReleaseBuffer(FrameCount);
		if (FAILED(hr)) {
			printf("\n\nERORR: pAudioCaptureClient->ReleaseBuffer\n");
			break;
		}

		// run data through handler
		if(pHandler != NULL)
			pHandler(lpCaptureDeviceInfo->FrameBuffer, FrameCount, &handlerResult);


		// get render buffer
		//printf("\nrequesting %i frames from render client\n", FrameCount);
		UINT32 padding = 0;
		lpRenderDeviceInfo->pAudioClient->GetCurrentPadding(&padding);
		printf("padding = %4i; ", padding);
		hr = pAudioRenderClient->GetBuffer(FrameCount, &pRenBuffer);
		if (FAILED(hr)) {
			if (hr == AUDCLNT_E_BUFFER_TOO_LARGE) {
				printf("\n\nERROR: pAudioRenderClient->GetBuffer -> Buffer too large\n");
			}
			else {
				printf("\n\nERROR: pAudioRenderClient->GetBuffer -> 0x%08x\n", hr);
			}
			break;
		}

		// copy the capture buffer data to the render buffer
		memcpy(pRenBuffer, lpCaptureDeviceInfo->FrameBuffer, 
			FrameCount * lpCaptureDeviceInfo->SizeOfFrame);

		// release the render buffer
		hr = pAudioRenderClient->ReleaseBuffer(FrameCount, 0);
		if (FAILED(hr)) {
			printf("\n\nERORR: pAudioRenderClient->ReleaseBuffer\n");
			break;
		}

		// find peaks
		short peak = 0;
		//for (FRAME* pFrame = lpCaptureDeviceInfo->FrameBuffer; pFrame < lpCaptureDeviceInfo->FrameBuffer + FrameCount; pFrame++) {
			//if (pFrame->left > peak) peak = pFrame->left;
		//}

		// break if done
		if (TRUE == handlerResult) break;

		QueryPerformanceCounter(&count3);

		float deltaSeconds = ((float)(count3.QuadPart - count1.QuadPart) * 1000.0f) / ((float)perff.QuadPart);
		printf("Wait = %5lli cts; Proc = %5lli cts; Total = %5lli cts; Total = %6.3f ms, Peak = %.5i\r",
			(count2.QuadPart - count1.QuadPart),
			(count3.QuadPart - count2.QuadPart),
			(count3.QuadPart - count1.QuadPart), deltaSeconds, peak);

	}

	printf("\n");

	hr = lpCaptureDeviceInfo->pAudioClient->Stop();
	CHKERR(hr, ERROR_16);

	hr = lpRenderDeviceInfo->pAudioClient->Stop();
	CHKERR(hr, ERROR_20);

	BOOL bResult = AvRevertMmThreadCharacteristics(hTask);
	if (bResult == 0) {
		last_error = ERROR_17;
	}

done:
	if (BufferEvent != NULL) CloseHandle(BufferEvent);
	if (pAudioCaptureClient != NULL) pAudioCaptureClient->Release();
	if (pAudioRenderClient != NULL) pAudioRenderClient->Release();

}
*/

void rta_capture_frames_rtwq(LPRTA_DEVICE_INFO lpCaptureDeviceInfo, 
	LPRTA_DEVICE_INFO lpRenderDeviceInfo, CAPTURE_DATA_HANDLER pHandler)
{

	if (lpCaptureDeviceInfo == NULL) return;

	last_error = NULL;

	IAudioCaptureClient *pAudioCaptureClient = NULL;
	IAudioRenderClient *pAudioRenderClient = NULL;

	RtaAudioHandler *pAudioHandler;
	DWORD RtwqTaskId = 0;
	IRtwqAsyncCallback *pAsyncCallback = NULL;

	pAudioHandler = new RtaAudioHandler();
	if (pAudioHandler == NULL) {
		last_error = ERROR_23;
		goto done;
	}

	HRESULT hr = lpCaptureDeviceInfo->pAudioClient->SetEventHandle(
		pAudioHandler->GetBufferEventHandle());
	CHKERR(hr, ERROR_10);

	hr = lpCaptureDeviceInfo->pAudioClient->GetService(__uuidof(IAudioCaptureClient), 
		(void**)&pAudioCaptureClient);
	CHKERR(hr, ERROR_11);

	if (lpRenderDeviceInfo != NULL) {
		hr = lpRenderDeviceInfo->pAudioClient->GetService(__uuidof(IAudioRenderClient),
			(void**)&pAudioRenderClient);
		CHKERR(hr, ERROR_18);
	}

	pAudioHandler->ConfigureClientInformation(
		pAudioCaptureClient, lpCaptureDeviceInfo,
		pAudioRenderClient, lpRenderDeviceInfo, pHandler);

	// START rtwq

	hr = RtwqStartup();
	CHKERR(hr, ERROR_21);

	hr = RtwqLockSharedWorkQueue(L"Audio", 0, &RtwqTaskId, &g_RtwqId);
	CHKERR(hr, ERROR_22);

	hr = pAudioHandler->QueryInterface(__uuidof(IRtwqAsyncCallback), (void**)&pAsyncCallback);
	CHKERR(hr, ERROR_24);

	hr = pAudioHandler->CreateAsyncResult();
	CHKERR(hr, ERROR_25);

	hr = pAudioHandler->PutWaitingWorkItem();
	CHKERR(hr, ERROR_26);

	g_RtwqStop = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (g_RtwqStop == NULL) {
		last_error = ERROR_9;
		goto done;
	}

	// END rtwq

	printf("\nPress any key...\n");
	_getch();

	hr = lpCaptureDeviceInfo->pAudioClient->Start();
	CHKERR(hr, ERROR_13);

	if (lpRenderDeviceInfo != NULL) {
		hr = lpRenderDeviceInfo->pAudioClient->Start();
		if (FAILED(hr)) {
			last_error = ERROR_19;
			lpCaptureDeviceInfo->pAudioClient->Stop();
			goto done;
		}
	}

	WINDOW_OUTPUT_BUFFER lpWindowOutputBuffer;
	lpWindowOutputBuffer.SamplesPerBuffer = lpCaptureDeviceInfo->BufferSizeFrames;
	lpWindowOutputBuffer.BufferPointer = (BYTE*)rta_alloc(lpCaptureDeviceInfo->SizeOfFrame * lpCaptureDeviceInfo->BufferSizeFrames);

	printf("Waiting for stop signal...\n");

	HANDLE hOutputWindowThread = RtaStartOutputWindow(1, &lpWindowOutputBuffer);

	HANDLE h[2] = { hOutputWindowThread, g_RtwqStop };

	WaitForMultipleObjects(2, h, TRUE, INFINITE);

	RtaCleanupOutputWindow();

	hr = lpCaptureDeviceInfo->pAudioClient->Stop();

	if (lpRenderDeviceInfo != NULL) {
		lpRenderDeviceInfo->pAudioClient->Stop();
	}

	if (lpWindowOutputBuffer.BufferPointer != NULL) {
		rta_free(lpWindowOutputBuffer.BufferPointer);
	}

done:
	if (last_error != NULL) printf("ERROR: %s\n", last_error);
	if (0 != g_RtwqId) {
		RtwqUnlockWorkQueue(g_RtwqId);
		RtwqShutdown();
	}
	if (pAsyncCallback != NULL) pAsyncCallback->Release();
	if (g_RtwqStop != NULL) CloseHandle(g_RtwqStop);
	if (pAudioCaptureClient != NULL) pAudioCaptureClient->Release();
	if (pAudioRenderClient != NULL) pAudioRenderClient->Release();

}
