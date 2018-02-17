#include "Utils.h"
#include "Device.h"
#include "Texture.h"
#include "RenderStages.h"
#include "Shaders\BasicShader.h"
#include <iostream>

int main()
{
	device_t device;

	std::size_t const width = 800, const height = 600;
	RenderPipeline<VertexShader, FragmentShader, Uniform, VSIn, VSOut, FSIn, FSOut> renderer(width, height);

	if (screen_init(width, height, _T("TinyRenderer")))
		return -1;

	device_init(&device, width, height, screen_fb);

	/* construct input */
	auto const p = proj_mat(90, 4/3, 1, 100);
	int cnt = 10;
	long long last_million_seconds = current_million_seconds();

	std::vector<Vec3f> pos = {
		{ 4.0f, 4.0f, 0.0f },
		{ 4.0f, -4.0f, 0.0f },
		{ -4.0f, 4.0f, 0.0f },
		{ -4.0f, -4.0f, 0.0f } };
	std::vector<Vec2f> tex_coord = {
		{ 0.0f, 0.0f },
		{ 0.0f, 1.0f },
		{ 1.0f, 0.0f },
		{ 1.0f, 1.0f } };
	std::vector<VSIn> inputs;
	for (int i = 0; i < 4; ++i)
	{
		VSIn vsin;
		vsin.position = pos[i];
		vsin.tex_coord = tex_coord[i];
		inputs.push_back(vsin);
	}

	std::vector<int> elements = { 
		2, 1, 0, 
		3, 1, 2,
		0, 1, 2,
		2, 1, 3
	};

	/* end construct input */
	
	device.render_state = RENDER_STATE_TEXTURE;

	while (screen_exit == 0 && screen_keys[VK_ESCAPE] == 0) {
		screen_dispatch();
		device_clear(&device, 1);

		/* construct input */
		cnt += 1;
		float scale = 0.02f;
		Mat4f w1; 
		w1 << 1.0f, 0.0f, 0.0f, 0.0f,  
			0.0f, std::cos(cnt * scale), -std::sin(cnt * scale), 0.0f,
			0.0f, std::sin(cnt * scale), std::cos(cnt * scale), 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f;
		Mat4f w2;
		w2 << 1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, -20.0f,
			0.0f, 0.0f, 0.0f, 1.0f;

		Mat4f wvp = p * w2 * w1;

		Uniform uniform = { Sample2D<FilterBilinear, Texture1, float>{ Texture1{} }, wvp };
		/* end construct input */

		renderer.clear_pipeline({ 0.1f, 0.1f, 0.1f, 1.0f });
		auto framebuffer = renderer.render(inputs, elements, uniform, MSAA::Standard);
		//std::cout << cnt << std::endl;

		flush_buffer(device, framebuffer, width, height);

		if (cnt % 20 == 0)
		{
			auto tmp = current_million_seconds();
			std::cout << "fps " << int(20 * 1000 / float(tmp - last_million_seconds)) << std::endl;
			last_million_seconds = tmp;
		}

		screen_update();
		Sleep(1);
	}
	return 0;

}