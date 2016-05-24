#ifndef TEST_LIGHT_SHADER_H
#define TEST_LIGHT_SHADER_H

#include "..\\Types.h"

#include <algorithm>

struct VSIn
{
	Vec3f position;
	Vec3f normal;
	Vec2f texture;
	Vec3i color;

	VSIn(Vec3f position, Vec3f normal, Vec3i color, Vec2f texture) :
		position(position), normal(normal), color(color), texture(texture) {}
};

struct VSOut
{
	Vec4f position;
	Vec4f normal;
	Vec2f texture;
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
	Eigen::Vector2i point_coord;
	Eigen::Vector4f frag_coord;
	Vec3f color;
	Vec4f normal;
	int prim_id;
	bool front_facing;
	float depth = (std::numeric_limits<float>::max)();
	Vec3f interp_coord;
	Vec2f texture_coord;

};

struct FSOut
{
	Eigen::Vector4f color;
};

template <>
VSOut Shader<TestUniform, VSIn, VSOut, int, int, FSIn, FSOut>::vertex_shader(VSIn const & indata)
{
	VSOut res;

	res.position.x() = indata.position.x();
	res.position.y() = indata.position.y();
	res.position.z() = indata.position.z();
	res.position.w() = 1.0f;
	res.position = m_uniform.projection * m_uniform.view * m_uniform.model * res.position;

	res.normal.x() = indata.normal.x();
	res.normal.y() = indata.normal.y();
	res.normal.z() = indata.normal.z();
	res.normal.w() = 1.0f;
	res.normal = m_uniform.model_i_t * res.normal;

	res.color.x() = indata.color.x() / 256.0f;
	res.color.y() = indata.color.y() / 256.0f;
	res.color.z() = indata.color.z() / 256.0f;

	res.texture = indata.texture;

	return (std::move)(res);
}

template <>
FSOut Shader<TestUniform, VSIn, VSOut, int, int, FSIn, FSOut>::fragment_shader(FSIn const & indata)
{
	FSOut res;

	//IUINT32 color = m_uniform.texture.sample(indata.texture_coord.x(), indata.texture_coord.y());

	// Ambient
	float ambientStrength = 0.2f;
	Vec3f ambient = ambientStrength * m_uniform.light_color;

	// Diffuse 
	Vec3f normal(indata.normal.x(), indata.normal.y(), indata.normal.z());
	normal.normalize();
	Vec3f pos;
	pos << indata.frag_coord.x(), indata.frag_coord.y(), indata.frag_coord.z();
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
	m_uniform.texture.sample(indata.texture_coord.x(), indata.texture_coord.y(), 1.0f);
	Vec3f color;
	color << total.x() * indata.color.x(), total.y() * indata.color.y(), total.z() * indata.color.z();
	//color = indata.color;

	res.color.x() = (std::min)(color.x(), 1.0f);
	res.color.y() = (std::min)(color.y(), 1.0f);
	res.color.z() = (std::min)(color.z(), 1.0f);
	res.color.w() = 1.0f;

	return (std::move)(res);
}

typedef Shader<TestUniform, VSIn, VSOut, int, int, FSIn, FSOut> TestShader;

std::pair<TestShader, std::vector<VSIn> > input_assembly_stage()
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


	std::vector<VSIn> vertices;
	for (int i = 0; i < 8; ++i)
	{
		vertices.push_back(VSIn(
			Vec3f(positions[i][0], positions[i][1], positions[i][2]),
			Vec3f(positions[i][0], positions[i][1], positions[i][2]),
			Vec3i(colors[i][0], colors[i][1], colors[i][2]),
			Vec2f(uvs[i][0], uvs[i][1])));
	}

	TestShader shader(uniform, false);

	return std::pair<TestShader, std::vector<VSIn> >(std::move(shader), std::move(vertices));

}

#endif