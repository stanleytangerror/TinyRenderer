#ifndef Device
#define Device

#include "Utils.h"

#include <windows.h>
#include <tchar.h>
#include <chrono>

//////////////////////////////////
// 渲染设备
//////////////////////////////////

using UINT32 = unsigned int;

struct device_t {
	//transform_t transform;      // 坐标变换器
	int width;                  // 窗口宽度
	int height;                 // 窗口高度
	UINT32 **framebuffer;      // 像素缓存：framebuffer[y] 代表第 y行
	float **zbuffer;            // 深度缓存：zbuffer[y] 为第 y行指针
	UINT32 **texture;          // 纹理：同样是每行索引
	int tex_width;              // 纹理宽度
	int tex_height;             // 纹理高度
	float max_u;                // 纹理最大宽度：tex_width - 1
	float max_v;                // 纹理最大高度：tex_height - 1
	int render_state;           // 渲染状态
	UINT32 background;         // 背景颜色
	UINT32 foreground;         // 线框颜色
};

#define RENDER_STATE_WIREFRAME      1		// 渲染线框
#define RENDER_STATE_TEXTURE        2		// 渲染纹理
#define RENDER_STATE_COLOR          4		// 渲染颜色

// 设备初始化，fb为外部帧缓存，非 NULL 将引用外部帧缓存（每行 4字节对齐）
void device_init(device_t *device, int width, int height, void *fb);

// 清空 framebuffer 和 zbuffer
void device_clear(device_t *device, int mode);

// 删除设备
void device_destroy(device_t *device);

// 画点
void device_pixel(device_t *device, int x, int y, UINT32 color);

//////////////////////////////////
// Win32 窗口及图形绘制：为 device 提供一个 DibSection 的 FB
//////////////////////////////////
extern int screen_w, screen_h, screen_exit;
extern int screen_mx, screen_my, screen_mb;
extern int screen_keys[512];	// 当前键盘按下状态

extern unsigned char *screen_fb;		// frame buffer
extern long screen_pitch;

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

void flush_buffer(device_t & device, Buffer2D<Vec4f> const & buffer, size_t width, size_t height);

inline long long current_million_seconds()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();
}

#endif