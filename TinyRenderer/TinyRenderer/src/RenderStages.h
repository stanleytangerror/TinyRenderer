#ifndef RENDER_STAGES_H
#define RENDER_STAGES_H

#include "Utils.h"
#include <Eigen\Core>
#include <algorithm>
#include <queue>
#include <vector>
#include <array>

inline float edge_equation(Vec2f const & p, Vec2f const & v1, Vec2f const & v2)
{
	return (p.y() - v2.y()) * (v1.x() - v2.x()) - (p.x() - v2.x()) * (v1.y() - v2.y());
}

inline bool top_left(Vec2f const & v1, Vec2f const & v2)
{
	return (v1.x() <= v2.x()) && (v1.y() <= v2.y());
}

inline Vec3f proj_correct(Vec3f bary, Vec3f const & inv_vertex_w, float inv_frag_w)
{
	auto frag_w = 1.0f / inv_frag_w;
	bary.x() *= inv_vertex_w.x() * frag_w;
	bary.y() *= inv_vertex_w.y() * frag_w;
	bary.z() *= inv_vertex_w.z() * frag_w;
	return bary;
}

template<class T>
constexpr T clamp(const T& v, const T& lo, const T& hi)
{
	return (v <= lo ? lo : (v <= hi ? v : hi));
}

/////////////////////////////////
// pipeline
/////////////////////////////////

template <
	typename VertexShader, typename FragmentShader, typename Uniform, 
	typename VSIn, typename VSOut, typename FSIn, typename FSOut
>
class RenderPipeline
{
private:
	int const m_width;
	int const m_height;

	float const eps = 1e-20f;

	std::vector<VSIn> m_vertex_attri_buffer;
	std::vector<int> m_vertex_element_buffer;

	std::vector<VSOut> m_post_vs_buffer;
	std::vector<VSOut> m_post_clip_buffer;
	std::vector<int> m_post_clip_element_buffer;

	//Buffer2D<FSOut> m_per_frag_queue_buffer;
	//Buffer2D<int> m_per_frag_mark;
	
	Buffer2D<Vec4f> m_framebuffer;
	Buffer2D<float> m_depth_buffer;

public:
	RenderPipeline(int width, int height):
		m_width(width + width % 2), m_height(height + height % 2),
		//m_per_frag_mark(m_width, m_height),
		//m_per_frag_queue_buffer(m_width, m_height),
		m_framebuffer(m_width, m_height),
		m_depth_buffer(m_width, m_height)
	{}

	void clear_pipeline(Vec4f color)
	{
		m_post_vs_buffer.clear();
		m_post_clip_buffer.clear();
		m_post_clip_element_buffer.clear();

		//m_per_frag_mark.clear(0);
		//m_per_frag_queue_buffer.clear(FSOut{ 1.0f, color });
		m_framebuffer.clear(Vec4f::Zero());
		m_depth_buffer.clear(1.0f);
	}

	void input_assembly_stage(std::vector<VSIn> const & inputs, std::vector<int> const & elements)
	{
		m_vertex_attri_buffer = inputs;
		m_vertex_element_buffer = elements;
	}

	void vertex_shading_stage(Uniform const & uni) 
	{
		for (auto const & vsin : m_vertex_attri_buffer)
			m_post_vs_buffer.push_back(VertexShader()(vsin, uni));
	}

	void primitive_assembly_stage()
	{
		/* clip */
		for (int i = 0; i < m_vertex_element_buffer.size() / 3; ++i)
			clip_primitive(i);
		/* cull */

	}
	
	void rasterization_stage_and_fragment_shading_stage_post_process_stage(Uniform const & uni, MSAA msaa)
	{
		/* projection divide and viewport transform */
		for (auto & vsout : m_post_clip_buffer)
			projection_divide_and_view_port_transform(vsout);

		/* rasterize primitive */
		for (int i = 0; i < m_post_clip_element_buffer.size() / 3; ++i)
			rasterize_triangle_and_fragment_shading_and_post_process(
				m_post_clip_buffer[m_post_clip_element_buffer[i * 3]],
				m_post_clip_buffer[m_post_clip_element_buffer[i * 3 + 1]],
				m_post_clip_buffer[m_post_clip_element_buffer[i * 3 + 2]],
				uni, msaa);
	}

