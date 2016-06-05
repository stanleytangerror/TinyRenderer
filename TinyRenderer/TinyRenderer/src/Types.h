#ifndef TYPES_H
#define TYPES_H

#include <Eigen\Dense>
#include <Eigen\StdVector>
#include <vector>
#include <memory>
#include <functional>
#include <tuple>
#include <map>
#include <list>
#include <queue>

///////////////////////////////
// basic types
///////////////////////////////

using IUINT32 = unsigned int;

using Vec2i = Eigen::Vector2i;
using Vec2f = Eigen::Vector2f;
using Vec2d = Eigen::Vector2d;

using Vec3i = Eigen::Vector3i;
using Vec3f = Eigen::Vector3f;
using Vec3d = Eigen::Vector3d;

using Vec4i = Eigen::Vector4i;
using Vec4f = Eigen::Vector4f;
using Vec4d = Eigen::Vector4d;

using Mat2i = Eigen::Matrix2i;
using Mat2f = Eigen::Matrix2f;
using Mat2d = Eigen::Matrix2d;

using Mat3i = Eigen::Matrix3i;
using Mat3f = Eigen::Matrix3f;
using Mat3d = Eigen::Matrix3d;

using Mat4i = Eigen::Matrix4i;
using Mat4f = Eigen::Matrix4f;
using Mat4d = Eigen::Matrix4d;

/* NOTE: according to https://eigen.tuxfamily.org/dox/group__TopicStlContainers.html
 * use stl containers with eigen
 */
#if defined(WIN32)
	template <typename T>
	using vector_with_eigen = std::vector<T, Eigen::aligned_allocator<T> >;
#else
	template <typename T>
	using vector_with_eigen = std::vector<T>;
#endif

///////////////////////////////
// enumerates
///////////////////////////////

enum class PositionTriangleTestResult 
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
	q.vec() = axis.normalized() * (std::sin)(h);
	q.w() = (std::cos)(h);
	q.normalize();
	return q.toRotationMatrix();
}

inline float edge_function(float ax, float ay, float bx, float by, float cx, float cy)
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

#if defined(WIN32)
	EIGEN_MAKE_ALIGNED_OPERATOR_NEW
#endif

};

struct VSInHeader
{
	Vec3f position;
	VSInHeader(Vec3f position) :
		position(position) {}
#if defined(WIN32)
	EIGEN_MAKE_ALIGNED_OPERATOR_NEW
#endif

};

struct VSOutHeader
{
	Vec4f position;
#if defined(WIN32)
	EIGEN_MAKE_ALIGNED_OPERATOR_NEW
#endif

};

inline VSOutHeader interpolate(float t, VSOutHeader const & out0, VSOutHeader const & out1)
{
	VSOutHeader res;
	res.position = t * out0.position + (1 - t) * out1.position;
	return std::move(res);
}


struct FSInHeader
{
	int prim_id = -1;
	Vec2i point_coord;
	Vec4f frag_coord;
	float depth = (std::numeric_limits<float>::max)();
	Vec3f interp_coord;
#if defined(WIN32)
	EIGEN_MAKE_ALIGNED_OPERATOR_NEW
#endif

};

struct FSOutHeader
{
	Eigen::Vector4f color;
#if defined(WIN32)
	EIGEN_MAKE_ALIGNED_OPERATOR_NEW
#endif

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

	void interpolate(FSIn & fsin, float w, VSOut const & vsout0, VSOut const & vsout1, VSOut const & vsout2, Vec3f const & coordinate);

	void quad_derivative(FSIn & fsin0, FSIn & fsin1, FSIn & fsin2, FSIn & fsin3);

private:
	Uniform m_uniform;
	bool m_use_gs;
#if defined(WIN32)
	EIGEN_MAKE_ALIGNED_OPERATOR_NEW
#endif


};

template <typename VertexType> class Primitive;

template <typename Shader, typename VSIn>
struct input_assembly_stage
{
	std::tuple<Shader, vector_with_eigen<Wrapper<VSInHeader, VSIn> >, std::vector<Primitive<int>  > > operator() ();
};

//////////////////////////////////
// primitives
//////////////////////////////////

enum PrimitiveType
{
	ILLEGAL_TYPE = 0,
	POINT_TYPE = 1, 
	LINE_TYPE = 2, 
	TRIANGLE_TYPE = 3,
	POLYGON_TYPE = 4
}; 

template <typename IndexType>
class Primitive
{
public:
	enum class DrawMode { POINT, LINE, FILL } n_draw_mode;

	PrimitiveType m_type;

	std::vector<IndexType> m_vertices;

	Primitive() = default;
	Primitive(Primitive &&) = default;
	Primitive(Primitive const &) = default;
	Primitive & operator=(Primitive const &) = default;

};

/////////////////////////////////
// anti-aliasing
/////////////////////////////////

struct MSAASample
{
	float percent;
	float depth;
	int prim_id;

	struct Cmp
	{
		bool operator () (MSAASample const & a, MSAASample const & b) 
		{
			return a.depth < b.depth;
		}
	};
};

struct MSAA_4
{
	using queue = std::priority_queue<MSAASample, std::vector<MSAASample>, MSAASample::Cmp>;
	queue samples[4];
	int const sample_num = 4;
};

/////////////////////////////////
// buffers
/////////////////////////////////

template <typename Type>
class Buffer2D
{
public:
	int const m_width, m_height;
	
	Buffer2D(int width, int height) :
		m_width(width), m_height(height),
		m_data(new Type[m_height * m_width]),
		m_buffer(new Type*[m_height])
	{
		for (int _j = 0; _j < m_height; ++_j)
			m_buffer[_j] = &(m_data[_j * m_width]);
	}

