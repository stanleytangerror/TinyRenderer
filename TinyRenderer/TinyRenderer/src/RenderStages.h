#ifndef RENDER_STAGES_H
#define RENDER_STAGES_H

#include "Types.h"
#include <set>

/////////////////////////////////
// pipeline functions
/////////////////////////////////
PositionTriangleTestResult position_triangle_test(float x, float y,
	float ax, float ay, float bx, float by, float cx, float cy,
	Vec3f & coord)
{
	using R = PositionTriangleTestResult;
	R result = R::INSIDE;
	coord[0] = edge_function(x, y, bx, by, cx, cy);
	coord[1] = edge_function(x, y, cx, cy, ax, ay);
	coord[2] = edge_function(x, y, ax, ay, bx, by);

	for (int _i = 0; _i < 3; ++_i)
	{
		if (coord[_i] > 0.0f)
		{
			switch (result)
			{
			case R::INSIDE: result = R::FRONT; break;
			case R::BACK: result = R::OUTSIDE; break;
			default: break;
			}
		}
		else if (coord[_i] < 0.0f)
		{
			switch (result)
			{
			case R::INSIDE: result = R::BACK; break;
			case R::FRONT: result = R::OUTSIDE; break;
			default: break;
			}
		}
		if (result == R::OUTSIDE)
			break;
	}
	return result;
}

template <typename Shader, typename VSIn, typename VSOut>
vector_with_eigen<Wrapper<VSOutHeader, VSOut>> vertex_shader_stage(vector_with_eigen<Wrapper<VSInHeader, VSIn>> & vertices, Shader & shader)
{
	vector_with_eigen<Wrapper<VSOutHeader, VSOut>> vsout;
	for (auto & v : vertices)
	{
		auto vso = shader.vertex_shader(v);
		vsout.push_back(vso);
	}
	return std::move(vsout);
}

template <typename VSOut>
QuadFragType fragment_triangle_test(
	FaceCulling culling, 
	float x, float y,
	Wrapper<VSOutHeader, VSOut> const & p0, 
	Wrapper<VSOutHeader, VSOut> const & p1, 
	Wrapper<VSOutHeader, VSOut> const & p2,
	PositionTriangleTestResult & sided, 
	Vec3f & barycentric_coordinate)
{

	sided = position_triangle_test(x, y,
		p0.header.position.x(), p0.header.position.y(),
		p1.header.position.x(), p1.header.position.y(),
		p2.header.position.x(), p2.header.position.y(),
		barycentric_coordinate);

	if (sided == PositionTriangleTestResult::OUTSIDE ||
		sided == PositionTriangleTestResult::INSIDE)
		return QuadFragType::HELPER_OUT_OF_RANGE;

	if (culling == FaceCulling::FRONT && sided == PositionTriangleTestResult::BACK)
		return QuadFragType::HELPER_OUT_OF_RANGE;

	return QuadFragType::RENDER;
	
}

template <typename VSOut>
QuadFragType fragment_barycoord_correction(
	Wrapper<VSOutHeader, VSOut> const & p0, Wrapper<VSOutHeader, VSOut> const & p1, Wrapper<VSOutHeader, VSOut> const & p2,
	PositionTriangleTestResult const & res, Vec3f & barycentric_coordinate, float & depth)
{
	float mark = (res == PositionTriangleTestResult::FRONT) ? 1.0f : -1.0f;
	barycentric_coordinate = mark * barycentric_coordinate;
	barycentric_coordinate = barycentric_coordinate /
		(barycentric_coordinate.x() + barycentric_coordinate.y() + barycentric_coordinate.z());

	float inv_depth0 = 1.0f / p0.header.position.w();
	float inv_depth1 = 1.0f / p1.header.position.w();
	float inv_depth2 = 1.0f / p2.header.position.w();

	barycentric_coordinate.x() = barycentric_coordinate.x() * inv_depth0;
	barycentric_coordinate.y() = barycentric_coordinate.y() * inv_depth1;
	barycentric_coordinate.z() = barycentric_coordinate.z() * inv_depth2;

	float inv_depth = barycentric_coordinate[0] +
		barycentric_coordinate[1] + barycentric_coordinate[2];

	depth = 1.0f / inv_depth;

	///* depth test */
	//if (fs_ins.coeff_ref(x, y).header.prim_id >= 0 && depth >= fs_ins.coeff_ref(x, y).header.depth)
	//	return QuadFragType::HELPER_DEPTH_TEST_FAILED;

	return QuadFragType::RENDER;
}

