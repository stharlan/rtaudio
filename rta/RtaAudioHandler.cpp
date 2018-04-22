#include "stdafx.h"
#include "RtaAudioHandler.h"

extern HANDLE g_RtwqStop;
extern DWORD g_RtwqId;

RtaAudioHandler::RtaAudioHandler()
{
	this->m_cRef = 0;
	this->BufferEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	this->Priority = 1;
	this->workItemKey = NULL;
	this->pAsyncResult = NULL;
	this->perff.QuadPart = 0;
	this->count1.QuadPart = 0;
	this->count3.QuadPart = 0;
	QueryPerformanceFrequency(&this->perff);
	printf("Counts per second %lli\n", perff.QuadPart);
	this->pAudioCaptureClient = NULL;
	this->pAudioRenderClient = NULL;
	this->pCapBuffer = NULL;
	this->pRenBuffer = NULL;
	this->FrameCount = 0;
	this->flags = 0;
	this->lpCaptureDeviceInfo = NULL;
	this->lpRenderDeviceInfo = NULL;
	this->pHandler = NULL;
	this->lastCount.QuadPart = 0;
	this->LoopCounter = 0;
	this->LoopLimit = 1;
}

RtaAudioHandler::~RtaAudioHandler()
{
	if (this->pAudioCaptureClient != NULL) pAudioCaptureClient->Release();
	if (this->pAudioRenderClient != NULL) pAudioRenderClient->Release();
	if(this->BufferEvent != NULL) CloseHandle(this->BufferEvent);
	if (this->pAsyncResult != NULL) this->pAsyncResult->Release();
	this->m_cRef = 0;
}

HRESULT RtaAudioHandler::QueryInterface(REFIID riid, LPVOID* ppvObj)
{
	// Always set out parameter to NULL, validating it first.
	if (!ppvObj)
		return E_INVALIDARG;
	*ppvObj = NULL;
	if (riid == IID_IUnknown || riid == __uuidof(IRtwqAsyncCallback))
	{
		// Increment the reference count and return the pointer.
		*ppvObj = (LPVOID)this;
		AddRef();
		return NOERROR;
	}
	return E_NOINTERFACE;
}

ULONG RtaAudioHandler::AddRef()
{
	InterlockedIncrement(&this->m_cRef);
	return this->m_cRef;
}

ULONG RtaAudioHandler::Release()
{
	// Decrement the object's internal counter.
	ULONG ulRefCount = InterlockedDecrement(&this->m_cRef);
	if (0 == this->m_cRef)
	{
		delete this;
	}
	return ulRefCount;
}

STDMETHODIMP RtaAudioHandler::GetParameters(DWORD* pdwFlags, DWORD* pdwQueue)
{
	HRESULT hr = S_OK;
	*pdwFlags = 0;
	*pdwQueue = g_RtwqId;
	return hr;
}

