#define USE_MY_MINI3D
#ifdef USE_MY_MINI3D

//#include "Shaders\\TestLightShader.h"
#include "Shaders\\TestTextureShader.h"
#include "Types.h"

#include <Eigen\Dense>

#include <windows.h>
#include <tchar.h>

#include <vector>
#include <map>
#include <functional>
#include <list>
#include <algorithm>
#include <iostream>
#include <numeric>


//////////////////////////////////
// 渲染设备
//////////////////////////////////
typedef struct {
	//transform_t transform;      // 坐标变换器
	int width;                  // 窗口宽度
	int height;                 // 窗口高度
	IUINT32 **framebuffer;      // 像素缓存：framebuffer[y] 代表第 y行
	float **zbuffer;            // 深度缓存：zbuffer[y] 为第 y行指针
	IUINT32 **texture;          // 纹理：同样是每行索引
	int tex_width;              // 纹理宽度
	int tex_height;             // 纹理高度
	float max_u;                // 纹理最大宽度：tex_width - 1
	float max_v;                // 纹理最大高度：tex_height - 1
	int render_state;           // 渲染状态
	IUINT32 background;         // 背景颜色
	IUINT32 foreground;         // 线框颜色
}	device_t;

#define RENDER_STATE_WIREFRAME      1		// 渲染线框
#define RENDER_STATE_TEXTURE        2		// 渲染纹理
#define RENDER_STATE_COLOR          4		// 渲染颜色