template <typename Shader, typename VSOut, typename FSIn>
void quad_rasterize(Shader & shader, Storage2D<Wrapper<FSInHeader, FSIn>> & fs_ins, Buffer2D<MSAA_4> & msaa,
	vector_with_eigen<Wrapper<VSOutHeader, VSOut>> & vsout,
	std::vector<Primitive<int> > & primitives,
	FaceCulling culling, int const qx, int const qy, int prim_id, int vid0, int vid1, int vid2)
{
	static std::pair<int, int> const quad_offsets[4] = {
		{ 0, 0 },{ 1, 0 },{ 0, 1 },{ 1, 1 } };

	auto & p0 = vsout[vid0];
	auto & p1 = vsout[vid1];
	auto & p2 = vsout[vid2];

	/* ---------- generate msaa barycentric coordinate and depth ---------- */

	/* used for msaa */
	static std::pair<float, float> const frag_msaa_offset[4] = {
		{ 0.25f, 0.25f }, { 0.75f, 0.25f }, { 0.25f, 0.75f }, { 0.75f, 0.75f } };
	static QuadFragType quad_masks[4];
	struct MSAA_DATA
	{
		Vec3f msaa_barycentric_coordinates[4];
		float msaa_depthes[4];
		QuadFragType msaa_masks[4];
		PositionTriangleTestResult msaa_test_results[4];
	};
	static MSAA_DATA frag_msaa_data[4];

	/* first phase
	* skip invalid quad where no fragment lying in the triangle
	*/
	bool valid = false;
	for (int _i = 0; _i < 4; ++_i)
	{
		int x = qx + quad_offsets[_i].first;
		int y = qy + quad_offsets[_i].second;
		auto & frag_msaa_datum = frag_msaa_data[_i];
		for (int frag_msaa_no = 0; frag_msaa_no < 4; ++frag_msaa_no)
		{
			/* res = {RENDER, HELPER_OUT_OF_RANGE} */
			frag_msaa_datum.msaa_masks[frag_msaa_no] = fragment_triangle_test(
				culling,
				float(x) + frag_msaa_offset[frag_msaa_no].first, float(y) + frag_msaa_offset[frag_msaa_no].second,
				p0, p1, p2, 
				frag_msaa_datum.msaa_test_results[frag_msaa_no], frag_msaa_datum.msaa_barycentric_coordinates[frag_msaa_no]);
			if (frag_msaa_datum.msaa_masks[frag_msaa_no] == QuadFragType::RENDER)
			{
				quad_masks[_i] = QuadFragType::RENDER;
				valid = true;
			}
		}
	}
	if (!valid) return;

	/* second phase
	* correct the msaa barycentric coordinates
	*/
	for (int _i = 0; _i < 4; ++_i)
	{
		int x = qx + quad_offsets[_i].first;
		int y = qy + quad_offsets[_i].second;
		auto & frag_msaa_datum = frag_msaa_data[_i];
		for (int frag_msaa_no = 0; frag_msaa_no < 4; ++frag_msaa_no)
		{
			if (frag_msaa_datum.msaa_masks[frag_msaa_no] != QuadFragType::RENDER) continue;

			fragment_barycoord_correction(
				p0, p1, p2,
				frag_msaa_datum.msaa_test_results[frag_msaa_no],
				frag_msaa_datum.msaa_barycentric_coordinates[frag_msaa_no],
				frag_msaa_datum.msaa_depthes[frag_msaa_no]);
			
			MSAASample sample;
			sample.percent = 0.25f;
			sample.depth = frag_msaa_datum.msaa_depthes[frag_msaa_no];
			sample.prim_id = prim_id;
		
			msaa.coeff_ref(x, y).samples[frag_msaa_no].push(std::move(sample));
		}
	}

	/* ---------- generate centric sample coordinate and depth ---------- */

	/* each quad has following properties */
	static Vec3f barycentric_coordinates[4];
	static PositionTriangleTestResult quad_test_results[4];
	static float depthes[4];

	Wrapper<FSInHeader, FSIn> quad_fsin[4];
	for (int _i = 0; _i < 4; ++_i)
	{
		int x = qx + quad_offsets[_i].first;
		int y = qy + quad_offsets[_i].second;
		/* get barycentric coordinates (with helper fargment) */
		fragment_triangle_test(
			culling,
			float(x) + 0.5f, float(y) + 0.5f,
			p0, p1, p2,
			quad_test_results[_i], barycentric_coordinates[_i]);
		fragment_barycoord_correction(
			p0, p1, p2,
			quad_test_results[_i],
			barycentric_coordinates[_i],
			depthes[_i]);
		/* get texture coordinates and their derivatives (with helper fargment) */
		shader.interpolate(quad_fsin[_i].content, depthes[_i],
			p0.content, p1.content, p2.content, barycentric_coordinates[_i]);
	}

	shader.quad_derivative(quad_fsin[0].content, quad_fsin[1].content, 
		quad_fsin[2].content, quad_fsin[3].content);

	/* construct header of fragment shaders */
	for (int _i = 0; _i < 4; ++_i)
	{
		int x = qx + quad_offsets[_i].first;
		int y = qy + quad_offsets[_i].second;

		auto & fsin = quad_fsin[_i];

		if (quad_masks[_i] != QuadFragType::RENDER)
		{
			continue;
		}

		fsin.header.prim_id = prim_id;
		fsin.header.point_coord << x, y;
		fsin.header.depth = depthes[_i];
		fsin.header.interp_coord = barycentric_coordinates[_i];

		//std::cout << "insert (" << x << ", " << y << ") prim_id " << prim_id << std::endl;
		fs_ins.insert(x, y, std::move(fsin), [](auto & list, auto && fsin) { list.push_back(fsin); });
	}

}

