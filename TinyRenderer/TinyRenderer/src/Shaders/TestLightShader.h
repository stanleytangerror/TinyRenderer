#ifndef TEST_LIGHT_SHADER_H
#define TEST_LIGHT_SHADER_H

#include "..\\Types.h"

#include <algorithm>

struct TestUniform;
struct VSIn;
struct VSOut;
struct FSIn;
struct FSOut;
typedef Shader<TestUniform, VSIn, VSOut, int, int, FSIn, FSOut> TestShader;

struct VSIn
{
	Vec3f normal;
	//Vec2f texture;
	Vec3i color;

	//VSIn(Vec3f normal, Vec3i color, Vec2f texture) :
	//	normal(normal), color(color), texture(texture) {}
};

struct VSOut
{
	Vec4f normal;
	//Vec2f texture;
	Vec3f color;
};

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

struct FSIn
{
	Vec3f color;
	Vec4f normal;
	//Vec2f texture_coord;

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

	res.content.normal.x() = indata.content.normal.x();
	res.content.normal.y() = indata.content.normal.y();
	res.content.normal.z() = indata.content.normal.z();
	res.content.normal.w() = 1.0f;
	res.content.normal = m_uniform.model_i_t * res.content.normal;

	res.content.color.x() = indata.content.color.x() / 256.0f;
	res.content.color.y() = indata.content.color.y() / 256.0f;
	res.content.color.z() = indata.content.color.z() / 256.0f;

	//res.texture = indata.texture;

	return (std::move)(res);
}

template <>
Wrapper<FSOutHeader, FSOut> TestShader::fragment_shader(Wrapper<FSInHeader, FSIn> const & indata)
{
	Wrapper<FSOutHeader, FSOut> res;

	//IUINT32 color = m_uniform.texture.sample(indata.content.texture_coord.x(), indata.content.texture_coord.y());

	// Ambient
	float ambientStrength = 0.2f;
	Vec3f ambient = ambientStrength * m_uniform.light_color;

	// Diffuse 
	Vec3f normal(indata.content.normal.x(), indata.content.normal.y(), indata.content.normal.z());
	normal.normalize();
	Vec3f pos;
	pos << indata.header.frag_coord.x(), indata.header.frag_coord.y(), indata.header.frag_coord.z();
	Vec3f light_dir = (m_uniform.light_pos - pos).normalized();
	float diff = (std::max)(float(normal.transpose() * light_dir), 0.0f);
	Vec3f diffuse = diff * m_uniform.light_color;

	// Specular
	float specularStrength = 0.8f;
	Vec3f view_dir = (m_uniform.view_pos - pos);
	Vec3f h_dir = (view_dir.normalized() + light_dir).normalized();
	float spec = (std::pow)((std::max)(float(h_dir.transpose() * normal), 0.0f), 32);
	Vec3f specular = specularStrength * spec * m_uniform.light_color;

	Vec3f total = ambient + diffuse + specular;

	// color
	//m_uniform.texture.sample(indata.content.texture_coord.x(), indata.content.texture_coord.y(), 1.0f);
	Vec3f color;
	color << total.x() * indata.content.color.x(), total.y() * indata.content.color.y(), total.z() * indata.content.color.z();
	//color = indata.content.color;

	res.header.color.x() = (std::min)(color.x(), 1.0f);
	res.header.color.y() = (std::min)(color.y(), 1.0f);
	res.header.color.z() = (std::min)(color.z(), 1.0f);
	res.header.color.w() = 1.0f;

	return (std::move)(res);
}

typedef Shader<TestUniform, VSIn, VSOut, int, int, FSIn, FSOut> TestShader;

template <>
struct input_assembly_stage<TestShader, VSIn>
{
	std::tuple<TestShader, std::vector<Wrapper<VSInHeader, VSIn> >, std::vector<Primitive<int> > > operator() ()
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

		auto & rot3 = rotate_matrix(Vec3f(-1.0f, 2.0f, 1.0f), time);
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

		float positions[8][3] = {
			{ -1.0f, -1.0f, -1.0f },
			{ -1.0f, -1.0f, 1.0f },
			{ 1.0f, -1.0f, 1.0f },
			{ 1.0f, -1.0f, -1.0f },
			{ -1.0f, 1.0f, -1.0f },
			{ -1.0f, 1.0f, 1.0f },
			{ 1.0f, 1.0f, 1.0f },
			{ 1.0f, 1.0f, -1.0f } };

		//float positions[4][3] = {
		//	{ -1.0f, -1.0f, -1.0f },
		//	{ -1.0f, -1.0f, 1.0f },
		//	{ 1.0f, -1.0f, 1.0f },
		//	{ 1.0f, -1.0f, -1.0f } };

		//float uvs[4][2] = {
		//	{ 0.0f, 0.0f },
		//	{ 0.0f, 1.0f },
		//	{ 1.0f, 1.0f },
		//	{ 1.0f, 0.0f } };

		int colors[8][3] = {
			{ 157, 98, 157 },
			{ 157, 98, 157 },
			{ 157, 98, 157 },
			{ 224, 17, 224 },
			{ 57, 234, 57 },
			{ 127, 174, 127 },
			{ 127, 124, 127 },
			{ 127, 124, 127 } };

