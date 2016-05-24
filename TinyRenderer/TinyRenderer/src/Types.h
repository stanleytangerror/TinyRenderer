#ifndef TYPES_H
#define TYPES_H

#include <Eigen\Dense>
#include <vector>
#include <memory>
#include <functional>

///////////////////////////////
// basic types
///////////////////////////////

typedef unsigned int IUINT32;

typedef Eigen::Vector2i Vec2i;
typedef Eigen::Vector2f Vec2f;
typedef Eigen::Vector2d Vec2d;

typedef Eigen::Vector3i Vec3i;
typedef Eigen::Vector3f Vec3f;
typedef Eigen::Vector3d Vec3d;

typedef Eigen::Vector4i Vec4i;
typedef Eigen::Vector4f Vec4f;
typedef Eigen::Vector4d Vec4d;

typedef Eigen::Matrix2i Mat2i;
typedef Eigen::Matrix2f Mat2f;
typedef Eigen::Matrix2d Mat2d;

typedef Eigen::Matrix3i Mat3i;
typedef Eigen::Matrix3f Mat3f;
typedef Eigen::Matrix3d Mat3d;

typedef Eigen::Matrix4i Mat4i;
typedef Eigen::Matrix4f Mat4f;
typedef Eigen::Matrix4d Mat4d;

///////////////////////////////
// enumerates
///////////////////////////////

enum class FragmentTriangleTestResult 
{ FRONT, BACK, OUTSIDE, INSIDE };

enum class FaceCulling 
{ FRONT, FRONT_AND_BACK };

enum class QuadFragType
{ HELPER_DEPTH_TEST_FAILED, HELPER_OUT_OF_RANGE, RENDER };

///////////////////////////////
// math operators
///////////////////////////////
inline int random_int(int min, int max)
{
	return min + (rand() % (int)(max - min + 1));
}

template <typename Type>
inline Type interpolation(Type p, Type x, Type y)
{
	Type dis = (x - y);
	if (dis == 0)
		return 0;
	return (p - y) / dis;
}

template <typename Type>
inline Type bound(Type p, Type min, Type max)
{
	if (p < min)
		p = min;
	if (p > max)
		p = max;
	return p;
}

inline Eigen::Matrix3f rotate_matrix(Vec3f axis, float theta)
{
	float h = theta / 2.0f;
	Eigen::Quaternionf q;
	q.vec() = axis * (std::sin)(h);
	q.w() = (std::cos)(h);
	q.normalize();
	return q.toRotationMatrix();
}

inline float edge_function(int ax, int ay, int bx, int by, int cx, int cy)
{
	return (cy - ay) * (bx - ax) - (cx - ax) * (by - ay);
}

///////////////////////////////
// shader prototypes
///////////////////////////////
template <typename Header, typename Content>
class Wrapper
{
public:
	Header header;
	Content content;

	Wrapper() = default;

	Wrapper(Header && header) :
		header(header), content() {}

	Wrapper(Header && header, Content && content) :
		header(header), content(content) {}
};

struct VSInHeader
{
	Vec3f position;
	VSInHeader(Vec3f position) :
		position(position) {}
};

struct VSOutHeader
{
	Vec4f position;
};

struct FSInHeader
{
	int prim_id;
	Vec2i point_coord;
	Vec4f frag_coord;
	float depth = (std::numeric_limits<float>::max)();
	Vec3f interp_coord;
};

struct FSOutHeader
{
	Eigen::Vector4f color;
};

template <typename Uniform,
	typename VSIn, typename VSOut,
	typename GSIn, typename GSOut,
	typename FSIn, typename FSOut>
class Shader
{
public:
	Shader(Uniform const & uniform, bool use_gs = false) :
		m_uniform(uniform), m_use_gs(use_gs) {}

	Wrapper<VSOutHeader, VSOut> vertex_shader(Wrapper<VSInHeader, VSIn> const & in);
	//GSOut geometry_shader(GSIn const & in);
	Wrapper<FSOutHeader, FSOut> fragment_shader(Wrapper<FSInHeader, FSIn> const & in);

private:
	Uniform m_uniform;
	bool m_use_gs;

};

