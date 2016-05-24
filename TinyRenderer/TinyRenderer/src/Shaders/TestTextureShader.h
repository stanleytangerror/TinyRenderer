#ifndef TEST_TEXTURE_SHADER_H
#define TEST_TEXTURE_SHADER_H

#include "..\\Types.h"

#include <algorithm>

struct In
{
	Vec3f position;
	Vec3f normal;
	Vec2f texture;
	Vec3i color;

	In(Vec3f position, Vec3f normal, Vec3i color, Vec2f texture) :
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

	Mat2f derivatives;
	Vec2f texture_coord;

};

struct FSOut
{
	Eigen::Vector4f color;
};

template <>
VSOut Shader<TestUniform, In, VSOut, int, int, FSIn, FSOut>::vertex_shader(In const & indata)
{
	VSOut res;

	res.position.x() = indata.position.x();
	res.position.y() = indata.position.y();
	res.position.z() = indata.position.z();
	res.position.w() = 1.0f;
	res.position = m_uniform.projection * m_uniform.view * m_uniform.model * res.position;

	//res.normal.x() = indata.normal.x();
	//res.normal.y() = indata.normal.y();
	//res.normal.z() = indata.normal.z();
	//res.normal.w() = 1.0f;
	//res.normal = m_uniform.model_i_t * res.normal;

	//res.color.x() = indata.color.x() / 256.0f;
	//res.color.y() = indata.color.y() / 256.0f;
	//res.color.z() = indata.color.z() / 256.0f;

	res.texture = indata.texture;

	return (std::move)(res);
}

template <>
FSOut Shader<TestUniform, In, VSOut, int, int, FSIn, FSOut>::fragment_shader(FSIn const & indata)
{
	FSOut res;

	////IUINT32 color = m_uniform.texture.sample(indata.texture_coord.x(), indata.texture_coord.y());

	//// Ambient
	//float ambientStrength = 0.2f;
	//Vec3f ambient = ambientStrength * m_uniform.light_color;

	//// Diffuse 
	//Vec3f normal(indata.normal.x(), indata.normal.y(), indata.normal.z());
	//normal.normalize();
	//Vec3f pos;
	//pos << indata.frag_coord.x(), indata.frag_coord.y(), indata.frag_coord.z();
	//Vec3f light_dir = (m_uniform.light_pos - pos).normalized();
	//float diff = (std::max)(float(normal.transpose() * light_dir), 0.0f);
	//Vec3f diffuse = diff * m_uniform.light_color;

	//// Specular
	//float specularStrength = 0.8f;
	//Vec3f view_dir = (m_uniform.view_pos - pos);
	//Vec3f h_dir = (view_dir.normalized() + light_dir).normalized();
	//float spec = (std::pow)((std::max)(float(h_dir.transpose() * normal), 0.0f), 32);
	//Vec3f specular = specularStrength * spec * m_uniform.light_color;

	//Vec3f total = ambient + diffuse + specular;

	Vec2f duv_dx = indata.derivatives.block<2, 1>(0, 0);
	Vec2f duv_dy = indata.derivatives.block<2, 1>(0, 1);
	float delta_max_sqr = (std::max)(duv_dx.squaredNorm(), duv_dy.squaredNorm());
	float temp = (std::sqrt)(delta_max_sqr);
	// color
	IUINT32 color = m_uniform.texture.sample(indata.texture_coord.x(), indata.texture_coord.y(), temp);
	//IUINT32 color = m_uniform.texture.sample_bilinear_no_mipmap(indata.texture_coord.x(), indata.texture_coord.y());
	//color << total.x() * indata.color.x(), total.y() * indata.color.y(), total.z() * indata.color.z();
	//color = indata.color;
	
	res.color.x() = (std::min)(((color & 0xff0000) >> 16) / 256.0f, 1.0f);
	res.color.y() = (std::min)(((color & 0xff00) >> 8) / 256.0f, 1.0f);
	res.color.z() = (std::min)((color & 0xff) / 256.0f, 1.0f);
	res.color.w() = 1.0f;

	return (std::move)(res);
}

typedef Shader<TestUniform, In, VSOut, int, int, FSIn, FSOut> TestShader;

#endif