template <typename Shader, typename VSOut, typename FSIn>
void rasterize_stage(Shader & shader, Buffer2D<IUINT32> & buffer, Buffer2D<IUINT32> & fsbuffer, Buffer2D<MSAA_4> & msaa,
	Storage2D<Wrapper<FSInHeader, FSIn>> & fs_ins, vector_with_eigen<Wrapper<VSOutHeader, VSOut> > & vsout, 
	std::vector<Primitive<int> > & primitives,
	FaceCulling culling = FaceCulling::FRONT_AND_BACK)
{
	enum class ClipState { INSIDE, TOO_CLOSE, TOO_FAR };

	static int w = buffer.m_width;
	static int h = buffer.m_height;
	static std::vector<int> primitive_mask;
	//static Buffer2D<IUINT32> fsbuffer(w, h);
	//static Storage2D<Wrapper<FSInHeader, FSIn>> fs_ins(w, h);

	fsbuffer.clear(0);

	Mat4f to_screen;
	to_screen <<
		w / 2, 0.0f, 0.0f, 0.0f,
		0.0f, h / 2, 0.0f, 0.0f,
		0.0f, 0.0f, 0.5f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f;

	std::vector<ClipState> clipped;
	clipped.reserve(vsout.size());

	/* ---------- frustum clipping ---------- */
	std::cout << "frustum clipping: near and far plane" << std::endl;
	/* select clipping vertices 
	 * clip near and far clipping plane 
	 */
	for (auto & v : vsout)
	{
		float cliplen = (std::abs)(v.header.position.w());
		/* to homogeneous clip space */
		if (v.header.position.z() < -cliplen)
			clipped.push_back(ClipState::TOO_CLOSE);
		else if (v.header.position.z() > cliplen)
			clipped.push_back(ClipState::TOO_FAR);
		else
			clipped.push_back(ClipState::INSIDE);
	}

	/* do primitive clipping */
	for (auto & prim : primitives)
	{
		std::vector<int> vertex_indices;
		Vec4f near_plane_coeff(0.0f, 0.0f, 1.0f, 1.0f);
		Vec4f far_plane_coeff(0.0f, 0.0f, -1.0f, 1.0f);
		/* for each edge of a primitive */
		for (int _i = 0; _i < prim.m_vertices.size(); ++_i)
		{
			int vid0 = prim.m_vertices[_i];
			int vid1 = prim.m_vertices[((_i == prim.m_vertices.size() - 1) ? 0 : _i + 1)];
			/* both inside */
			if (clipped[vid0] == ClipState::INSIDE && clipped[vid1] == ClipState::INSIDE)
			{
				vertex_indices.push_back(vid0);
				vertex_indices.push_back(vid1);
				continue;
			}
			/* both too close or too far */
			if (clipped[vid0] != ClipState::INSIDE && clipped[vid1] != ClipState::INSIDE && clipped[vid0] == clipped[vid1])
			{
				continue;
			}

			auto & p0 = vsout[vid0];
			auto & p1 = vsout[vid1];
			float t_4d = 0.0f;

			/* check vid0 side */
			if (clipped[vid0] == ClipState::INSIDE)
			{
				vertex_indices.push_back(vid0);
			}
			else
			{
				if (clipped[vid0] == ClipState::TOO_CLOSE)
					t_4d = -float(near_plane_coeff.transpose() * p1.header.position) /
						float(near_plane_coeff.transpose() * (p0.header.position - p1.header.position));
				else if (clipped[vid0] == ClipState::TOO_FAR)
					t_4d = -float(far_plane_coeff.transpose() * p1.header.position) /
						float(far_plane_coeff.transpose() * (p0.header.position - p1.header.position));
				/* new vsout in vertex shader's output stream */
				vsout.push_back(Wrapper<VSOutHeader, VSOut>(
					interpolate(t_4d, p0.header, p1.header), interpolate(t_4d, p0.content, p1.content)));
				vertex_indices.push_back(vsout.size() - 1);
			}

			/* check vid1 side */
			if (clipped[vid1] == ClipState::INSIDE)
			{
				vertex_indices.push_back(vid1);
			}
			else
			{
				if (clipped[vid1] == ClipState::TOO_CLOSE)
					t_4d = -float(near_plane_coeff.transpose() * p1.header.position) /
						float(near_plane_coeff.transpose() * (p0.header.position - p1.header.position));
				else if (clipped[vid0] == ClipState::TOO_FAR)
					t_4d = -float(far_plane_coeff.transpose() * p1.header.position) /
						float(far_plane_coeff.transpose() * (p0.header.position - p1.header.position));
				/* new vsout in vertex shader's output stream */
				vsout.push_back(Wrapper<VSOutHeader, VSOut>(
					interpolate(t_4d, p0.header, p1.header), interpolate(t_4d, p0.content, p1.content)));
				vertex_indices.push_back(vsout.size() - 1);
			}
			

			if (clipped[vid1] == ClipState::INSIDE)
				vertex_indices.push_back(vid1);
		}

		prim.m_vertices.clear();
		for (int _i = 0; _i < vertex_indices.size(); ++_i)
		{
			/* remove repeated points */
			if (prim.m_vertices.empty() || vertex_indices[_i] != prim.m_vertices.back())
				prim.m_vertices.push_back(vertex_indices[_i]);
		}
		/* remove repeated start and end point */
		if (!prim.m_vertices.empty() && prim.m_vertices.front() == prim.m_vertices.back())
			prim.m_vertices.pop_back();
		prim.m_type = PrimitiveType(bound<int>(prim.m_vertices.size(), 1, 4));
	}

	/* ---------- triangulate ---------- */
	primitive_mask.clear();
	int const origin_prim_num = primitives.size();
	for (int _i = 0; _i < origin_prim_num; ++_i)
	{
		/* WARNING: could not have loop invariance
		 *due to the modification and reallocation during looping 
		 */
		switch (primitives[_i].m_type)
		{
		case PrimitiveType::TRIANGLE_TYPE:
		{
			primitive_mask.push_back(_i);
		}
		break;
		case PrimitiveType::POLYGON_TYPE:
		{
			int vid0 = primitives[_i].m_vertices[0];
			for (int _j = 1; _j < primitives[_i].m_vertices.size() - 1; ++_j)
			{
				int vid1 = primitives[_i].m_vertices[_j];
				int vid2 = primitives[_i].m_vertices[_j + 1];
				Primitive<int> new_prim;
				new_prim.m_type = PrimitiveType::TRIANGLE_TYPE;
				new_prim.m_vertices.push_back(vid0);
				new_prim.m_vertices.push_back(vid1);
				new_prim.m_vertices.push_back(vid2);
				primitives.push_back(new_prim);
				primitive_mask.push_back(primitives.size() - 1);
			}
		}
		break;
		default:
			break;
		}
				
	}

	/* ---------- homogeneous divide ---------- */
	std::cout << "homogeneous divide" << std::endl;
	for (auto & v : vsout)
	{
		/* projection division, to NDC coordinate */
		//v.position = v.position / v.position.w();
		v.header.position.x() = v.header.position.x() / v.header.position.w();
		v.header.position.y() = v.header.position.y() / v.header.position.w();
		v.header.position.z() = v.header.position.z() / v.header.position.w();
	}
	
	/* ---------- viewport mapping ---------- */
	std::cout << "viewport mapping" << std::endl;
	for (auto & v : vsout)
	{
		v.header.position = to_screen * (v.header.position + Vec4f(1.0f, 1.0f, 1.0f, 0.0f));
	}

	// rasterization
	std::cout << "rasterization " << std::endl;

	/* for each primitive, generate fragment shader input */
	for (int prim_id : primitive_mask)
	{
		//std::cout << "rasterize " << prim_id << std::endl;
		auto & prim = primitives[prim_id];

		int vid0 = prim.m_vertices[0];
		int vid1 = prim.m_vertices[1];
		int vid2 = prim.m_vertices[2];

		auto & p0 = vsout[vid0];
		auto & p1 = vsout[vid1];
		auto & p2 = vsout[vid2];

		int minx = (std::min)({
			(std::floor)(p0.header.position.x()),
			(std::floor)(p1.header.position.x()),
			(std::floor)(p2.header.position.x()) });
		int miny = (std::min)({
			(std::floor)(p0.header.position.y()),
			(std::floor)(p1.header.position.y()),
			(std::floor)(p2.header.position.y()) });
		int maxx = (std::max)({
			(std::ceil)(p0.header.position.x()),
			(std::ceil)(p1.header.position.x()),
			(std::ceil)(p2.header.position.x()) });
		int maxy = (std::max)({
			(std::ceil)(p0.header.position.y()),
			(std::ceil)(p1.header.position.y()),
			(std::ceil)(p2.header.position.y()) });

		minx = (std::max)(0, (minx % 2 == 0) ? minx : (minx - 1));
		miny = (std::max)(0, (miny % 2 == 0) ? miny : (miny - 1));
		maxx = (std::min)(w - 2, (maxx % 2 == 0) ? maxx : (maxx + 1));
		maxy = (std::min)(h - 2, (maxy % 2 == 0) ? maxy : (maxy + 1));

		/* dealing with each quad */
		for (int qx = minx; qx <= maxx; qx += 2) for (int qy = miny; qy <= maxy; qy += 2)
		{
			quad_rasterize(shader, fs_ins, msaa, vsout, primitives, culling,
				qx, qy, prim_id, vid0, vid1, vid2);
		}
		
	}
}