//////////////////////////////////
// primitives
//////////////////////////////////
template <typename VertexType>
struct Primitive
{
	enum { POINT, LINE, TRIANGLE } type;
	VertexType p0;
	VertexType p1;
	VertexType p2;
};


/////////////////////////////////
// buffers
/////////////////////////////////

template <typename Type>
class Buffer2D
{
public:
	Buffer2D(int width, int height) :
		m_width(width), m_height(height),
		m_data(new Type[m_height * m_width]),
		m_buffer(new Type*[m_width])
	{
		for (int _j = 0; _j < m_width; ++_j)
			m_buffer[_j] = &(m_data[_j * m_height]);
	}

	Buffer2D() = delete;

	Buffer2D(Buffer2D const & other) :
		m_width(other.m_width), m_height(other.m_height),
		m_data(new Type[m_height * m_width]),
		m_buffer(new Type*[m_width])
	{
		for (int _j = 0; _j < m_width; ++_j)
			m_buffer[_j] = &(m_data[_j * m_height]);
		size_t s = m_height * m_width * sizeof(Type);
		memcpy_s(m_data.get(), s, other.m_data.get(), s);
	}

	Buffer2D & operator=(Buffer2D const & other)
	{
		if (this == &other)
			return *this;
		size_t s = m_height * m_width * sizeof(Type);
		memcpy_s(m_data, s, other.m_data, s);
		return *this;
	}

	Buffer2D(Buffer2D && other) :
		m_width(std::move(other.m_width)), m_height(std::move(other.m_height)),
		m_data(std::move(other.m_data)), m_buffer(std::move(other.m_buffer))
	{}

	Buffer2D & operator=(Buffer2D && other) = delete;

	Type & coeff_ref(int x, int y) const
	{
		return m_buffer[y][x];
	}

	Type coeff(int x, int y) const
	{
		return m_buffer[y][x];
	}

	void clear(Type const & val)
	{
		for (int _i = 0; _i < m_width * m_height; ++_i)
			m_data[_i] = val;
	}

	void clear(std::function<Type(int, int)> coordinate_map)
	{
		for (int x = 0; x < m_width; ++x) for (int y = 0; y < m_height; ++y)
			coeff_ref(x, y) = coordinate_map(x, y);
	}

	int const m_width, m_height;

private:
	std::unique_ptr< Type * []> m_buffer;
	std::unique_ptr<Type[]> m_data;
};

class GBuffer
{
public:
	size_t const m_width, m_height;
	Buffer2D<IUINT32> m_z_buffer;
	Buffer2D<IUINT32> m_stencil_buffer;
	Buffer2D<IUINT32> m_rgb_buffer;
};

//////////////////////////////////////
// textures
//////////////////////////////////////
class Texture2D
{
public:
	Texture2D(int width, int height, bool with_mipmap = true) :
		m_width(width), m_height(height), m_with_mipmap(with_mipmap), m_level(1)
	{
		int w_level = 1;
		while (width != 1 && width % 2 == 0)
		{
			width /= 2;
			w_level += 1;
		}
		if (width != 1) m_with_mipmap = false;
		int h_level = 1;
		while (height != 1 && height % 2 == 0)
		{
			height /= 2;
			h_level += 1;
		}
		if (height != 1) m_with_mipmap = false;
		if (m_with_mipmap)
		{
			m_level = (m_width >= m_height) ? w_level : h_level;
			m_mipmap.reserve(m_level);
		}
	}

	void set_content(Buffer2D<IUINT32> && buffer)
	{
		m_mipmap.push_back(buffer);
		if (!m_with_mipmap) return;

		for (int level_no = 1; level_no < m_level; ++level_no)
		{
			Buffer2D<IUINT32> & parent = m_mipmap[level_no - 1];
			Buffer2D<IUINT32> new_buffer(parent.m_width >> 1, parent.m_height >> 1);
			for (unsigned int x = 0; x < new_buffer.m_width; ++x) for (unsigned int y = 0; y < new_buffer.m_height; ++y)
			{
				new_buffer.coeff_ref(x, y) = (parent.coeff(x << 1, y << 1) + parent.coeff((x << 1) + 1, y << 1) +
					parent.coeff(x << 1, (y << 1) + 1) + parent.coeff((x << 1) + 1, (y << 1) + 1)) >> 2;
			}
			m_mipmap.push_back(std::move(new_buffer));
		}
	}

