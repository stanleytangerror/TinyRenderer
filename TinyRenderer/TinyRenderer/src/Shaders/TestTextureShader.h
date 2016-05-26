#ifndef TEST_TEXTURE_SHADER_H
#define TEST_TEXTURE_SHADER_H

#include "..\\Types.h"

#include <algorithm>
#include <tuple>

struct TestUniform;
struct VSIn;
struct VSOut;
struct FSIn;
struct FSOut;
typedef Shader<TestUniform, VSIn, VSOut, int, int, FSIn, FSOut> TestShader;

struct TestUniform
{
	Mat4f projection;
	Mat4f view;
	Mat4f model;
	Mat4f model_i_t;

	Vec3f light_pos;
	Vec3f view_pos;
	Vec3f light_color;

	Texture2D texture;

	TestUniform(Mat4f projection, Mat4f view, Mat4f model, Texture2D texture) :
		projection(projection), view(view), model(model), texture(texture)
	{
		model_i_t = model.inverse().transpose();
		light_pos << -100.0f, 100.0f, 50.0f;
		view_pos << 0.0f, 0.0f, 500.0f;
		light_color << 0.8f, 0.6f, 0.6f;
	}
};

struct VSIn
{
	Vec2f texture;
};

struct VSOut
{
	Vec2f texture;
};

struct FSIn
{
	Mat2f derivatives;
	Vec2f texture_coord;
};

struct FSOut
{
};

template <>
Wrapper<VSOutHeader, VSOut> TestShader::vertex_shader(Wrapper<VSInHeader, VSIn> const & indata)
{
	Wrapper<VSOutHeader, VSOut> res;

	res.header.position.x() = indata.header.position.x();
	res.header.position.y() = indata.header.position.y();
	res.header.position.z() = indata.header.position.z();
	res.header.position.w() = 1.0f;
	res.header.position = m_uniform.projection * m_uniform.view * m_uniform.model * res.header.position;

	res.content.texture = indata.content.texture;

	return (std::move)(res);
}

template <>
Wrapper<FSOutHeader, FSOut> TestShader::fragment_shader(Wrapper<FSInHeader, FSIn> const & indata)
{
	Wrapper<FSOutHeader, FSOut> res;

	Vec2f duv_dx = indata.content.derivatives.block<2, 1>(0, 0);
	Vec2f duv_dy = indata.content.derivatives.block<2, 1>(0, 1);
	float delta_max = (std::sqrt)((std::max)(duv_dx.squaredNorm(), duv_dy.squaredNorm()));

	// color
	IUINT32 color = m_uniform.texture.sample(indata.content.texture_coord.x(), indata.content.texture_coord.y(), delta_max);

	res.header.color.x() = (std::min)(((color & 0xff0000) >> 16) / 256.0f, 1.0f);
	res.header.color.y() = (std::min)(((color & 0xff00) >> 8) / 256.0f, 1.0f);
	res.header.color.z() = (std::min)((color & 0xff) / 256.0f, 1.0f);
	res.header.color.w() = 1.0f;

	return (std::move)(res);
}

template <>
struct input_assembly_stage<TestShader, VSIn>
{
	std::tuple<TestShader, std::vector<Wrapper<VSInHeader, VSIn> >, std::vector<Primitive<int> > > operator() ()
	{
		static float time = 0.4f;
		time += 0.03f;

		// abs
		float n = 1.0f;
		float f = 300.0f;
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
			0.0f, 0.0f, 1.0f, -200.0f,
			0.0f, 0.0f, 0.0f, 1.0f;
		Mat4f projection;
		projection <<
			n / r, 0.0f, 0.0f, 0.0f,
			0.0f, n / t, 0.0f, 0.0f,
			0.0f, 0.0f, -(f + n) / (f - n), -2 * (f * n) / (f - n),
			0.0f, 0.0f, -1.0f, 0.0f;

		TestUniform uniform(projection, view, model, texture);

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

		std::vector<Wrapper<VSInHeader, VSIn> > vertices;
		for (int i = 0; i < 4; ++i)
		{
			Wrapper<VSInHeader, VSIn> vsin(VSInHeader(Vec3f(positions[i][0], positions[i][1], positions[i][2])));
			vsin.content.texture = Vec2f(uvs[i][0], uvs[i][1]);
			vertices.push_back(std::move(vsin));
		}

		int ebo[2][3] = {
			{ 0, 2, 1 },
			{ 2, 0, 3 } };

		std::vector<Primitive<int> > primitives;
		for (auto & es : ebo)
		{
			Primitive<int> prim;
			prim.type = Primitive<int>::Type::TRIANGLE;
			prim.p0 = es[0];
			prim.p1 = es[1];
			prim.p2 = es[2];
			primitives.push_back((std::move)(prim));
		}

		TestShader shader(uniform, false);

		return std::make_tuple(std::move(shader), std::move(vertices), std::move(primitives));

	}
};