template <typename Shader, typename FSIn, typename FSOut>
void fragment_shader_stage(Buffer2D<IUINT32> & buffer, Buffer2D<IUINT32> & fsbuffer, Storage2D<Wrapper<FSInHeader, FSIn>> & fs_ins, 
	Buffer2D<MSAA_4> & msaa, Shader & shader)
{
	static std::map<int, float> prim_ids;
	// fragment shader
	std::cout << "fragment shader" << std::endl;
	for (int y = 0; y < fsbuffer.m_height; ++y) for (int x = 0; x < fsbuffer.m_width; ++x)
	{
		bool draw_frag = false;
		prim_ids.clear();

		auto & fsin_list = fs_ins.coeff_ref(x, y);
		auto & msaa_samples = msaa.coeff_ref(x, y);
		
		for (int frag_msaa_no = 0; frag_msaa_no < msaa_samples.sample_num; ++frag_msaa_no)
		{
			if (msaa_samples.samples[frag_msaa_no].empty()) continue;

			auto & sample = msaa_samples.samples[frag_msaa_no].top();
			if (prim_ids.find(sample.prim_id) == prim_ids.end())
				prim_ids[sample.prim_id] = 0.0f;
			prim_ids.at(sample.prim_id) += sample.percent;
			draw_frag = true;
		}

		if (!draw_frag) continue;

		Vec4f color = Vec4f::Zero();
		for (auto id_percent : prim_ids)
		{
			for (auto & fsin : fsin_list)
			{
				if (fsin.header.prim_id != id_percent.first) continue;

				Wrapper<FSOutHeader, FSOut> const fsout = shader.fragment_shader(fsin);
				color = color + fsout.header.color * id_percent.second;
				break;
			}
		}
		
		unsigned int r = unsigned int((std::max)(0.0f, color.x() * 256.f - 1.f)) & 0xff;
		unsigned int g = unsigned int((std::max)(0.0f, color.y() * 256.f - 1.f)) & 0xff;
		unsigned int b = unsigned int((std::max)(0.0f, color.z() * 256.f - 1.f)) & 0xff;

		buffer.coeff_ref(x, y) = IUINT32((r << 16) | (g << 8) | b);
	}
}