	IUINT32 sample(float x, float y, float pixel_len)
	{
		if (!m_with_mipmap || pixel_len >= 1.0f)
			return IUINT32(sample_bilinear(m_mipmap[0], x, y));
			
		float len = 1.0f / (std::pow)(2.0f, m_level);
		int level_no = 0;
		while (len <= pixel_len && level_no < m_level)
		{
			len *= 2.0f;
			level_no += 1;
		}
		if (level_no == m_level)
			return sample_bilinear(m_mipmap[m_level - 1], x, y);

		float t = interpolation(pixel_len, len / 2.0f, len);
		IUINT32 val_floor = sample_bilinear(m_mipmap[level_no - 1], x, y);
		IUINT32 val_ceil = sample_bilinear(m_mipmap[level_no], x, y);
		
		unsigned int r = unsigned int(float((val_floor & 0xff0000) >> 16) * t + float((val_ceil & 0xff0000) >> 16) * (1 - t));
		unsigned int g = unsigned int(float((val_floor & 0xff00) >> 8) * t + float((val_ceil & 0xff00) >> 8) * (1 - t));
		unsigned int b = unsigned int(float(val_floor & 0xff) * t + float(val_ceil & 0xff) * (1 - t));

		return IUINT32(r << 16 | g << 8 | b);

	}

	IUINT32 sample_bilinear_no_mipmap(float x, float y)
	{
		return IUINT32(sample_bilinear(m_mipmap[0], x, y));
	}

	IUINT32 sample_nearest_no_mipmap(float x, float y)
	{
		return sample_nearest(m_mipmap[0], x, y);
	}

private:
	int m_width, m_height;
	
	bool m_with_mipmap;
	std::vector<Buffer2D<IUINT32> > m_mipmap;
	int m_level;

	IUINT32 sample_bilinear(Buffer2D<IUINT32> const & texture, float x, float y)
	{
		static IUINT32 samples[4];
		static float factors[4];
		float xi = x * texture.m_width;
		float yi = y * texture.m_height;
		float x_floor = bound((std::floor)(xi), 0.0f, (float)texture.m_width - 1);
		float x_ceil = bound((std::ceil)(xi), 0.0f, (float)texture.m_width - 1);
		float y_floor = bound((std::floor)(yi), 0.0f, (float)texture.m_width - 1);
		float y_ceil = bound((std::ceil)(yi), 0.0f, (float)texture.m_width - 1);
		float xt = interpolation(xi, x_floor, x_ceil);
		float yt = interpolation(yi, y_floor, y_ceil);
		/* get sample value */
		samples[0] = texture.coeff((int)x_floor, (int)y_floor);
		samples[1] = texture.coeff((int)x_ceil, (int)y_floor);
		samples[2] = texture.coeff((int)x_floor, (int)y_ceil);
		samples[3] = texture.coeff((int)x_ceil, (int)y_ceil);
		factors[0] = xt * yt;
		factors[1] = (1 - xt) * yt;
		factors[2] = xt * (1 - yt);
		factors[3] = (1 - xt) * (1 - yt);
		unsigned int r = 0, g = 0, b = 0;
		for (int _i = 0; _i < 4; ++_i)
		{
			r += unsigned int(float((samples[_i] & 0xff0000) >> 16) * factors[_i]);
			g += unsigned int(float((samples[_i] & 0xff00) >> 8) * factors[_i]);
			b += unsigned int(float(samples[_i] & 0xff) * factors[_i]);
		}
		return IUINT32(r << 16 | g << 8 | b);
	}

	IUINT32 sample_nearest(Buffer2D<IUINT32> const & texture, float x, float y)
	{
		float xi = x * texture.m_width;
		float yi = y * texture.m_height;
		if (xi < 0.0f) xi = 0.0f;
		else if (xi > texture.m_width - 1.0f) xi = texture.m_width - 1.0f;
		if (yi < 0.0f) yi = 0.0f;
		else if (yi > texture.m_height - 1.0f) yi = texture.m_height - 1.0f;
		return texture.coeff(xi, yi);
	}
};

#endif

