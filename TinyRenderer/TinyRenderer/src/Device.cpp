#define USE_MY_MINI3D
#ifdef USE_MY_MINI3D

#include "Device.h"

////////////////////////////////////
//// ��Ⱦ�豸
////////////////////////////////////
//typedef struct {
//	//transform_t transform;      // ����任��
//	int width;                  // ���ڿ��
//	int height;                 // ���ڸ߶�
//	UINT32 **framebuffer;      // ���ػ��棺framebuffer[y] ����� y��
//	float **zbuffer;            // ��Ȼ��棺zbuffer[y] Ϊ�� y��ָ��
//	UINT32 **texture;          // ����ͬ����ÿ������
//	int tex_width;              // ������
//	int tex_height;             // ����߶�
//	float max_u;                // ��������ȣ�tex_width - 1
//	float max_v;                // �������߶ȣ�tex_height - 1
//	int render_state;           // ��Ⱦ״̬
//	UINT32 background;         // ������ɫ
//	UINT32 foreground;         // �߿���ɫ
//}	device_t;
//
//#define RENDER_STATE_WIREFRAME      1		// ��Ⱦ�߿�
//#define RENDER_STATE_TEXTURE        2		// ��Ⱦ����
//#define RENDER_STATE_COLOR          4		// ��Ⱦ��ɫ

// �豸��ʼ����fbΪ�ⲿ֡���棬�� NULL �������ⲿ֡���棨ÿ�� 4�ֽڶ��룩
void device_init(device_t *device, int width, int height, void *fb) {
	int need = sizeof(void*) * (height * 2 + 1024) + width * height * 8;
	char *ptr = (char*)malloc(need + 64);
	char *framebuf, *zbuf;
	int j;
	assert(ptr);
	device->framebuffer = (UINT32**)ptr;
	device->zbuffer = (float**)(ptr + sizeof(void*) * height);
	ptr += sizeof(void*) * height * 2;
	device->texture = (UINT32**)ptr;
	ptr += sizeof(void*) * 1024;
	framebuf = (char*)ptr;
	zbuf = (char*)ptr + width * height * 4;
	ptr += width * height * 8;
	if (fb != NULL) framebuf = (char*)fb;
	for (j = 0; j < height; j++) {
		device->framebuffer[j] = (UINT32*)(framebuf + width * 4 * j);
		device->zbuffer[j] = (float*)(zbuf + width * 4 * j);
	}
	device->texture[0] = (UINT32*)ptr;
	device->texture[1] = (UINT32*)(ptr + 16);
	memset(device->texture[0], 0, 64);
	device->tex_width = 2;
	device->tex_height = 2;
	device->max_u = 1.0f;
	device->max_v = 1.0f;
	device->width = width;
	device->height = height;
	device->background = 0xc0c0c0;
	device->foreground = 0;
	//transform_init(&device->transform, width, height);
	device->render_state = RENDER_STATE_WIREFRAME;
}

// ��� framebuffer �� zbuffer
void device_clear(device_t *device, int mode) {
	int y, x, height = device->height;
	for (y = 0; y < device->height; y++) {
		UINT32 *dst = device->framebuffer[y];
		UINT32 cc = (height - 1 - y) * 230 / (height - 1);
		cc = (cc << 16) | (cc << 8) | cc;
		if (mode == 0) cc = device->background;
		for (x = device->width; x > 0; dst++, x--) dst[0] = cc;
	}
	for (y = 0; y < device->height; y++) {
		float *dst = device->zbuffer[y];
		for (x = device->width; x > 0; dst++, x--) dst[0] = 0.0f;
	}
}

// ɾ���豸
void device_destroy(device_t *device) {
	if (device->framebuffer)
		free(device->framebuffer);
	device->framebuffer = NULL;
	device->zbuffer = NULL;
	device->texture = NULL;
}

// ����
void device_pixel(device_t *device, int x, int y, UINT32 color) {
	if (((UINT32)x) < (UINT32)device->width && ((UINT32)y) < (UINT32)device->height) {
		device->framebuffer[y][x] = color;
	}
}

//////////////////////////////////
// Win32 ���ڼ�ͼ�λ��ƣ�Ϊ device �ṩһ�� DibSection �� FB
//////////////////////////////////
int screen_w, screen_h, screen_exit = 0;
int screen_mx = 0, screen_my = 0, screen_mb = 0;
int screen_keys[512];	// ��ǰ���̰���״̬
static HWND screen_handle = NULL;		// ������ HWND
static HDC screen_dc = NULL;			// ���׵� HDC
static HBITMAP screen_hb = NULL;		// DIB
static HBITMAP screen_ob = NULL;		// �ϵ� BITMAP
unsigned char *screen_fb = NULL;		// frame buffer
long screen_pitch = 0;

int screen_init(int w, int h, const TCHAR *title);	// ��Ļ��ʼ��
int screen_close(void);								// �ر���Ļ
void screen_dispatch(void);							// ������Ϣ
void screen_update(void);							// ��ʾ FrameBuffer

													// win32 event handler