template <typename FSIn>
void clear(Buffer2D<IUINT32> & buffer, Buffer2D<IUINT32> & fsbuffer, Storage2D<Wrapper<FSInHeader, FSIn> > & fs_ins, 
	Buffer2D<MSAA_4> & msaa)
{
	buffer.clear([](int x, int y, IUINT32 & val)
	{
		y = y / 4;
		val = IUINT32((y << 16) | (y << 8) | y);
	});
	
	fsbuffer.clear(0);
	
	msaa.clear([](int x, int y, MSAA_4 & msaa_4)
	{
		for (int _i = 0; _i < msaa_4.sample_num; ++_i)
			msaa_4.samples[_i] = MSAA_4::queue();
	});
	
	fs_ins.clear([](int x, int y, vector_with_eigen<Wrapper<FSInHeader, FSIn> > & container)
	{
		container.clear();
	});
}

template <typename Shader, typename VSIn, typename VSOut, typename FSIn, typename FSOut>
void pipeline(Buffer2D<IUINT32> & buffer)
{
	static float w = float(buffer.m_width);
	static float h = float(buffer.m_height);
	static Buffer2D<IUINT32> fsbuffer(w, h);

	static Storage2D<Wrapper<FSInHeader, FSIn>> fs_ins(w, h);
	static Buffer2D<MSAA_4> msaa(w, h);

	clear(buffer, fsbuffer, fs_ins, msaa);
	
	auto & vsdata = (input_assembly_stage<Shader, VSIn>())();
	Shader & shader = std::get<0>(vsdata);
	vector_with_eigen<Wrapper<VSInHeader, VSIn> > & vs_ins = std::get<1>(vsdata);
	std::vector<Primitive<int>  > & primitives = std::get<2>(vsdata);

	auto & vs_outs = vertex_shader_stage<Shader, VSIn, VSOut>(vs_ins, shader);

	//auto & primitives = primitive_assembly_stage(ebo);

	rasterize_stage<Shader, VSOut, FSIn>(shader, buffer, fsbuffer, msaa, fs_ins, vs_outs, primitives, FaceCulling::FRONT_AND_BACK);
	fragment_shader_stage<Shader, FSIn, FSOut>(buffer, fsbuffer, fs_ins, msaa, shader);

}

#endif