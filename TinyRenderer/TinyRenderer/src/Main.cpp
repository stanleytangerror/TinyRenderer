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

/////////////////////////////////
// pipeline functions
/////////////////////////////////
FragmentTriangleTestResult fragment_triangle_test(int x, int y,
	int ax, int ay, int bx, int by, int cx, int cy,
	Vec3f & coord)
{
	using R = FragmentTriangleTestResult;
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

void flush_buffer(device_t & device, Buffer2D<IUINT32> & buffer)
{
	for (int x = 0; x < buffer.m_width; x++) for (int y = 0; y < buffer.m_height; y++)
	{
		device.framebuffer[y][x] = buffer.coeff(x, buffer.m_height - y - 1);
	}
	buffer.clear([](int x, int y)->IUINT32
	{
		y = y / 3;
		return IUINT32((y << 16) | (y << 8) | y);
	});
}

template <typename Shader, typename VSIn, typename VSOut>
vector_with_eigen<Wrapper<VSOutHeader, VSOut>> vertex_shader_stage(vector_with_eigen<Wrapper<VSInHeader, VSIn>> & vertices, Shader & shader)
{
	vector_with_eigen<Wrapper<VSOutHeader, VSOut>> vsout;
	for (auto & v : vertices)
	{
		auto vso = shader.vertex_shader(v);
		vsout.push_back(vso);
	}
	return std::move(vsout);
}

template <typename VSOut>
QuadFragType quad_fragment_rasterize(
	vector_with_eigen<Wrapper<VSOutHeader, VSOut>> & vsout, 
	std::vector<Primitive<int> > & primitives, int prim_id, 
	FaceCulling culling, int x, int y, 
	FragmentTriangleTestResult & res, Vec3f & barycentric_coordinate)
{
	auto & prim = primitives[prim_id];
	auto & p0 = vsout[prim.p0];
	auto & p1 = vsout[prim.p1];
	auto & p2 = vsout[prim.p2];

	res = fragment_triangle_test(x, y,
		p0.header.position.x(), p0.header.position.y(),
		p1.header.position.x(), p1.header.position.y(),
		p2.header.position.x(), p2.header.position.y(),
		barycentric_coordinate);

	if (res == FragmentTriangleTestResult::OUTSIDE ||
		res == FragmentTriangleTestResult::INSIDE)
		return QuadFragType::HELPER_OUT_OF_RANGE;

	if (culling == FaceCulling::FRONT && res == FragmentTriangleTestResult::BACK)
		return QuadFragType::HELPER_OUT_OF_RANGE;

	return QuadFragType::RENDER;
	
}

template <typename VSOut, typename FSIn>
QuadFragType quad_fragment_barycoord_correction_and_depth_test(
	Buffer2D<Wrapper<FSInHeader, FSIn>> & fs_ins, vector_with_eigen<Wrapper<VSOutHeader, VSOut>> & vsout, std::vector<Primitive<int> > & primitives,
	int prim_id, int x, int y, FragmentTriangleTestResult const & res, Vec3f & barycentric_coordinate, float & depth)
{
	auto & prim = primitives[prim_id];
	auto & p0 = vsout[prim.p0];
	auto & p1 = vsout[prim.p1];
	auto & p2 = vsout[prim.p2];

	float mark = (res == FragmentTriangleTestResult::FRONT) ? 1.0f : -1.0f;
	barycentric_coordinate = mark * barycentric_coordinate;
	barycentric_coordinate = barycentric_coordinate /
		(barycentric_coordinate.x() + barycentric_coordinate.y() + barycentric_coordinate.z());

	float inv_depth0 = 1.0f / p0.header.position.w();
	float inv_depth1 = 1.0f / p1.header.position.w();
	float inv_depth2 = 1.0f / p2.header.position.w();

	barycentric_coordinate.x() = barycentric_coordinate.x() * inv_depth0;
	barycentric_coordinate.y() = barycentric_coordinate.y() * inv_depth1;
	barycentric_coordinate.z() = barycentric_coordinate.z() * inv_depth2;

	float inv_depth = barycentric_coordinate[0] +
		barycentric_coordinate[1] + barycentric_coordinate[2];

	depth = 1.0f / inv_depth;

	/* depth test */
	if (fs_ins.coeff_ref(x, y).header.prim_id >= 0 && depth >= fs_ins.coeff_ref(x, y).header.depth)
		return QuadFragType::HELPER_DEPTH_TEST_FAILED;

	return QuadFragType::RENDER;
}

template <typename Shader, typename VSOut, typename FSIn>
void rasterize_stage(Shader & shader, Buffer2D<IUINT32> & buffer, Buffer2D<IUINT32> & fsbuffer,
	Buffer2D<Wrapper<FSInHeader, FSIn>> & fs_ins, vector_with_eigen<Wrapper<VSOutHeader, VSOut>> & vsout, std::vector<Primitive<int> > & primitives,
	FaceCulling culling = FaceCulling::FRONT_AND_BACK)
{

	static int w = buffer.m_width;
	static int h = buffer.m_height;
	//static Buffer2D<IUINT32> fsbuffer(w, h);
	//static Buffer2D<Wrapper<FSInHeader, FSIn>> fs_ins(w, h);

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
		float cliplen = (std::abs)(v.header.position.w());
		/* to homogeneous clip space */
		if (v.header.position.x() < -cliplen || v.header.position.x() > cliplen ||
			v.header.position.y() < -cliplen || v.header.position.y() > cliplen ||
			v.header.position.z() < -cliplen || v.header.position.z() > cliplen)
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
		v.header.position.x() = v.header.position.x() / v.header.position.w();
		v.header.position.y() = v.header.position.y() / v.header.position.w();
		v.header.position.z() = v.header.position.z() / v.header.position.w();

		/* to viewport coordinate */
		v.header.position = to_screen * v.header.position + Vec4f(w / 2, h / 2, 0.5f, 0.0f);

	}

	// rasterization
	std::cout << "rasterization " << std::endl;

	/* each quad has following properties */
	static Vec3f barycentric_coordinates[4];
	static FragmentTriangleTestResult quad_test_results[4];
	static float depthes[4];
	static QuadFragType quad_masks[4];

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

		int minx = (std::min)({ 
			(std::floor)(p0.header.position.x()),
			(std::floor)(p1.header.position.x()),
			(std::floor)(p2.header.position.x()) });
		int miny = (std::min)({
			(std::floor)(p0.header.position.y()),
			(std::floor)(p1.header.position.y()),
			(std::floor)(p2.header.position.y()) });
		int maxx = (std::max)({
			(std::ceil)(p0.header.position.x()),
			(std::ceil)(p1.header.position.x()),
			(std::ceil)(p2.header.position.x()) });
		int maxy = (std::max)({
			(std::ceil)(p0.header.position.y()),
			(std::ceil)(p1.header.position.y()),
			(std::ceil)(p2.header.position.y()) });

		minx = (std::max)(0, (minx % 2 == 0) ? minx : (minx - 1));
		miny = (std::max)(0, (miny % 2 == 0) ? miny : (miny - 1));
		maxx = (std::min)(w - 2, (maxx % 2 == 0) ? maxx : (maxx + 1));
		maxy = (std::min)(h - 2, (maxy % 2 == 0) ? maxy : (maxy + 1));

		/* dealing with each quad */
		for (int qx = minx; qx <= maxx; qx += 2) for (int qy = miny; qy <= maxy; qy += 2)
		{
			/* first phase 
			 * skip invalid quad where no fragment lying in the triangle 
			 */
			bool valid = false;
			for (int _i = 0; _i < 4; ++_i)
			{
				int x = qx + quad_offsets[_i].first;
				int y = qy + quad_offsets[_i].second;
				quad_masks[_i] = quad_fragment_rasterize<VSOut>(vsout, primitives, prim_id, culling,
					x, y, quad_test_results[_i], barycentric_coordinates[_i]);
				/* quad_masks[_i] = {RENDER, HELPER_OUT_OF_RANGE} */
				if (quad_masks[_i] == QuadFragType::RENDER) valid = true;
				if (quad_masks[_i] == QuadFragType::RENDER)
					buffer.coeff_ref(x, y) = 255 << 16;
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
				QuadFragType mask = quad_fragment_barycoord_correction_and_depth_test(fs_ins, vsout, primitives, prim_id,
					x, y, quad_test_results[_i], barycentric_coordinates[_i], depthes[_i]);
				/* mask = {RENDER, HELPER_DEPTH_TEST_FAILED} 
				 * quad_masks[_i] = {RENDER, HELPER_OUT_OF_RANGE} 
				 */
				quad_masks[_i] = (mask == QuadFragType::RENDER) ? quad_masks[_i] : mask;
				if (quad_masks[_i] == QuadFragType::RENDER) valid = true;
			}
			if (!valid) continue;

			/* third phase
			 * get texture coordinates and their derivatives (with helper fargment)
			 */
			for (int _i = 0; _i < 4; ++_i)
			{
				int x = qx + quad_offsets[_i].first;
				int y = qy + quad_offsets[_i].second;
				shader.interpolate(fs_ins.coeff_ref(x, y).content, depthes[_i],
					p0.content, p1.content, p2.content, barycentric_coordinates[_i]);
			}

			shader.quad_derivative(fs_ins.coeff_ref(qx, qy).content, fs_ins.coeff_ref(qx + 1, qy).content,
				fs_ins.coeff_ref(qx, qy + 1).content, fs_ins.coeff_ref(qx + 1, qy + 1).content);
			
			/* fourth phase
			 * construct header of fragment shaders 
			 */
			for (int _i = 0; _i < 4; ++_i)
			{
				int x = qx + quad_offsets[_i].first;
				int y = qy + quad_offsets[_i].second;

				auto & fsin = fs_ins.coeff_ref(x, y);

				if (quad_masks[_i] != QuadFragType::RENDER)
				{
					//fsin.header.prim_id = -1;
					continue;
				}
				
				fsin.header.prim_id = prim_id;
				fsin.header.point_coord << x, y;
				fsin.header.depth = depthes[_i];
				fsin.header.interp_coord = barycentric_coordinates[_i];

			}

		}
	}

}

