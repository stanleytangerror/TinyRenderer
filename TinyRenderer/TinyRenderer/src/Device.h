#ifndef Device
#define Device

#include "Utils.h"

#include <windows.h>
#include <tchar.h>
#include <chrono>

//////////////////////////////////
// ��Ⱦ�豸
//////////////////////////////////

using UINT32 = unsigned int;

struct device_t {
	//transform_t transform;      // ����任��
	int width;                  // ���ڿ��
	int height;                 // ���ڸ߶�
	UINT32 **framebuffer;      // ���ػ��棺framebuffer[y] ����� y��
	float **zbuffer;            // ��Ȼ��棺zbuffer[y] Ϊ�� y��ָ��
	UINT32 **texture;          // ����ͬ����ÿ������
	int tex_width;              // ������
	int tex_height;             // ����߶�
	float max_u;                // ��������ȣ�tex_width - 1
	float max_v;                // �������߶ȣ�tex_height - 1
	int render_state;           // ��Ⱦ״̬
	UINT32 background;         // ������ɫ
	UINT32 foreground;         // �߿���ɫ
};

#define RENDER_STATE_WIREFRAME      1		// ��Ⱦ�߿�
#define RENDER_STATE_TEXTURE        2		// ��Ⱦ����
#define RENDER_STATE_COLOR          4		// ��Ⱦ��ɫ

// �豸��ʼ����fbΪ�ⲿ֡���棬�� NULL �������ⲿ֡���棨ÿ�� 4�ֽڶ��룩
void device_init(device_t *device, int width, int height, void *fb);

// ��� framebuffer �� zbuffer
void device_clear(device_t *device, int mode);

// ɾ���豸
void device_destroy(device_t *device);

// ����
void device_pixel(device_t *device, int x, int y, UINT32 color);

//////////////////////////////////
// Win32 ���ڼ�ͼ�λ��ƣ�Ϊ device �ṩһ�� DibSection �� FB
//////////////////////////////////
extern int screen_w, screen_h, screen_exit;
extern int screen_mx, screen_my, screen_mb;
extern int screen_keys[512];	// ��ǰ���̰���״̬

extern unsigned char *screen_fb;		// frame buffer
extern long screen_pitch;

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

void flush_buffer(device_t & device, Buffer2D<Vec4f> const & buffer, size_t width, size_t height);

inline long long current_million_seconds()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();
}

#endif