		std::vector<Wrapper<VSInHeader, VSIn> > vertices;
		for (int i = 0; i < 8; ++i)
		{
			Wrapper<VSInHeader, VSIn> vsin(VSInHeader(Vec3f(positions[i][0], positions[i][1], positions[i][2])));
			//vsin.content.texture = Vec2f(uvs[i][0], uvs[i][1]);
			vsin.content.normal = Vec3f(positions[i][0], positions[i][1], positions[i][2]);
			vsin.content.color = Vec3i(colors[i][0], colors[i][1], colors[i][2]);

			vertices.push_back(std::move(vsin));
		}

		int ebo[12][3] = {
			{ 0, 2, 1 },
			{ 2, 0, 3 },
			{ 0, 5, 4 },
			{ 0, 1, 5 },
			{ 5, 1, 2 },
			{ 5, 2, 6 },
			{ 6, 2, 3 },
			{ 3, 7, 6 },
			{ 4, 3, 0 },
			{ 4, 7, 3 },
			{ 4, 5, 6 },
			{ 4, 6, 7 } };

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

		TestShader shader(uniform, false);

		return std::make_tuple(std::move(shader), std::move(vertices), primitives);

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
//	auto & rot3 = rotate_matrix(Vec3f(-1.0f, 2.0f, 1.0f), time);
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
//	float positions[8][3] = {
//		{ -1.0f, -1.0f, -1.0f },
//		{ -1.0f, -1.0f, 1.0f },
//		{ 1.0f, -1.0f, 1.0f },
//		{ 1.0f, -1.0f, -1.0f },
//		{ -1.0f, 1.0f, -1.0f },
//		{ -1.0f, 1.0f, 1.0f },
//		{ 1.0f, 1.0f, 1.0f },
//		{ 1.0f, 1.0f, -1.0f } };
//
//	//float positions[4][3] = {
//	//	{ -1.0f, -1.0f, -1.0f },
//	//	{ -1.0f, -1.0f, 1.0f },
//	//	{ 1.0f, -1.0f, 1.0f },
//	//	{ 1.0f, -1.0f, -1.0f } };
//
//	//float uvs[4][2] = {
//	//	{ 0.0f, 0.0f },
//	//	{ 0.0f, 1.0f },
//	//	{ 1.0f, 1.0f },
//	//	{ 1.0f, 0.0f } };
//
//	int colors[8][3] = {
//		{ 157, 98, 157 },
//		{ 157, 98, 157 },
//		{ 157, 98, 157 },
//		{ 224, 17, 224 },
//		{ 57, 234, 57 },
//		{ 127, 174, 127 },
//		{ 127, 124, 127 },
//		{ 127, 124, 127 } };
//
//	std::vector<Wrapper<VSInHeader, VSIn> > vertices;
//	for (int i = 0; i < 8; ++i)
//	{
//		Wrapper<VSInHeader, VSIn> vsin(VSInHeader(Vec3f(positions[i][0], positions[i][1], positions[i][2])));
//		//vsin.content.texture = Vec2f(uvs[i][0], uvs[i][1]);
//		vsin.content.normal = Vec3f(positions[i][0], positions[i][1], positions[i][2]);
//		vsin.content.color = Vec3i(colors[i][0], colors[i][1], colors[i][2]);
//		
//		vertices.push_back(std::move(vsin));
//	}
//
//	int ebo[12][3] = {
//		{ 0, 2, 1 },
//		{ 2, 0, 3 },
//		{ 0, 5, 4 },
//		{ 0, 1, 5 },
//		{ 5, 1, 2 },
//		{ 5, 2, 6 },
//		{ 6, 2, 3 },
//		{ 3, 7, 6 },
//		{ 4, 3, 0 },
//		{ 4, 7, 3 },
//		{ 4, 5, 6 },
//		{ 4, 6, 7 } };
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
	fsin.color = w * (coordinate.x() * vsout0.color +
		coordinate.y() * vsout1.color + coordinate.z() * vsout2.color);
	fsin.normal = w * (coordinate.x() * vsout0.normal +
		coordinate.y() * vsout1.normal + coordinate.z() * vsout2.normal);
}

void TestShader::quad_derivative(FSIn & fsin0, FSIn & fsin1, FSIn & fsin2, FSIn & fsin3)
{
	//Mat2f uv_derivatives;
	//uv_derivatives.block<2, 1>(0, 0) =
	//	(-fsin0.texture_coord + fsin1.texture_coord - fsin2.texture_coord + fsin3.texture_coord) / 2.0f;
	//uv_derivatives.block<2, 1>(0, 1) =
	//	(-fsin0.texture_coord - fsin1.texture_coord + fsin2.texture_coord + fsin3.texture_coord) / 2.0f;
	//fsin0.derivatives = uv_derivatives;
	//fsin1.derivatives = uv_derivatives;
	//fsin2.derivatives = uv_derivatives;
	//fsin3.derivatives = uv_derivatives;
}

#endif