template <typename Shader, typename FSIn, typename FSOut>
void fragment_shader_stage(Buffer2D<IUINT32> & buffer, Buffer2D<IUINT32> & fsbuffer, Buffer2D<Wrapper<FSInHeader, FSIn>> & fs_ins, Shader & shader)
{
	// fragment shader
	std::cout << "fragment shader" << std::endl;
	for (int y = 0; y < fsbuffer.m_height; ++y) for (int x = 0; x < fsbuffer.m_width; ++x)
	{
		Wrapper<FSInHeader, FSIn> & fsin = fs_ins.coeff_ref(x, y);
		if (fsin.header.prim_id < 0)
			continue;

		Wrapper<FSOutHeader, FSOut> const fsout = shader.fragment_shader(fsin);

		unsigned int r = unsigned int((std::max)(0.0f, fsout.header.color.x() * 256.f - 1.f)) & 0xff;
		unsigned int g = unsigned int((std::max)(0.0f, fsout.header.color.y() * 256.f - 1.f)) & 0xff;
		unsigned int b = unsigned int((std::max)(0.0f, fsout.header.color.z() * 256.f - 1.f)) & 0xff;

		buffer.coeff_ref(x, y) = IUINT32((r << 16) | (g << 8) | b);
	}
}

template <typename FSIn>
void clear(Buffer2D<IUINT32> & buffer, Buffer2D<IUINT32> & fsbuffer, Buffer2D<Wrapper<FSInHeader, FSIn>> & fs_ins)
{
	fsbuffer.clear(0);
	for (int y = 0; y < fs_ins.m_height; ++y) for (int x = 0; x < fs_ins.m_width; ++x)
	{
		fs_ins.coeff_ref(x, y).header.prim_id = -1;
	}
}

