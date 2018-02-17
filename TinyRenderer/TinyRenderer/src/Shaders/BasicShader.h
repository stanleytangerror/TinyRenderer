#ifndef BasicShader
#define BasicShader

#include "../Utils.h"
#include "../Texture.h"

class Texture1
{
private:
	int edge_len = 80;
	int grid_len = 10;

	int at(int x, int y) const
	{
		return (x / grid_len + y / grid_len) % 2;
	}

public:
	float nearest(float x, float y) const
	{
		x = clamp<float>(x, 0.0f, 1.0f);
		y = clamp<float>(y, 0.0f, 1.0f);

		return at(x * (edge_len - 1), y * (edge_len - 1));
	}

	float bilinear(float x, float y) const
	{
		x = clamp(x, 0.0f, 1.0f);
		y = clamp(y, 0.0f, 1.0f);
		
		x *= edge_len - 1; y *= edge_len - 1;
		int xi = std::floor(x), yi = std::floor(y);

		return lerp(
			lerp(at(xi, yi),at(xi + 1, yi), x - xi),
			lerp(at(xi, yi + 1), at(xi + 1, yi + 1), x - xi),
			y - yi);
	}
};

struct Uniform
{
	Sample2D<FilterBilinear, Texture1, float> texture;
	Mat4f wvp;
};

struct VSIn : public VSInPart
{
	Vec3f position;
	Vec2f tex_coord;
};

struct VSOut : public VSOutPart
{
	Vec2f tex_coord;
};

struct FSIn : public FSInPart
{
	Vec2f tex_coord;
	float tex_coord_derivative;
};

struct FSOut : public FSOutPart
{
	//Vec4f color;
};

struct VertexShader
{
	VSOut operator() (VSIn const & vsin, Uniform const & uni)
	{
		VSOut vsout;
		vsout.gl_Position = uni.wvp * Vec4f{ vsin.position.x(), vsin.position.y(), vsin.position.z(), 1.0f };
		vsout.tex_coord = vsin.tex_coord;
		return vsout;
	}
};

inline VSOut lerp(VSOut const & vsout0, VSOut const & vsout1, float t)
{
	VSOut vsout;
	vsout.gl_Position = vsout0.gl_Position * (1.0f - t) + vsout1.gl_Position * t;
	vsout.tex_coord = vsout0.tex_coord * (1.0f - t) + vsout1.tex_coord * t;
	return vsout;
}

QuadOf<FSIn> quad_interp(QuadOf<Vec3f> const & quad_bary_correct, VSOut const & vsout0, VSOut const & vsout1, VSOut const & vsout2)
{
	QuadOf<FSIn> quad_fsin;
	
	for (int i = 0; i < 2; ++i) for (int j = 0; j < 2; ++j)
	{
		auto & fsin = quad_fsin[i][j];
		auto const & bary_correct = quad_bary_correct[i][j];
		fsin.gl_FragCoord =
			bary_correct.x() * vsout0.gl_Position
			+ bary_correct.y() * vsout1.gl_Position
			+ bary_correct.z() * vsout2.gl_Position;
		fsin.tex_coord =
			bary_correct.x() * vsout0.tex_coord
			+ bary_correct.y() * vsout1.tex_coord
			+ bary_correct.z() * vsout2.tex_coord;
	}
	Vec2f tex_coord_ddx = 0.5f *
		(-quad_fsin[0][0].tex_coord + quad_fsin[0][1].tex_coord +
			-quad_fsin[1][0].tex_coord + quad_fsin[1][1].tex_coord);
	Vec2f tex_coord_ddy = 0.5f *
		(-quad_fsin[0][0].tex_coord - quad_fsin[0][1].tex_coord +
			quad_fsin[1][0].tex_coord + quad_fsin[1][1].tex_coord);

	float tex_coord_derivative = (std::sqrt)((std::max)(tex_coord_ddx.squaredNorm(), tex_coord_ddy.squaredNorm()));
	for (int i = 0; i < 2; ++i) for (int j = 0; j < 2; ++j)
		quad_fsin[i][j].tex_coord_derivative = tex_coord_derivative;

	return quad_fsin;
}

struct FragmentShader
{
	FSOut operator() (FSIn const & fsin, Uniform const & uni)
	{
		float tex = uni.texture(fsin.tex_coord.x(), fsin.tex_coord.y());
		FSOut fsout;
		fsout.gl_FragDepth = fsin.gl_FragCoord.z();
		Vec3f color = tex * Vec3f{ 0.4f, 0.0f, 0.0f } +Vec3f{ 0.6f, 0.0f, 0.0f };
		fsout.out_color = Vec4f{ color.x(), color.y(), color.z(), 1.0f };
		return fsout;
	}
};

inline void fsout_aa(FSOut & fsout, float aa_ratio)
{
	fsout.out_color *= aa_ratio;
}

#endif
