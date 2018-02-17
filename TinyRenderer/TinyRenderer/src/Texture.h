#ifndef Texture
#define Texture


#include "Utils.h"

struct FilterNearest {};
struct FilterBilinear {};
struct FilterTrilinear {};

template <typename FilterMode, typename TexType, typename RetType>
struct Sample2D;

template <typename TexType, typename RetType>
struct Sample2D<FilterNearest, TexType, RetType>
{
private:
	TexType m_tex;

public:
	Sample2D(TexType const & tex) : m_tex(tex) {}

	RetType operator() (float x, float y) const
	{
		return m_tex.nearest(x, y);
	}
};

template <typename TexType, typename RetType>
struct Sample2D<FilterBilinear, TexType, RetType>
{
private:
	TexType m_tex;

public:
	Sample2D(TexType const & tex) : m_tex(tex) {}

	RetType operator() (float x, float y) const
	{
		return m_tex.bilinear(x, y);
	}
};

template <typename TexType, typename RetType>
struct Sample2D<FilterTrilinear, TexType, RetType>
{
private:
	TexType m_tex;

public:
	Sample2D(TexType const & tex) : m_tex(tex) {}

	RetType operator() (float x, float y, float d) const
	{
		return m_tex.trilinear(x, y, d);
	}
};

#endif