	Buffer2D<Vec4f> const & render(std::vector<VSIn> const & inputs, std::vector<int> const & elements, 
		Uniform const & uni, MSAA msaa)
	{
		input_assembly_stage(inputs, elements);
		vertex_shading_stage(uni);
		primitive_assembly_stage();
		rasterization_stage_and_fragment_shading_stage_post_process_stage(uni, msaa);

		return m_framebuffer;
	}

private:
	void clip_primitive(size_t prim_id)
	{
		bool need_last = false;
		std::vector<VSOut> post_clip_prim_buffer;
		for (int eid = 0; eid < 3; ++eid) /* for each triangle edge */
		{
			auto const & v0 = m_post_vs_buffer[m_vertex_element_buffer[prim_id * 3 + eid % 3]];
			auto const & v1 = m_post_vs_buffer[m_vertex_element_buffer[prim_id * 3 + (eid + 1) % 3]];

			auto const & vp0 = v0.gl_Position;
			auto const & vp1 = v1.gl_Position;

			/* insert vertex */
			std::vector<float> interp_vertex_t;
			//interp_vertex_t.push_back(0.0f);
			/* w = z */
			float det = (vp1.z() - vp0.z()) - (vp1.w() - vp0.w());
			if (std::abs(det) > eps)
			{
				auto tmp = (vp0.w() - vp0.z()) / det;
				if (0.0f < tmp && tmp < 1.0f) interp_vertex_t.push_back(tmp);
			}
			/* w = -z */
			det = (vp1.z() - vp0.z()) + (vp1.w() - vp0.w());
			if (std::abs(det) > eps)
			{
				auto tmp = (-vp0.w() - vp0.z()) / det;
				if (0.0f < tmp && tmp < 1.0f) interp_vertex_t.push_back(tmp);
			}
			if (interp_vertex_t.size() == 2 && interp_vertex_t[0] > interp_vertex_t[1])
				std::swap(interp_vertex_t[0], interp_vertex_t[1]);
			//interp_vertex_t.push_back(1.0f);

			auto between_near_and_far = [](Vec4f const p) { return std::abs(p.w()) > std::abs(p.z()); };
			if (between_near_and_far(vp0)) 
				post_clip_prim_buffer.push_back(v0);
			for (auto t : interp_vertex_t)
			{
				post_clip_prim_buffer.push_back(lerp(v0, v1, t));
			}
		}

		if (post_clip_prim_buffer.size() < 3) return;

		int start_id = m_post_clip_buffer.size();
		m_post_clip_buffer.insert(m_post_clip_buffer.end(), 
			post_clip_prim_buffer.begin(), post_clip_prim_buffer.end());
		int end_id = m_post_clip_buffer.size() - 1;

		for (int i = start_id + 1; i < end_id; ++i)
		{
			m_post_clip_element_buffer.push_back(start_id);
			m_post_clip_element_buffer.push_back(i);
			m_post_clip_element_buffer.push_back(i + 1);
		}

	}

	void projection_divide_and_view_port_transform(VSOut & vsout)
	{
		auto tmp = 1.0f / vsout.gl_Position.w();
		vsout.gl_Position.x() *= tmp;
		vsout.gl_Position.y() *= tmp;
		vsout.gl_Position.z() *= tmp;
		vsout.gl_Position.x() = vsout.gl_Position.x() * (m_width / 2) + m_width / 2;
		vsout.gl_Position.y() = vsout.gl_Position.y() * (m_height / 2) + m_height / 2;
	}

