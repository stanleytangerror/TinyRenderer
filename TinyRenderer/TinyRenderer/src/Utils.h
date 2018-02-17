#ifndef Types
#define Types

#define _USE_MATH_DEFINES // for C++  
#include <cmath>

#include <Eigen\Core>
#include <vector>
#include <chrono>
#include <iostream>

using RGBA32 = unsigned int;

using Vec2i = Eigen::Vector2i;
using Vec2f = Eigen::Vector2f;
using Vec3f = Eigen::Vector3f;
using Vec4f = Eigen::Vector4f;

using Mat3f = Eigen::Matrix3f;
using Mat4f = Eigen::Matrix4f;

/* bottem left corner is (0, 0) */
template <typename T>
class Buffer2D
{
public:
	Buffer2D(size_t width, size_t height) :
		m_width(width), m_height(height),
		m_storage(height * width, {})
	{}

	T& coeff(int x, int y)
	{
		return m_storage[y * m_width + x];
	}

	T const & coeff(int x, int y) const
	{
		return m_storage[y * m_width + x];
	}

	void clear(T const & val)
	{
		for (auto & v : m_storage)
			v = val;
	}

private:
	size_t m_width, m_height;
	std::vector<T> m_storage;
};

enum class MSAA 
{
	Standard, MSAAx4
};

template <typename T>
using QuadOf = std::array<std::array<T, 2>, 2>;

struct VSInPart
{
	//int gl_VertexID;
	//int gl_InstanceID;
};

struct VSOutPart
{
	Vec4f gl_Position;
	float gl_PointSize;
	float * gl_ClipDistance;
};

struct FSInPart
{
	Vec4f gl_FragCoord;
	bool gl_FrontFacing;
	Vec2f gl_PointCoord;
};

struct FSOutPart
{
	float gl_FragDepth;
	Vec4f out_color;
};


//Mat4f view_mat(Vec3f const & eye, Vec3f const & dir, Vec3f const & up)
//{
//
//}

inline Mat4f proj_mat(float fovy_deg, float aspect, float near, float far)
{
	auto tmp = std::tan(M_PI * 2 - fovy_deg * 0.5f);
	Mat4f mat = Mat4f::Zero();
	mat.coeffRef(0, 0) = tmp;
	mat.coeffRef(1, 1) = tmp * aspect;
	mat.coeffRef(2, 2) = -(far + near) / (far - near);
	mat.coeffRef(2, 3) = -2 * far * near / (far - near);
	mat.coeffRef(3, 2) = -1.0f;
	return mat;
}

inline float lerp(float v0, float v1, float t)
{
	return v0 * (1.0f - t) + v1 * t;
}

inline Vec4f lerp(Vec4f const & v0, Vec4f const & v1, float t)
{
	return v0 * (1.0f - t) + v1 * t;
}

#endif