//template <>
//std::tuple<TestShader, std::vector<Wrapper<VSInHeader, VSIn> >, std::vector<Primitive<int> > > input_assembly_stage<TestShader, VSIn>()
//{
//	static float time = 0.0f;
//	time += 0.03f;
//
//	// abs
//	float n = 1.0f;
//	float f = 1000.0f;
//	float r = 1.0f;
//	float t = 1.0f;
//
//	Buffer2D<IUINT32> texture_buffer(256, 256);
//	for (int x = 0; x < 256; ++x) for (int y = 0; y < 256; ++y)
//	{
//		if (x / 16 % 2 == 0 ^ y / 16 % 2 == 0)
//			texture_buffer.coeff_ref(x, y) = (10U << 16) | (10U << 8) | 10U;
//		else
//			texture_buffer.coeff_ref(x, y) = (200U << 16) | (200U << 8) | 200U;
//	}
//	Texture2D texture(256, 256);
//	texture.set_content(std::move(texture_buffer));
//
//	auto & rot3 = rotate_matrix(Vec3f(1.0f, 1.0f, 1.0f), time);
//	Mat4f rot4 = Mat4f::Identity();
//	rot4.block<3, 3>(0, 0) = rot3;
//	Mat4f model;
//	model <<
//		70.0f, 0.0f, 0.0f, 0.0f,
//		0.0f, 70.0f, 0.0f, 0.0f,
//		0.0f, 0.0f, 70.0f, 0.0f,
//		0.0f, 0.0f, 0.0f, 1.0f;
//	model = rot4 * model;
//
//	Mat4f view;
//	view <<
//		1.0f, 0.0f, 0.0f, 0.0f,
//		0.0f, 1.0f, 0.0f, 0.0f,
//		0.0f, 0.0f, 1.0f, -500.0f,
//		0.0f, 0.0f, 0.0f, 1.0f;
//	Mat4f projection;
//	projection <<
//		n / r, 0.0f, 0.0f, 0.0f,
//		0.0f, n / t, 0.0f, 0.0f,
//		0.0f, 0.0f, -(f + n) / (f - n), -2 * (f * n) / (f - n),
//		0.0f, 0.0f, -1.0f, 0.0f;
//
//	TestUniform uniform(projection, view, model, texture);
//
//	float positions[4][3] = {
//		{ -1.0f, -1.0f, -1.0f },
//		{ -1.0f, -1.0f, 1.0f },
//		{ 1.0f, -1.0f, 1.0f },
//		{ 1.0f, -1.0f, -1.0f } };
//
//	float uvs[4][2] = {
//		{ 0.0f, 0.0f },
//		{ 0.0f, 1.0f },
//		{ 1.0f, 1.0f },
//		{ 1.0f, 0.0f } };
//
//	std::vector<Wrapper<VSInHeader, VSIn> > vertices;
//	for (int i = 0; i < 4; ++i)
//	{
//		Wrapper<VSInHeader, VSIn> vsin(VSInHeader(Vec3f(positions[i][0], positions[i][1], positions[i][2])));
//		vsin.content.texture = Vec2f(uvs[i][0], uvs[i][1]);
//		vertices.push_back(std::move(vsin));
//	}
//
//	int ebo[12][3] = {
//		{ 0, 2, 1 },
//		{ 2, 0, 3 } };
//
//	std::vector<Primitive<int> > primitives;
//	for (auto & es : ebo)
//	{
//		Primitive<int> prim;
//		prim.type = prim.TRIANGLE;
//		prim.p0 = es[0];
//		prim.p1 = es[1];
//		prim.p2 = es[2];
//		primitives.push_back((std::move)(prim));
//	}
//
//	TestShader shader(uniform, false);
//
//	return std::make_tuple(std::move(shader), std::move(vertices), primitives);
//
//}

void TestShader::interpolate(FSIn & fsin, float w, VSOut const & vsout0, VSOut const & vsout1, VSOut const & vsout2, Vec3f const & coordinate)
{
	fsin.texture_coord = w * (coordinate.x() * vsout0.texture + 
		coordinate.y() * vsout1.texture + coordinate.z() * vsout2.texture);
}

void TestShader::quad_derivative(FSIn & fsin0, FSIn & fsin1, FSIn & fsin2, FSIn & fsin3)
{
	Mat2f uv_derivatives;
	uv_derivatives.block<2, 1>(0, 0) =
		(-fsin0.texture_coord + fsin1.texture_coord - fsin2.texture_coord + fsin3.texture_coord) / 2.0f;
	uv_derivatives.block<2, 1>(0, 1) =
		(-fsin0.texture_coord - fsin1.texture_coord + fsin2.texture_coord + fsin3.texture_coord) / 2.0f;
	fsin0.derivatives = uv_derivatives;
	fsin1.derivatives = uv_derivatives;
	fsin2.derivatives = uv_derivatives;
	fsin3.derivatives = uv_derivatives;
}


#endif