static LRESULT screen_events(HWND, UINT, WPARAM, LPARAM);

#ifdef _MSC_VER
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#endif

// ��ʼ�����ڲ����ñ���
int screen_init(int w, int h, const TCHAR *title) {
	WNDCLASS wc = { CS_BYTEALIGNCLIENT, (WNDPROC)screen_events, 0, 0, 0,
		NULL, NULL, NULL, NULL, _T("SCREEN3.1415926") };
	BITMAPINFO bi = { { sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB,
		w * h * 4, 0, 0, 0, 0 } };
	RECT rect = { 0, 0, w, h };
	int wx, wy, sx, sy;
	LPVOID ptr;
	HDC hDC;

	screen_close();

	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.hInstance = GetModuleHandle(NULL);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	if (!RegisterClass(&wc)) return -1;

	screen_handle = CreateWindow(_T("SCREEN3.1415926"), title,
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		0, 0, 0, 0, NULL, NULL, wc.hInstance, NULL);
	if (screen_handle == NULL) return -2;

	screen_exit = 0;
	hDC = GetDC(screen_handle);
	screen_dc = CreateCompatibleDC(hDC);
	ReleaseDC(screen_handle, hDC);

	screen_hb = CreateDIBSection(screen_dc, &bi, DIB_RGB_COLORS, &ptr, 0, 0);
	if (screen_hb == NULL) return -3;

	screen_ob = (HBITMAP)SelectObject(screen_dc, screen_hb);
	screen_fb = (unsigned char*)ptr;
	screen_w = w;
	screen_h = h;
	screen_pitch = w * 4;

	AdjustWindowRect(&rect, GetWindowLong(screen_handle, GWL_STYLE), 0);
	wx = rect.right - rect.left;
	wy = rect.bottom - rect.top;
	sx = (GetSystemMetrics(SM_CXSCREEN) - wx) / 2;
	sy = (GetSystemMetrics(SM_CYSCREEN) - wy) / 2;
	if (sy < 0) sy = 0;
	SetWindowPos(screen_handle, NULL, sx, sy, wx, wy, (SWP_NOCOPYBITS | SWP_NOZORDER | SWP_SHOWWINDOW));
	SetForegroundWindow(screen_handle);

	ShowWindow(screen_handle, SW_NORMAL);
	screen_dispatch();

	memset(screen_keys, 0, sizeof(int) * 512);
	memset(screen_fb, 0, w * h * 4);

	return 0;
}

int screen_close(void) {
	if (screen_dc) {
		if (screen_ob) {
			SelectObject(screen_dc, screen_ob);
			screen_ob = NULL;
		}
		DeleteDC(screen_dc);
		screen_dc = NULL;
	}
	if (screen_hb) {
		DeleteObject(screen_hb);
		screen_hb = NULL;
	}
	if (screen_handle) {
		CloseWindow(screen_handle);
		screen_handle = NULL;
	}
	return 0;
}

static LRESULT screen_events(HWND hWnd, UINT msg,
	WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_CLOSE: screen_exit = 1; break;
	case WM_KEYDOWN: screen_keys[wParam & 511] = 1; break;
	case WM_KEYUP: screen_keys[wParam & 511] = 0; break;
	default: return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}

void screen_dispatch(void) {
	MSG msg;
	while (1) {
		if (!PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) break;
		if (!GetMessage(&msg, NULL, 0, 0)) break;
		DispatchMessage(&msg);
	}
}

void screen_update(void) {
	HDC hDC = GetDC(screen_handle);
	BitBlt(hDC, 0, 0, screen_w, screen_h, screen_dc, 0, 0, SRCCOPY);
	ReleaseDC(screen_handle, hDC);
	screen_dispatch();
}


template<class T>
constexpr const T& clamp(const T& v, const T& lo, const T& hi)
{
	return (lo >= v ? lo : (v < hi ? v : hi));
}

void flush_buffer(device_t & device, Buffer2D<Vec4f> const & buffer, size_t width, size_t height)
{
	long size = 0;
	//BYTE * bmp_buffer = ConvertRGBToBMPBuffer(buffer.get_raw_buffer(), buffer.m_width, buffer.m_height, size);
	//SaveBMP(bmp_buffer, buffer.m_width, buffer.m_height, size, _T("..//01.bmp"));

	for (int x = 0; x < width; x++) for (int y = 0; y < height; y++)
	{
		auto const & val = buffer.coeff(x, y);
		//auto val = Vec4f{ 0.5f, 0.5f, 0.5f, 1.0f };
		unsigned int r = clamp<unsigned int>(val.x() * 255.f, 0, 255) & 0xff;
		unsigned int g = clamp<unsigned int>(val.y() * 255.f, 0, 255) & 0xff;
		unsigned int b = clamp<unsigned int>(val.z() * 255.f, 0, 255) & 0xff;
		unsigned int a = clamp<unsigned int>(val.w() * 255.f, 0, 255) & 0xff;
		device.framebuffer[height - y - 1][x] = UINT32((r << 16) | (g << 8) | b);
	}
}



#endif