// 设备初始化，fb为外部帧缓存，非 NULL 将引用外部帧缓存（每行 4字节对齐）
void device_init(device_t *device, int width, int height, void *fb) {
	int need = sizeof(void*) * (height * 2 + 1024) + width * height * 8;
	char *ptr = (char*)malloc(need + 64);
	char *framebuf, *zbuf;
	int j;
	assert(ptr);
	device->framebuffer = (IUINT32**)ptr;
	device->zbuffer = (float**)(ptr + sizeof(void*) * height);
	ptr += sizeof(void*) * height * 2;
	device->texture = (IUINT32**)ptr;
	ptr += sizeof(void*) * 1024;
	framebuf = (char*)ptr;
	zbuf = (char*)ptr + width * height * 4;
	ptr += width * height * 8;
	if (fb != NULL) framebuf = (char*)fb;
	for (j = 0; j < height; j++) {
		device->framebuffer[j] = (IUINT32*)(framebuf + width * 4 * j);
		device->zbuffer[j] = (float*)(zbuf + width * 4 * j);
	}
	device->texture[0] = (IUINT32*)ptr;
	device->texture[1] = (IUINT32*)(ptr + 16);
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


// 设置当前纹理
//void device_set_texture(device_t *device, void *bits, long pitch, int w, int h) {
//	char *ptr = (char*)bits;
//	int j;
//	assert(w <= 1024 && h <= 1024);
//	for (j = 0; j < h; ptr += pitch, j++) 	// 重新计算每行纹理的指针
//		device->texture[j] = (IUINT32*)ptr;
//	device->tex_width = w;
//	device->tex_height = h;
//	device->max_u = (float)(w - 1);
//	device->max_v = (float)(h - 1);
//}

// 清空 framebuffer 和 zbuffer
void device_clear(device_t *device, int mode) {
	int y, x, height = device->height;
	for (y = 0; y < device->height; y++) {
		IUINT32 *dst = device->framebuffer[y];
		IUINT32 cc = (height - 1 - y) * 230 / (height - 1);
		cc = (cc << 16) | (cc << 8) | cc;
		if (mode == 0) cc = device->background;
		for (x = device->width; x > 0; dst++, x--) dst[0] = cc;
	}
	for (y = 0; y < device->height; y++) {
		float *dst = device->zbuffer[y];
		for (x = device->width; x > 0; dst++, x--) dst[0] = 0.0f;
	}
}

// 删除设备
void device_destroy(device_t *device) {
	if (device->framebuffer)
		free(device->framebuffer);
	device->framebuffer = NULL;
	device->zbuffer = NULL;
	device->texture = NULL;
}

// 画点
void device_pixel(device_t *device, int x, int y, IUINT32 color) {
	if (((IUINT32)x) < (IUINT32)device->width && ((IUINT32)y) < (IUINT32)device->height) {
		device->framebuffer[y][x] = color;
	}
}

//////////////////////////////////
// Win32 窗口及图形绘制：为 device 提供一个 DibSection 的 FB
//////////////////////////////////
int screen_w, screen_h, screen_exit = 0;
int screen_mx = 0, screen_my = 0, screen_mb = 0;
int screen_keys[512];	// 当前键盘按下状态
static HWND screen_handle = NULL;		// 主窗口 HWND
static HDC screen_dc = NULL;			// 配套的 HDC
static HBITMAP screen_hb = NULL;		// DIB
static HBITMAP screen_ob = NULL;		// 老的 BITMAP
unsigned char *screen_fb = NULL;		// frame buffer
long screen_pitch = 0;

int screen_init(int w, int h, const TCHAR *title);	// 屏幕初始化
int screen_close(void);								// 关闭屏幕
void screen_dispatch(void);							// 处理消息
void screen_update(void);							// 显示 FrameBuffer

													// win32 event handler
static LRESULT screen_events(HWND, UINT, WPARAM, LPARAM);

#ifdef _MSC_VER
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#endif

// 初始化窗口并设置标题
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


//////////////////////////////////////////////
// BMP interface
//////////////////////////////////////////////
BYTE* ConvertRGBToBMPBuffer(IUINT32 * const * Buffer, int width, int height,
	long & newsize)
{
	if ((NULL == Buffer) || (width == 0) || (height == 0))
		return NULL;

	// find the number of bytes the buffer has to be padded with and length of padded scanline :
	int padding = 0;
	int scanlinebytes = width * 3;
	while ((scanlinebytes + padding) % 4 != 0)
		padding++;
	int psw = scanlinebytes + padding;

	// calculate the size of the padded buffer and create it :
	newsize = height * psw;
	BYTE* newbuf = new BYTE[newsize];

	// Now we could of course copy the old buffer to the new one, 
	// flip it and change RGB to GRB and then fill every scanline 
	// with zeroes to the next DWORD boundary, but we are smart coders of course, 
	// so we don't bother with any actual padding at all, but just initialize the 
	// whole buffer with zeroes and then copy the color values to their new positions 
	// without touching those padding-bytes anymore :)

	memset(newbuf, 0, newsize);

	long bufpos = 0;
	long newpos = 0;
	for (int y = 0; y < height; y++) for (int x = 0; x < width; x += 1)
	{
		//bufpos = y * width + x;     // position in original buffer
		newpos = (height - y - 1) * psw + x * 3; // position in padded buffer
		newbuf[newpos] = Buffer[y][x] >> 16;       // swap r and b
		newbuf[newpos + 1] = Buffer[y][x] >> 8; // g stays
		newbuf[newpos + 2] = Buffer[y][x];     // swap b and r
	}
	return newbuf;
}

bool SaveBMP(BYTE* Buffer, int width, int height, long paddedsize, LPCTSTR bmpfile)
{
	BITMAPFILEHEADER bmfh;
	BITMAPINFOHEADER info;
	memset(&bmfh, 0, sizeof(BITMAPFILEHEADER));
	memset(&info, 0, sizeof(BITMAPINFOHEADER));

	// Next we fill the file header with data :
	bmfh.bfType = 0x4d42;       // 0x4d42 = 'BM'
	bmfh.bfReserved1 = 0;
	bmfh.bfReserved2 = 0;
	bmfh.bfSize = sizeof(BITMAPFILEHEADER) +
		sizeof(BITMAPINFOHEADER) + paddedsize;
	bmfh.bfOffBits = 0x36;

	// and the info header :
	info.biSize = sizeof(BITMAPINFOHEADER);
	info.biWidth = width;
	info.biHeight = height;
	info.biPlanes = 1;
	info.biBitCount = 24;
	info.biCompression = BI_RGB;
	info.biSizeImage = 0;
	info.biXPelsPerMeter = 0x0ec4;
	info.biYPelsPerMeter = 0x0ec4;
	info.biClrUsed = 0;
	info.biClrImportant = 0;


	HANDLE file = CreateFile(bmpfile, GENERIC_WRITE, FILE_SHARE_READ,
		NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (NULL == file)
	{
		CloseHandle(file);
		return false;
	}

	// Now we write the file header and info header :
	unsigned long bwritten;
	if (WriteFile(file, &bmfh, sizeof(BITMAPFILEHEADER),
		&bwritten, NULL) == false)
	{
		CloseHandle(file);
		return false;
	}

	if (WriteFile(file, &info, sizeof(BITMAPINFOHEADER),
		&bwritten, NULL) == false)
	{
		CloseHandle(file);
		return false;
	}

	// and finally the image data :
	if (WriteFile(file, Buffer, paddedsize, &bwritten, NULL) == false)
	{
		CloseHandle(file);
		return false;
	}

	CloseHandle(file);
	return true;
}


inline float edge_function(int ax, int ay, int bx, int by, int cx, int cy)
{
	return (cy - ay) * (bx - ax) - (cx - ax) * (by - ay);
}

FragmentTriangleTestResult fragment_triangle_test(int x, int y,
	int ax, int ay, int bx, int by, int cx, int cy,
	Vec3f & coord)
{
	typedef FragmentTriangleTestResult R;
	R result = R::INSIDE;
	coord[0] = edge_function(x, y, bx, by, cx, cy);
	coord[1] = edge_function(x, y, cx, cy, ax, ay);
	coord[2] = edge_function(x, y, ax, ay, bx, by);

	for (int _i = 0; _i < 3; ++_i)
	{
		if (coord[_i] > 0.0f)
		{
			switch (result)
			{
			case R::INSIDE: result = R::FRONT; break;
			case R::BACK: result = R::OUTSIDE; break;
			default: break;
			}
		}
		else if (coord[_i] < 0.0f)
		{
			switch (result)
			{
			case R::INSIDE: result = R::BACK; break;
			case R::FRONT: result = R::OUTSIDE; break;
			default: break;
			}
		}
		if (result == R::OUTSIDE)
			break;
	}
	return result;
}

template <typename EigenType>
Vec3f barycentric(EigenType q, EigenType p0, EigenType p1, EigenType p2)
{
	EigenType v0 = p1 - p0, v1 = p2 - p0, v2 = q - p0;
	float d00 = v0.transpose() * v0;
	float d01 = v0.transpose() * v1;
	float d11 = v1.transpose() * v1;
	float d20 = v2.transpose() * v0;
	float d21 = v2.transpose() * v1;

	float determine = d00 * d11 - d01 * d01;
	float a = (d11 * d20 - d01 * d21) / determine;
	float b = (d00 * d21 - d01 * d20) / determine;
	float c = 1.0f - a - b;

	return Vec3f(a, b, c);
}

inline void draw_point(Buffer2D<IUINT32> & buffer, int x, int y,
	int min_clip_x, int min_clip_y, int max_clip_x, int max_clip_y)
{
	if (x < min_clip_x || x > max_clip_x || y < min_clip_y || y > max_clip_y)
		return;
	buffer.coeff_ref(x, y) = 1;
}

inline void draw_scanline(Buffer2D<IUINT32> & buffer, int x_start, int x_end, int y)
{
	for (int x = x_start; x <= x_end; ++x)
	{
		buffer.coeff_ref(x, y) = 1;
	}
}


void draw_bottom_tri(Buffer2D<IUINT32> & buffer,
	int x0, int y0, int x1, int y1, int x2, int y2,
	int min_clip_x, int min_clip_y, int max_clip_x, int max_clip_y)
{
	if (x1 > x2)
		std::swap(x1, x2);

	float height = float(y2 - y0);
	float inv_k_left = (x1 - x0) / height, inv_k_right = (x2 - x0) / height;
	float x_start = float(x0), x_end = float(x0) + 0.5f;

	if (y0 < min_clip_y)
	{
		x_start += inv_k_left * (float)(min_clip_y - y0);
		x_end += inv_k_right * (float)(min_clip_y - y0);

		y0 = float(min_clip_y);
	}

	y2 = (std::min)(y2, max_clip_y);

	for (int y = y0; y <= y2; ++y)
	{
		draw_scanline(buffer, (std::max)(min_clip_x, int(x_start)), (std::min)(max_clip_x, int(x_end)), y);
		x_start += inv_k_left;
		x_end += inv_k_right;
	}

}

void draw_top_tri(Buffer2D<IUINT32> & buffer,
	int x0, int y0, int x1, int y1, int x2, int y2,
	int min_clip_x, int min_clip_y, int max_clip_x, int max_clip_y)
{
	if (x0 > x1)
		std::swap(x0, x1);

	float height = float(y2 - y0);
	float inv_k_left = (x2 - x0) / height, inv_k_right = (x2 - x1) / height;
	float x_start = float(x0), x_end = float(x1) + 0.5f;

	if (y0 < min_clip_y)
	{
		x_start += inv_k_left * (float)(min_clip_y - y0);
		x_end += inv_k_right * (float)(min_clip_y - y0);

		y0 = float(min_clip_y);
	}

	y2 = (std::min)(y2, max_clip_y);

	for (int y = y0; y <= y2; ++y)
	{
		draw_scanline(buffer, (std::max)(min_clip_x, int(x_start)), (std::min)(max_clip_x, int(x_end)), y);
		x_start += inv_k_left;
		x_end += inv_k_right;
	}
}

void draw_primitive(Buffer2D<IUINT32> & buffer,
	int x0, int y0, int x1, int y1, int x2, int y2,
	int min_clip_x, int min_clip_y, int max_clip_x, int max_clip_y)
{
	if (y0 > y1)
	{
		std::swap(x0, x1);
		std::swap(y0, y1);
	}
	if (y0 > y2)
	{
		std::swap(x0, x2);
		std::swap(y0, y2);
	}
	if (y1 > y2)
	{
		std::swap(x1, x2);
		std::swap(y1, y2);
	}
	if (y0 < min_clip_y || y2 > max_clip_y ||
		(x0 < min_clip_x && x1 < min_clip_x && x2 < min_clip_x) ||
		(x0 < min_clip_x && x1 < min_clip_x && x2 < min_clip_x))
		return;
	if (y0 == y1)
	{
		draw_top_tri(buffer, x0, y0, x1, y1, x2, y2, min_clip_x, min_clip_y, max_clip_x, max_clip_y);
	}
	else if (y1 == y2)
	{
		draw_bottom_tri(buffer, x0, y0, x1, y1, x2, y2, min_clip_x, min_clip_y, max_clip_x, max_clip_y);
	}
	else
	{
		int new_x = x0 + int(0.5f + float((y1 - y0) * (x2 - x0)) / float(y2 - y0));
		draw_bottom_tri(buffer, x0, y0, x1, y1, new_x, y1, min_clip_x, min_clip_y, max_clip_x, y1 - 1);
		draw_top_tri(buffer, new_x, y1, x1, y1, x2, y2, min_clip_x, min_clip_y, max_clip_x, max_clip_y);
	}
}

void draw_line(IUINT32 * const * buffer, int x0, int y0, int x1, int y1/*,
																	   int min_clip_x, int min_clip_y, int max_clip_x, int max_clip_y*/)
{
	/*if ((x0 < min_clip_x && x1 < min_clip_x) ||
	(x0 > max_clip_x && x1 > max_clip_x) ||
	(y0 < min_clip_y && y1 < min_clip_y) ||
	(y0 > max_clip_y && y1 > max_clip_y))
	return;*/

	/* https://segmentfault.com/a/1190000002700500
	* f(x, y) = y * delta_x - x * delta_y + x_0 * delta_y - y_0 * delta_x
	*/
	int delta_x = x1 - x0, delta_y = y1 - y0;
	int x = x0, y = y0;
	int x_end = x1, y_end = y1;
	int x_dir = x1 < x0 ? -1 : 1, y_dir = y1 < y0 ? -1 : 1;

	bool xyswaped = false;
	int f_implicit = 0;

	if ((std::abs)(delta_x) < (std::abs)(delta_y))
	{
		std::swap(delta_x, delta_y);
		std::swap(x, y);
		std::swap(x_end, y_end);
		std::swap(x_dir, y_dir);
		xyswaped = true;
	}

	if (delta_x < 0)
	{
		delta_x = -delta_x;
		delta_y = -delta_y;
		std::swap(x, x_end);
		std::swap(y, y_end);
		x_dir = -x_dir;
		y_dir = -y_dir;
	}

	for (; x <= x_end; x += x_dir)
	{
		if (xyswaped) buffer[y][x] = 1;
		else buffer[x][y] = 1;
		/* f(x+1, y) + f(x+1, y+1) < 0 => draw (x+1, y) */
		if ((y_dir >= 0) && (f_implicit + delta_y) + (f_implicit + delta_y - delta_x) < 0)
			f_implicit += delta_y;
		/* f(x+1, y) + f(x+1, y-1) > 0 => draw (x+1, y) */
		else if ((y_dir < 0) && (f_implicit + delta_y) + (f_implicit + delta_y + delta_x) > 0)
			f_implicit += delta_y;
		/* draw (x+1, y+-1) */
		else
		{
			f_implicit += delta_y - ((y_dir < 0) ? -delta_x : delta_x);
			y += y_dir;
		}
	}
}

void flush_buffer(device_t & device, Buffer2D<IUINT32> & buffer)
{
	for (int x = 0; x < buffer.m_width; x++) for (int y = 0; y < buffer.m_height; y++)
	{
		device.framebuffer[y][x] = buffer.coeff(x, buffer.m_height - y - 1);
	}
	buffer.clear((100U << 16) | (100U << 8) | 100U);
}

std::pair<TestShader, std::vector<In> > input_assembly_stage()
{
	static float time = 0.0f;
	time += 0.03f;

	// abs
	float n = 1.0f;
	float f = 1000.0f;
	float r = 1.0f;
	float t = 1.0f;

	Buffer2D<IUINT32> texture_buffer(256, 256);
	for (int x = 0; x < 256; ++x) for (int y = 0; y < 256; ++y)
	{
		if (x / 16 % 2 == 0 ^ y / 16 % 2 == 0)
			texture_buffer.coeff_ref(x, y) = (10U << 16) | (10U << 8) | 10U;
		else
			texture_buffer.coeff_ref(x, y) = (200U << 16) | (200U << 8) | 200U;
	}
	Texture2D texture(256, 256);
	texture.set_content(std::move(texture_buffer));

	auto & rot3 = rotate_matrix(Vec3f(1.0f, 1.0f, 1.0f), time);
	Mat4f rot4 = Mat4f::Identity();
	rot4.block<3, 3>(0, 0) = rot3;
	Mat4f model;
	model <<
		70.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 70.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 70.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f;
	model = rot4 * model;

	Mat4f view;
	view <<
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, -500.0f,
		0.0f, 0.0f, 0.0f, 1.0f;
	Mat4f projection;
	projection <<
		n / r, 0.0f, 0.0f, 0.0f,
		0.0f, n / t, 0.0f, 0.0f,
		0.0f, 0.0f, -(f + n) / (f - n), -2 * (f * n) / (f - n),
		0.0f, 0.0f, -1.0f, 0.0f;

	TestUniform uniform(projection, view, model, texture);

	//float positions[8][3] = {
	//	{ -1.0f, -1.0f, -1.0f },
	//	{ -1.0f, -1.0f, 1.0f },
	//	{ 1.0f, -1.0f, 1.0f },
	//	{ 1.0f, -1.0f, -1.0f },
	//	{ -1.0f, 1.0f, -1.0f },
	//	{ -1.0f, 1.0f, 1.0f },
	//	{ 1.0f, 1.0f, 1.0f },
	//	{ 1.0f, 1.0f, -1.0f } };

	float positions[4][3] = {
		{ -1.0f, -1.0f, -1.0f },
		{ -1.0f, -1.0f, 1.0f },
		{ 1.0f, -1.0f, 1.0f },
		{ 1.0f, -1.0f, -1.0f } };

	float uvs[4][2] = {
		{ 0.0f, 0.0f },
		{ 0.0f, 1.0f },
		{ 1.0f, 1.0f },
		{ 1.0f, 0.0f } };

	int colors[8][3] = {
		{ 234, 184, 129 },
		{ 234, 174, 259 },
		{ 57, 56, 224 },
		{ 74, 46, 129 },
		{ 224, 174, 191 },
		{ 174, 64, 139 },
		{ 157, 98, 144 },
		{ 23, 174, 129 } };

	//Vec3i col0(100, 154, 230);
	//Vec3i col1(230, 14, 135);
	//Vec3i col2(100, 221, 31);
	//Vec3i col3(32, 223, 141);
	//Vec3f p0(8.0f, 0.0f, -4.0f);
	//Vec3f p1(0.0f, -3.0f, 7.0f);
	//Vec3f p2(-6.0f, 2.0f, -8.0f);
	//Vec3f p3(0.0f, 5.0f, 0.0f);


	std::vector<In> vertices;
	for (int i = 0; i < 8; ++i)
	{
		vertices.push_back(In(
			Vec3f(positions[i][0], positions[i][1], positions[i][2]),
			Vec3f(positions[i][0], positions[i][1], positions[i][2]),
			Vec3i(colors[i][0], colors[i][1], colors[i][2]),
			Vec2f(uvs[i][0], uvs[i][1])));
	}

	TestShader shader(uniform, false);

	return std::pair<TestShader, std::vector<In> >(std::move(shader), std::move(vertices));

}

std::vector<VSOut> vertex_shader_stage(std::vector<In> & vertices, TestShader & shader)
{
	std::vector<VSOut> vsout;
	for (auto & v : vertices)
	{
		auto vso = shader.vertex_shader(v);
		vsout.push_back(vso);
	}
	return std::move(vsout);
}

std::vector<Primitive<int> > primitive_assembly_stage()
{
	//int ebo[12][3] = {
	//	{ 0, 2, 1 },
	//	{ 2, 0, 3 },
	//	{ 0, 5, 4 },
	//	{ 0, 1, 5 },
	//	{ 5, 1, 2 },
	//	{ 5, 2, 6 },
	//	{ 6, 2, 3 },
	//	{ 3, 7, 6 },
	//	{ 4, 3, 0 },
	//	{ 4, 7, 3 },
	//	{ 4, 5, 6 },
	//	{ 4, 6, 7 } };

	int ebo[2][3] = {
		{ 0, 2, 1 },
		{ 2, 0, 3 } };

	std::cout << "primitive assembly " << std::endl;
	std::vector<Primitive<int> > primitives;
	for (auto & es : ebo)
	{
		Primitive<int> prim;
		prim.type = prim.TRIANGLE;
		prim.p0 = es[0];
		prim.p1 = es[1];
		prim.p2 = es[2];
		primitives.push_back((std::move)(prim));
	}

	return std::move(primitives);
}

QuadFragType quad_fragment_rasterize(Buffer2D<IUINT32> & buffer, Buffer2D<IUINT32> & fsbuffer,
	Buffer2D<FSIn> & fs_ins, std::vector<VSOut> & vsout, std::vector<Primitive<int> > & primitives,
	int prim_id, FaceCulling culling, int x, int y, FragmentTriangleTestResult & res, Vec3f & barycentric_coordinate)
{
	auto & prim = primitives[prim_id];
	auto & p0 = vsout[prim.p0];
	auto & p1 = vsout[prim.p1];
	auto & p2 = vsout[prim.p2];

	res = fragment_triangle_test(x, y,
		p0.position.x(), p0.position.y(),
		p1.position.x(), p1.position.y(),
		p2.position.x(), p2.position.y(),
		barycentric_coordinate);

	if (res == FragmentTriangleTestResult::OUTSIDE ||
		res == FragmentTriangleTestResult::INSIDE)
		return QuadFragType::HELPER_OUT_OF_RANGE;

	if (culling == FaceCulling::FRONT && res == FragmentTriangleTestResult::BACK)
		return QuadFragType::HELPER_OUT_OF_RANGE;

	return QuadFragType::RENDER;
	
}

QuadFragType quad_fragment_barycoord_correction_and_depth_test(Buffer2D<IUINT32> & buffer, Buffer2D<IUINT32> & fsbuffer,
	Buffer2D<FSIn> & fs_ins, std::vector<VSOut> & vsout, std::vector<Primitive<int> > & primitives,
	int prim_id, FaceCulling culling, int x, int y, FragmentTriangleTestResult const & res, Vec3f & barycentric_coordinate, float & depth)
{
	auto & prim = primitives[prim_id];
	auto & p0 = vsout[prim.p0];
	auto & p1 = vsout[prim.p1];
	auto & p2 = vsout[prim.p2];

	float mark = (res == FragmentTriangleTestResult::FRONT) ? 1.0f : -1.0f;
	barycentric_coordinate = mark * barycentric_coordinate;
	barycentric_coordinate = barycentric_coordinate /
		(barycentric_coordinate.x() + barycentric_coordinate.y() + barycentric_coordinate.z());

	float inv_depth0 = 1.0f / p0.position.w();
	float inv_depth1 = 1.0f / p1.position.w();
	float inv_depth2 = 1.0f / p2.position.w();

	barycentric_coordinate.x() = barycentric_coordinate.x() * inv_depth0;
	barycentric_coordinate.y() = barycentric_coordinate.y() * inv_depth1;
	barycentric_coordinate.z() = barycentric_coordinate.z() * inv_depth2;

	float inv_depth = barycentric_coordinate[0] +
		barycentric_coordinate[1] + barycentric_coordinate[2];

	depth = 1.0f / inv_depth;

	/* depth test failed, skip this fragment */
	if (fs_ins.coeff_ref(x, y).prim_id >= 0 && depth >= fs_ins.coeff_ref(x, y).depth)
		return QuadFragType::HELPER_DEPTH_TEST_FAILED;

	return QuadFragType::RENDER;
}

void rasterize_stage(Buffer2D<IUINT32> & buffer, Buffer2D<IUINT32> & fsbuffer,
	Buffer2D<FSIn> & fs_ins, std::vector<VSOut> & vsout, std::vector<Primitive<int> > & primitives,
	FaceCulling culling = FaceCulling::FRONT_AND_BACK)
{

	static int w = buffer.m_width;
	static int h = buffer.m_height;
	//static Buffer2D<IUINT32> fsbuffer(w, h);
	//static Buffer2D<FSIn> fs_ins(w, h);

	fsbuffer.clear(0);


	Mat4f to_screen;
	to_screen <<
		w / 2, 0.0f, 0.0f, 0.0f,
		0.0f, h / 2, 0.0f, 0.0f,
		0.0f, 0.0f, 0.5f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f;

	std::vector<bool> clipped;
	clipped.reserve(vsout.size());

	std::cout << "clip" << std::endl;
	for (auto & v : vsout)
	{
		float cliplen = (std::abs)(v.position.w());
		/* to homogeneous clip space */
		if (v.position.x() < -cliplen || v.position.x() > cliplen ||
			v.position.y() < -cliplen || v.position.y() > cliplen ||
			v.position.z() < -cliplen || v.position.z() > cliplen)
			clipped.push_back(true);
		else
			clipped.push_back(false);
	}

	for (int _i = 0; _i < primitives.size(); ++_i)
	{
		auto & prim = primitives[_i];
		if (!clipped[prim.p0] || !clipped[prim.p1] || !clipped[prim.p2])
			continue;
		primitives.erase(primitives.begin() + _i);
		_i -= 1;
	}

	std::cout << "to viewport space" << std::endl;
	for (auto & v : vsout)
	{
		/* projection division, to NDC coordinate */
		//v.position = v.position / v.position.w();
		v.position.x() = v.position.x() / v.position.w();
		v.position.y() = v.position.y() / v.position.w();
		v.position.z() = v.position.z() / v.position.w();

		/* to viewport coordinate */
		v.position = to_screen * v.position + Vec4f(w / 2, h / 2, 0.5f, 0.0f);

	}

	// rasterization
	std::cout << "rasterization " << std::endl;

	//#ifdef DEBUG

	/* each quad has following properties */
	static Vec3f barycentric_coordinates[4];
	static FragmentTriangleTestResult quad_test_results[4];
	static float depthes[4];
	static QuadFragType quad_masks[4];
	static Vec2f textures_coordinates[4];
	static Mat2f uv_derivatives;

	static std::pair<int, int> const quad_offsets[4] = { 
		std::pair<int, int>(0, 0),
		std::pair<int, int>(1, 0),
		std::pair<int, int>(0, 1),
		std::pair<int, int>(1, 1) };
	/* for each primitive, generate fragment shader input */
	for (int prim_id = 0; prim_id < primitives.size(); ++prim_id)
	{
		auto & prim = primitives[prim_id];
		auto & p0 = vsout[prim.p0];
		auto & p1 = vsout[prim.p1];
		auto & p2 = vsout[prim.p2];

		int minx = (std::max)(0, (int)(std::min)(p0.position.x(), (std::min)(p1.position.x(), p2.position.x())));
		int miny = (std::max)(0, (int)(std::min)(p0.position.y(), (std::min)(p1.position.y(), p2.position.y())));
		int maxx = (std::min)(w - 1, (int)(std::max)(p0.position.x(), (std::max)(p1.position.x(), p2.position.x())));
		int maxy = (std::min)(h - 1, (int)(std::max)(p0.position.y(), (std::max)(p1.position.y(), p2.position.y())));

		/* dealing with each quad */
		for (int qx = minx; qx <= maxx; qx += 2) for (int qy = miny; qy <= maxy; qy += 2)
		{
			/* fist phase 
			 * skip invalid quad where no fragment lying in the triangle 
			 */
			bool valid = false;
			for (int _i = 0; _i < 4; ++_i)
			{
				int x = qx + quad_offsets[_i].first;
				int y = qy + quad_offsets[_i].second;
				quad_masks[_i] = quad_fragment_rasterize(buffer, fsbuffer, fs_ins, vsout, primitives, prim_id, culling,
					x, y, quad_test_results[_i], barycentric_coordinates[_i]);
				valid = (quad_masks[_i] == QuadFragType::RENDER) ? true : valid;
			}
			if (!valid) continue;

			/* second phase
			* correct the barycentric coordinates and run depth test
			*/
			valid = false;
			for (int _i = 0; _i < 4; ++_i)
			{
				int x = qx + quad_offsets[_i].first;
				int y = qy + quad_offsets[_i].second;
				quad_masks[_i] = quad_fragment_barycoord_correction_and_depth_test(buffer, fsbuffer, fs_ins, vsout, primitives, prim_id, culling,
					x, y, quad_test_results[_i], barycentric_coordinates[_i], depthes[_i]);
				valid = (quad_masks[_i] == QuadFragType::RENDER) ? true : valid;
			}
			if (!valid) continue;


			/* third phase
			 * get derivatives of texture coordinates (with helper fargment) 
			 */
			for (int _i = 0; _i < 4; ++_i)
			{
				textures_coordinates[_i] = (barycentric_coordinates[_i].x() * p0.texture +
					barycentric_coordinates[_i].y() * p1.texture +
					barycentric_coordinates[_i].z() * p2.texture) * depthes[_i];
			}
			uv_derivatives.block<2, 1>(0, 0) =
				(-textures_coordinates[0] + textures_coordinates[1] - textures_coordinates[2] + textures_coordinates[3]) / 2.0f;
			uv_derivatives.block<2, 1>(0, 1) =
				(-textures_coordinates[0] - textures_coordinates[1] + textures_coordinates[2] + textures_coordinates[3]) / 2.0f;

			/* fourth phase
			 * construct input of fragment shaders 
			 */
			for (int _i = 0; _i < 4; ++_i)
			{
				int x = qx + quad_offsets[_i].first;
				int y = qy + quad_offsets[_i].second;

				if (quad_masks[_i] != QuadFragType::RENDER)
					continue;
				
				auto & fsin = fs_ins.coeff_ref(x, y);
				fsin.prim_id = prim_id;
				fsin.point_coord << x, y;
				fsin.depth = depthes[_i];
				fsin.interp_coord = barycentric_coordinates[_i];

				//fsin.frag_coord = frag_coord;
				//fsin.color = barycentric_coordinate[0] * p0.color +
				//	barycentric_coordinate[1] * p1.color +
				//	barycentric_coordinate[2] * p2.color;

				fsin.color = (barycentric_coordinates[_i].x() * p0.color +
					barycentric_coordinates[_i].y() * p1.color +
					barycentric_coordinates[_i].z() * p2.color) * depthes[_i];
				fsin.normal = (barycentric_coordinates[_i].x() * p0.normal +
					barycentric_coordinates[_i].y() * p1.normal +
					barycentric_coordinates[_i].z() * p2.normal) * depthes[_i];

				fsin.texture_coord = textures_coordinates[_i];
				fsin.derivatives = uv_derivatives;

			}

		}
	}

}

void fragment_shader_stage(Buffer2D<IUINT32> & buffer, Buffer2D<IUINT32> & fsbuffer, Buffer2D<FSIn> & fs_ins, TestShader & shader)
{
	// fragment shader
	std::cout << "fragment shader" << std::endl;
	for (int y = 0; y < fsbuffer.m_height; ++y) for (int x = 0; x < fsbuffer.m_width; ++x)
	{
		FSIn & fsin = fs_ins.coeff_ref(x, y);
		if (fsin.prim_id < 0)
			continue;

		FSOut const fsout = shader.fragment_shader(fsin);

		unsigned int r = unsigned int((std::max)(0.0f, fsout.color.x() * 256.f - 1.f)) & 0xff;
		unsigned int g = unsigned int((std::max)(0.0f, fsout.color.y() * 256.f - 1.f)) & 0xff;
		unsigned int b = unsigned int((std::max)(0.0f, fsout.color.z() * 256.f - 1.f)) & 0xff;

		buffer.coeff_ref(x, y) = IUINT32((r << 16) | (g << 8) | b);
	}
}

void clear(Buffer2D<IUINT32> & buffer, Buffer2D<IUINT32> & fsbuffer, Buffer2D<FSIn> & fs_ins)
{
	fsbuffer.clear(0);
	for (int y = 0; y < fs_ins.m_height; ++y) for (int x = 0; x < fs_ins.m_width; ++x)
	{
		fs_ins.coeff_ref(x, y).prim_id = -1;
	}
}

void pipeline(Buffer2D<IUINT32> & buffer)
{
	static float w = float(buffer.m_width);
	static float h = float(buffer.m_height);
	static Buffer2D<IUINT32> fsbuffer(w, h);
	static Buffer2D<FSIn> fs_ins(w, h);

	clear(buffer, fsbuffer, fs_ins);

	auto & vsdata = input_assembly_stage();
	auto & vs_outs = vertex_shader_stage(vsdata.second, vsdata.first);
	auto & primitives = primitive_assembly_stage();
	rasterize_stage(buffer, fsbuffer, fs_ins, vs_outs, primitives, FaceCulling::FRONT_AND_BACK);
	fragment_shader_stage(buffer, fsbuffer, fs_ins, vsdata.first);

}

void render(Buffer2D<IUINT32> & buffer)
{
	static float time = 4.0f;
	int w = buffer.m_width;
	int h = buffer.m_height;

	time += 0.1f;

	Vec3f p0(400.0f, 400.0f, 0.0f);
	Vec3f r1(170.0f * (std::sin)(time), 150.0f * (std::cos)(time), 0.0f);
	Vec3f r2(210.0f * (std::cos)(time * 0.5f), 130.0f * (std::sin)(time * 0.5f), 0.0f);
	Vec3f p1 = p0 + r1;
	Vec3f p2 = p0 + r2;
	Vec3f col0(100.0f, 154.0f, 230.0f);
	Vec3f col1(230.0f, 14.0f, 135.0f);
	Vec3f col2(100.0f, 221.0f, 31.0f);

	Vec3f barycentric_coordinate;

	int minx = (std::max)(0, (int)(std::min)(p0.x(), (std::min)(p1.x(), p2.x())));
	int miny = (std::max)(0, (int)(std::min)(p0.y(), (std::min)(p1.y(), p2.y())));
	int maxx = (std::min)(w - 1, (int)(std::max)(p0.x(), (std::max)(p1.x(), p2.x())));
	int maxy = (std::min)(h - 1, (int)(std::max)(p0.y(), (std::max)(p1.y(), p2.y())));

	draw_point(buffer, p1.x(), p1.y(), 0, 0, w, h);
	draw_point(buffer, p2.x(), p2.y(), 0, 0, w, h);

	for (int x = minx; x <= maxx; ++x) for (int y = miny; y <= maxy; ++y)
	{
		auto res = fragment_triangle_test(x, y,
			p0.x(), p0.y(),
			p1.x(), p1.y(),
			p2.x(), p2.y(),
			barycentric_coordinate);

		if (res == FragmentTriangleTestResult::OUTSIDE ||
			res == FragmentTriangleTestResult::INSIDE)
			continue;

		barycentric_coordinate = barycentric_coordinate.cwiseAbs();
		barycentric_coordinate = barycentric_coordinate /
			(barycentric_coordinate.x() + barycentric_coordinate.y() + barycentric_coordinate.z());

		auto & color = barycentric_coordinate.x() * col0 +
			barycentric_coordinate.y() * col1 +
			barycentric_coordinate.z() * col2;

		unsigned int r = color.x();
		unsigned int g = color.y();
		unsigned int b = color.z();

		buffer.coeff_ref(x, y) = ((r << 16) | (g << 8) | b);
	}

	draw_point(buffer, p1.x(), p1.y(), 0, 0, w, h);
	draw_point(buffer, p2.x(), p2.y(), 0, 0, w, h);

}

int main()
{
	device_t device;
	int states[] = { RENDER_STATE_TEXTURE, RENDER_STATE_COLOR, RENDER_STATE_WIREFRAME };
	int indicator = 0;
	int kbhit = 0;
	float alpha = 1;
	float pos = 3.5;

	Buffer2D<IUINT32> buffer(800, 800);
	buffer.clear((100U << 16) | (100U << 8) | 100U);

	if (screen_init(800, 800, _T("test")))
		return -1;

	device_init(&device, 800, 800, screen_fb);
	//camera_at_zero(&device, 3, 0, 0);

	//init_texture(&device);
	device.render_state = RENDER_STATE_TEXTURE;

	while (screen_exit == 0 && screen_keys[VK_ESCAPE] == 0) {
		screen_dispatch();
		device_clear(&device, 1);

		pipeline(buffer);
		//render(buffer);

		flush_buffer(device, buffer);

		screen_update();
		Sleep(1);
	}
	return 0;

}

#endif