	Buffer2D() = delete;

	Buffer2D(Buffer2D const & other) :
		m_width(other.m_width), m_height(other.m_height),
		m_data(new Type[m_height * m_width]),
		m_buffer(new Type*[m_height])
	{
		for (int _j = 0; _j < m_height; ++_j)
			m_buffer[_j] = &(m_data[_j * m_width]);
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

	void clear(std::function<void(int, int, Type &)> coordinate_map)
	{
		for (int x = 0; x < m_width; ++x) for (int y = 0; y < m_height; ++y)
		{
			coordinate_map(x, y, coeff_ref(x, y));
		}
	}

	Type const * const * get_raw_buffer() const
	{
		return m_buffer.get();
	}

#if defined(WIN32)
	EIGEN_MAKE_ALIGNED_OPERATOR_NEW
#endif


private:
	std::unique_ptr< Type * []> m_buffer;
	std::unique_ptr<Type[]> m_data;
};

class GBuffer
{
public:
	size_t const m_width, m_height;

	Buffer2D<float> m_depth_buffer;
	Buffer2D<IUINT32> m_stencil_buffer;
	Buffer2D<IUINT32> m_frame_buffer;
};


//////////////////////////////////////
// maps
//////////////////////////////////////
template <typename Type, class Container = vector_with_eigen<Type> >
class Storage2D
{
public:
	size_t const m_width, m_height;

	Storage2D(int width, int height) :
		m_width(width), m_height(height),
		m_storage(std::vector<std::unique_ptr<Container> >())
	{
		int reserve = m_height * m_width;
		m_storage.reserve(reserve);
		for (int _i = 0; _i < reserve; ++_i)
			m_storage.push_back(std::unique_ptr<Container>(new Container()));
	}

	void insert(int x, int y, Type && type, std::function<void(Container &, Type &&)> const & pusher)
	{
		pusher(coeff_ref(x, y), std::move(type));
	}

	void insert(int x, int y, Type && type)
	{
		coeff_ref(x, y).push_back(std::move(type));
	}

	Container & coeff_ref(int x, int y)
	{
		if (!(x >= 0 && x < m_width && y >= 0 && y < m_height))
		{
			std::cout << "coordinate overflow" << std::endl;
		}
		int idx = x + y * m_width;
		return *(m_storage.at(idx).get());
	}

	void clear(std::function<void(int, int, Container &)> coordinate_map)
	{
		for (int y = 0; y < m_height; ++y) for (int x = 0; x < m_width; ++x)
			coordinate_map(x, y, coeff_ref(x, y));
	}


private:
	std::vector<std::unique_ptr<Container> > m_storage;

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
			m_level = ((m_width >= m_height) ? w_level : h_level);
			m_pyramid.reserve(m_level);
			m_raw_pixel_size = ((m_width >= m_height) ? (1.0f / float(m_width)) : (1.0f / float(m_height)));
		}
	}

	void set_content(Buffer2D<IUINT32> && buffer)
	{
		m_pyramid.push_back(buffer);
		if (!m_with_mipmap) return;

		for (int level_no = 1; level_no < m_level; ++level_no)
		{
			Buffer2D<IUINT32> & parent = m_pyramid[level_no - 1];
			Buffer2D<IUINT32> new_buffer(parent.m_width >> 1, parent.m_height >> 1);
			for (unsigned int x = 0; x < new_buffer.m_width; ++x) for (unsigned int y = 0; y < new_buffer.m_height; ++y)
			{
				new_buffer.coeff_ref(x, y) = (parent.coeff(x << 1, y << 1) + parent.coeff((x << 1) + 1, y << 1) +
					parent.coeff(x << 1, (y << 1) + 1) + parent.coeff((x << 1) + 1, (y << 1) + 1)) >> 2;
			}
			m_pyramid.push_back(std::move(new_buffer));
		}
	}

	IUINT32 sample(float x, float y, float pixel_len)
	{
		if (!m_with_mipmap || pixel_len <= m_raw_pixel_size)
			return IUINT32(sample_bilinear(m_pyramid[0], x, y));
			
		float len = m_raw_pixel_size;
		int level_no = 0;
		while (pixel_len >= len && level_no < m_level)
		{
			len *= 2.0f;
			level_no += 1;
		}
		if (level_no == m_level)
			return sample_bilinear(m_pyramid[m_level - 1], x, y);

		float t = interpolation(pixel_len, len / 2.0f, len);
		IUINT32 val_floor = sample_bilinear(m_pyramid[level_no - 1], x, y);
		IUINT32 val_ceil = sample_bilinear(m_pyramid[level_no], x, y);
		
		unsigned int r = unsigned int(float((val_floor & 0xff0000) >> 16) * t + float((val_ceil & 0xff0000) >> 16) * (1 - t));
		unsigned int g = unsigned int(float((val_floor & 0xff00) >> 8) * t + float((val_ceil & 0xff00) >> 8) * (1 - t));
		unsigned int b = unsigned int(float(val_floor & 0xff) * t + float(val_ceil & 0xff) * (1 - t));

		return IUINT32(r << 16 | g << 8 | b);

	}

	IUINT32 sample_bilinear_no_mipmap(float x, float y)
	{
		return IUINT32(sample_bilinear(m_pyramid[0], x, y));
	}

	IUINT32 sample_nearest_no_mipmap(float x, float y)
	{
		return sample_nearest(m_pyramid[0], x, y);
	}

private:
	int m_width, m_height;
	
	bool m_with_mipmap;
	vector_with_eigen<Buffer2D<IUINT32> > m_pyramid;
	int m_level;
	float m_raw_pixel_size;

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