template <typename Shader, typename VSIn, typename VSOut, typename FSIn, typename FSOut>
void pipeline(Buffer2D<IUINT32> & buffer)
{
	static float w = float(buffer.m_width);
	static float h = float(buffer.m_height);
	static Buffer2D<IUINT32> fsbuffer(w, h);
	static Buffer2D<Wrapper<FSInHeader, FSIn>> fs_ins(w, h);

	clear(buffer, fsbuffer, fs_ins);

	auto & vsdata = (input_assembly_stage<Shader, VSIn>())();
	Shader & shader = std::get<0>(vsdata);
	vector_with_eigen<Wrapper<VSInHeader, VSIn> > & vs_ins = std::get<1>(vsdata);
	std::vector<Primitive<int> > & primitives = std::get<2>(vsdata);
	
	auto & vs_outs = vertex_shader_stage<Shader, VSIn, VSOut>(vs_ins, shader);

	//auto & primitives = primitive_assembly_stage(ebo);

	rasterize_stage<Shader, VSOut, FSIn>(shader, buffer, fsbuffer, fs_ins, vs_outs, primitives, FaceCulling::FRONT_AND_BACK);
	fragment_shader_stage<Shader, FSIn, FSOut>(buffer, fsbuffer, fs_ins, shader);

}

int main()
{
	device_t device;
	int states[] = { RENDER_STATE_TEXTURE, RENDER_STATE_COLOR, RENDER_STATE_WIREFRAME };
	int indicator = 0;
	int kbhit = 0;
	float alpha = 1;
	float pos = 3.5;

	int width = 640, height = 640;
	Buffer2D<IUINT32> buffer(width, height);
	buffer.clear(0);

	if (screen_init(width, height, _T("TinyRenderer")))
		return -1;

	device_init(&device, width, height, screen_fb);
	//camera_at_zero(&device, 3, 0, 0);

	//init_texture(&device);
	device.render_state = RENDER_STATE_TEXTURE;

	while (screen_exit == 0 && screen_keys[VK_ESCAPE] == 0) {
		screen_dispatch();
		device_clear(&device, 1);

		pipeline<TestShader, VSIn, VSOut, FSIn, FSOut>(buffer);

		flush_buffer(device, buffer);

		screen_update();
		Sleep(1);
	}
	return 0;

}

#endif