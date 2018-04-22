
#define RTAM_CLOSE_WINDOW WM_USER + 1 
#define RTAM_UPDATE_WINDOW WM_USER + 2

typedef struct {
	DWORD SamplesPerBuffer;
	BYTE* BufferPointer;
	//ASIOSampleType SampleType;
} WINDOW_OUTPUT_BUFFER, *LPWINDOW_OUTPUT_BUFFER;

HANDLE RtaStartOutputWindow(DWORD dwNumOutputWindowBuffers, LPWINDOW_OUTPUT_BUFFER lpOutputWindowBuffers);
void RtaStopOutputWindow();
void RtaCleanupOutputWindow();
void RtaUpdateOutputWindow();
DWORD RtaGetNumOutputWindowBuffers();
LPWINDOW_OUTPUT_BUFFER RtaGetWindowOutputBuffers();