	void rasterize_triangle_and_fragment_shading_and_post_process(
		VSOut const & vsout0, VSOut const & vsout1, VSOut const & vsout2, Uniform const & uni, MSAA aa_mode)
	{
		auto v0 = vsout0.gl_Position, v1 = vsout1.gl_Position, v2 = vsout2.gl_Position;
		
		Vec2f sv0 = { v0.x(), v0.y() }, sv1 = { v1.x(), v1.y() }, sv2 = { v2.x(), v2.y() };
		Vec3f inv_vertex_w = { 1.0f / v0.w(), 1.0f / v1.w(), 1.0f / v2.w() };

		float x1 = sv0.x(), x2 = sv1.x(), x3 = sv2.x();
		float y1 = sv0.y(), y2 = sv1.y(), y3 = sv2.y();

		int minx = (std::max)(0, int(std::floor((std::min)(x1, (std::min)(x2, x3)))));
		int miny = (std::max)(0, int(std::floor((std::min)(y1, (std::min)(y2, y3)))));
		int maxx = (std::min)(m_width - 1, int(std::ceil((std::max)(x1, (std::max)(x2, x3)))));
		int maxy = (std::min)(m_height - 1, int(std::ceil((std::max)(y1, (std::max)(y2, y3)))));

		if (minx >= maxx || miny >= maxy) return;

		if ((maxx - minx + 1) % 2 == 1)
		{
			if (maxx < m_width - 1) maxx += 1;
			else minx -= 1;
		}
		if ((maxy - miny + 1) % 2 == 1)
		{
			if (maxy < m_height - 1) maxy += 1;
			else miny -= 1;
		}

		Vec2f line_vec[3];
		line_vec[0] = sv2 - sv1;
		line_vec[1] = sv0 - sv2;
		line_vec[2] = sv1 - sv0;

		bool is_top_left[3];
		is_top_left[0] = top_left(sv1, sv2);
		is_top_left[1] = top_left(sv2, sv0);
		is_top_left[2] = top_left(sv0, sv1);

		Vec2f left_up_corner = { minx + 0.5f, miny + 0.5f };
		Vec3f left_up_corner_bary = {
				edge_equation(left_up_corner, sv1, sv2),
				edge_equation(left_up_corner, sv2, sv0),
				edge_equation(left_up_corner, sv0, sv1) };

		for (int x = minx; x <= maxx - 1; x += 2) for (int y = miny; y <= maxy - 1; y += 2)
		{
			QuadOf<Vec3f> quad_bary;
			QuadOf<float> quad_ratio;
			QuadOf<bool> quad_need_rast;

			/* compute barycentric coordinates */
			for (int i = 0; i < 2; ++i) for (int j = 0; j < 2; ++j)
			{
				auto & bary = quad_bary[i][j];
				auto & aa_ratio = quad_ratio[i][j]; 
				bool & need_rast = quad_need_rast[i][j];
			
				bary = left_up_corner_bary;
				aa_ratio = 1.0f;
				need_rast = true;

				int const posx = x + i - minx;
				int const posy = y + j - miny;

				switch (aa_mode) 
				{
				case MSAA::MSAAx4:
				{
					static std::array<Vec2f, 4> frag_sample_offset =
					{ Vec2f{ 0.25f, 0.25f },Vec2f{ 0.25f, -0.25f },Vec2f{ -0.25f, 0.25f },Vec2f{ -0.25f, -0.25f } };

					int sample_inside_cnt = 0;
					//Vec3f accum_bary = Vec3f::Zero();
					for (auto const & off : frag_sample_offset)
					{
						Vec2f sample_pos = { posx + off.x(), posy + off.y() };
						Vec3f sample_bary = bary + Vec3f{
							sample_pos.x() * line_vec[0].y() - sample_pos.y() * line_vec[0].x(),
							sample_pos.x() * line_vec[1].y() - sample_pos.y() * line_vec[1].x(),
							sample_pos.x() * line_vec[2].y() - sample_pos.y() * line_vec[2].x()
						};
						if ((sample_bary.x() > eps || (std::abs(sample_bary.x()) < eps && is_top_left[0]))
							&& (sample_bary.y() > eps || (std::abs(sample_bary.y()) < eps && is_top_left[1]))
							&& (sample_bary.z() > eps || (std::abs(sample_bary.z()) < eps && is_top_left[2])))
						{
							sample_inside_cnt += 1;
						}
					}
					aa_ratio = float(sample_inside_cnt) * 0.25f;

					if (sample_inside_cnt > 0)
						need_rast = true;
					else
						need_rast = false;

					//accum_bary /= sample_inside_cnt;

					bary = bary + Vec3f{
						posx * line_vec[0].y() - posy * line_vec[0].x(),
						posx * line_vec[1].y() - posy * line_vec[1].x(),
						posx * line_vec[2].y() - posy * line_vec[2].x()
					};
					bary /= bary.x() + bary.y() + bary.z();
				
				}
				break;
				case MSAA::Standard:
				{
					bary = bary + Vec3f{
						posx * line_vec[0].y() - posy * line_vec[0].x(),
						posx * line_vec[1].y() - posy * line_vec[1].x(),
						posx * line_vec[2].y() - posy * line_vec[2].x()
					};

					if ((bary.x() > eps || (std::abs(bary.x()) < eps && is_top_left[0]))
						&& (bary.y() > eps || (std::abs(bary.y()) < eps && is_top_left[1]))
						&& (bary.z() > eps || (std::abs(bary.z()) < eps && is_top_left[2])))
						need_rast = true;
					else
						need_rast = false;

					bary /= bary.x() + bary.y() + bary.z();
				}
				break;
				default:
					return;
				}
			
			}
			if (!(quad_need_rast[0][0] || quad_need_rast[1][0] 
				|| quad_need_rast[0][1] || quad_need_rast[1][1])) continue;

			/* rasterize quad and post process */
			auto quad_fsout = quad_shading(vsout0, vsout1, vsout2, inv_vertex_w, uni, 
				quad_need_rast, quad_bary, quad_ratio);
			quad_post_process(quad_fsout, quad_need_rast, { x, y });
		}
	}