STDMETHODIMP RtaAudioHandler::Invoke(IRtwqAsyncResult* pAsyncResult)
{

	this->LoopCounter++;
	if (this->LoopCounter >= this->LoopLimit) this->LoopCounter = 0;

	QueryPerformanceCounter(&count1);
	float deltaSeconds = ((float)(count1.QuadPart - this->lastCount.QuadPart) * 1000.0f) / ((float)perff.QuadPart);
	printf("%8lli [ %6.3f ms ]; ", (count1.QuadPart - this->lastCount.QuadPart),
		deltaSeconds);
	this->lastCount = count1;

	// get capture buffer
	HRESULT hr = this->pAudioCaptureClient->GetBuffer(
		&(this->pCapBuffer), &(this->FrameCount), &(this->flags), NULL, NULL);
	if (FAILED(hr)) {
		printf("\n\nERORR: pAudioCaptureClient->GetBuffer\n");
		goto err;
	}

	if (this->FrameCount < this->lpCaptureDeviceInfo->BufferSizeFrames) {
		this->LoopLimit++;
	}

	// copy data to frame buffer
	memcpy(this->lpCaptureDeviceInfo->FrameBuffer, this->pCapBuffer, 
		this->FrameCount * this->lpCaptureDeviceInfo->SizeOfFrame);

	// release capture buffer
	hr = this->pAudioCaptureClient->ReleaseBuffer(this->FrameCount);
	if (FAILED(hr)) {
		printf("\n\nERORR: pAudioCaptureClient->ReleaseBuffer\n");
		goto err;
	}

	// run data through handler
	BOOL handlerResult = FALSE;
	if (this->pHandler != NULL)
		pHandler(this->lpCaptureDeviceInfo->FrameBuffer, this->FrameCount, &handlerResult);

	if (this->pAudioRenderClient != NULL) {

		UINT32 padding = 0;
		this->lpRenderDeviceInfo->pAudioClient->GetCurrentPadding(&padding);
		UINT32 avail = this->lpRenderDeviceInfo->RealBufferSizeFrames - padding;
		printf("frms %4i; pd %4i; av %4i; %i; ", this->FrameCount, padding, avail, this->LoopLimit);

		// get buffer
		hr = pAudioRenderClient->GetBuffer(this->FrameCount, &this->pRenBuffer);
		if (SUCCEEDED(hr)) {

			// copy capture buffer into render buffer
			memcpy(this->pRenBuffer, this->lpCaptureDeviceInfo->FrameBuffer,
				this->FrameCount * this->lpCaptureDeviceInfo->SizeOfFrame);

			// release the render buffer
			hr = this->pAudioRenderClient->ReleaseBuffer(this->FrameCount, 0);
			if (FAILED(hr)) {
				printf("ERR: REL BUFFER; ");
			}
		} else if(hr == AUDCLNT_E_BUFFER_TOO_LARGE) {
			printf("ERR: BUFFER TOO LARGE; ");
		} else {
			printf("ERR: GET BUFFER; ");
		}
	}

	// find peaks
	//FLOAT_FRAME_2CH *pFrame = (FLOAT_FRAME_2CH*)this->lpCaptureDeviceInfo->FrameBuffer;
	//float peakup = 0;
	//float peakdn = 0;
	//for (DWORD frameId = 0; frameId < this->FrameCount; frameId++) {
		//if (pFrame[frameId].left > peakup) peakup = pFrame[frameId].left;
		//if (pFrame[frameId].left < peakdn) peakdn = pFrame[frameId].left;
	//}

	QueryPerformanceCounter(&count3);

	// send a message to the window that new data is available
	if (this->LoopCounter == 0) {
		DWORD numUOW = RtaGetNumOutputWindowBuffers();
		if (numUOW == 1) {
			LPWINDOW_OUTPUT_BUFFER lpWOB = RtaGetWindowOutputBuffers();
			if (lpWOB != NULL) {
				memcpy(lpWOB->BufferPointer, this->lpCaptureDeviceInfo->FrameBuffer,
					this->FrameCount * this->lpCaptureDeviceInfo->SizeOfFrame);
				RtaUpdateOutputWindow();
			}
		}
	}

	//deltaSeconds = ((float)(count3.QuadPart - count1.QuadPart) * 1000.0f) / ((float)perff.QuadPart);
	//printf("%8lli [ %6.3f ms ]; peakup %.4f; peakdn %.4f",
		//(count3.QuadPart - count1.QuadPart), deltaSeconds, peakup, peakdn);

	printf("\n");

	// break if done
	if (TRUE == handlerResult) {
		// stop
		goto err;
	}
	else {
		this->PutWaitingWorkItem();
	}
	return S_OK;

err:
	SetEvent(g_RtwqStop);
	return S_OK;

}

HRESULT RtaAudioHandler::CreateAsyncResult()
{
	return RtwqCreateAsyncResult(NULL, this, NULL, &(this->pAsyncResult));
}

HRESULT RtaAudioHandler::PutWaitingWorkItem()
{
	return RtwqPutWaitingWorkItem(
		this->BufferEvent, 
		this->Priority,
		this->pAsyncResult, 
		&this->workItemKey);
}