	QuadOf<FSOut> quad_shading(VSOut const & vsout0, VSOut const & vsout1, VSOut const & vsout2,
		Vec3f const & inv_vertex_w, Uniform const & uni, 
		QuadOf<bool> const & quad_need_rast, QuadOf<Vec3f> const & bary, QuadOf<float> const & aa_ratio) {
		
		QuadOf<Vec3f> quad_bary_correct;
		for (int i = 0; i < 2; ++i) for (int j = 0; j < 2; ++j)
		{
			float inv_frag_w = inv_vertex_w.transpose() * bary[i][j];
			quad_bary_correct[i][j] = proj_correct(bary[i][j], inv_vertex_w, inv_frag_w);
		}
		auto quad_fsin = quad_interp(quad_bary_correct, vsout0, vsout1, vsout2);
		
		QuadOf<FSOut> res;
		for (int i = 0; i < 2; ++i) for (int j = 0; j < 2; ++j)
		{
			if (quad_need_rast[i][j] == false) continue;
			res[i][j] = FragmentShader()(quad_fsin[i][j], uni);
			res[i][j].out_color.w() *= aa_ratio[i][j];
		}
		return res;
	}

	void quad_post_process(QuadOf<FSOut> const & quad_fsout, QuadOf<bool> const & quad_need_rast,
		Vec2i const & screen_coord)
	{
		for (int i = 0; i < 2; ++i) for (int j = 0; j < 2; ++j)
		{
			if (quad_need_rast[i][j] == false) continue;
			auto const & fsout = quad_fsout[i][j];
			int x = screen_coord.x() + i, y = screen_coord.y() + j;
			
			//m_framebuffer.coeff(x, y) = fsout.out_color;

			/* late z test */
			if (m_depth_buffer.coeff(x, y) <= fsout.gl_FragDepth) continue;
			
			m_depth_buffer.coeff(x, y) = fsout.gl_FragDepth;

			/* alpha blend */
			auto const & src = m_framebuffer.coeff(x, y);
			m_framebuffer.coeff(x, y) = lerp(src, fsout.out_color, fsout.out_color.w());
			m_framebuffer.coeff(x, y).w() = 1.0f;
		}
	}

};

#endif