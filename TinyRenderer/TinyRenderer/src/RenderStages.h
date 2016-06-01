#ifndef RENDER_STAGES_H
#define RENDER_STAGES_H

#include "Types.h"

/////////////////////////////////
// pipeline functions
/////////////////////////////////
FragmentTriangleTestResult fragment_triangle_test(int x, int y,
	int ax, int ay, int bx, int by, int cx, int cy,
	Vec3f & coord)
{
	using R = FragmentTriangleTestResult;
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
	vector_with_eigen<Wrapper<VSOutHeader, VSOut>> & vsout, 
	std::vector<Primitive<int>  > & primitives, int prim_id, 
	FaceCulling culling, int x, int y, 
	Wrapper<VSOutHeader, VSOut> const & p0, Wrapper<VSOutHeader, VSOut> const & p1, Wrapper<VSOutHeader, VSOut> const & p2,
	FragmentTriangleTestResult & res, Vec3f & barycentric_coordinate)
{
	//auto & prim = primitives[prim_id];
	//auto & p0 = vsout[prim.p0];
	//auto & p1 = vsout[prim.p1];
	//auto & p2 = vsout[prim.p2];

	res = fragment_triangle_test(x, y,
		p0.header.position.x(), p0.header.position.y(),
		p1.header.position.x(), p1.header.position.y(),
		p2.header.position.x(), p2.header.position.y(),
		barycentric_coordinate);

	if (res == FragmentTriangleTestResult::OUTSIDE ||
		res == FragmentTriangleTestResult::INSIDE)
		return QuadFragType::HELPER_OUT_OF_RANGE;

	if (culling == FaceCulling::FRONT && res == FragmentTriangleTestResult::BACK)
		return QuadFragType::HELPER_OUT_OF_RANGE;

	return QuadFragType::RENDER;
	
}

template <typename VSOut, typename FSIn>
QuadFragType fragment_barycoord_correction_and_depth_test(
	Buffer2D<Wrapper<FSInHeader, FSIn>> & fs_ins, vector_with_eigen<Wrapper<VSOutHeader, VSOut>> & vsout, std::vector<Primitive<int>  > & primitives,
	int prim_id, int x, int y,
	Wrapper<VSOutHeader, VSOut> const & p0, Wrapper<VSOutHeader, VSOut> const & p1, Wrapper<VSOutHeader, VSOut> const & p2,
	FragmentTriangleTestResult const & res, Vec3f & barycentric_coordinate, float & depth)
{
	float mark = (res == FragmentTriangleTestResult::FRONT) ? 1.0f : -1.0f;
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

	/* depth test */
	if (fs_ins.coeff_ref(x, y).header.prim_id >= 0 && depth >= fs_ins.coeff_ref(x, y).header.depth)
		return QuadFragType::HELPER_DEPTH_TEST_FAILED;

	return QuadFragType::RENDER;
}

template <typename Shader, typename VSOut, typename FSIn>
void quad_rasterize(Shader & shader, Buffer2D<Wrapper<FSInHeader, FSIn>> & fs_ins, 
	vector_with_eigen<Wrapper<VSOutHeader, VSOut>> & vsout,
	std::vector<Primitive<int>  > & primitives,
	FaceCulling culling, int const qx, int const qy, int prim_id, int vid0, int vid1, int vid2)
{
	/* each quad has following properties */
	static Vec3f barycentric_coordinates[4];
	static FragmentTriangleTestResult quad_test_results[4];
	static float depthes[4];
	static QuadFragType quad_masks[4];
	static std::pair<int, int> const quad_offsets[4] = {
		{0, 0}, {1, 0}, {0, 1}, {1, 1} };

	auto & p0 = vsout[vid0];
	auto & p1 = vsout[vid1];
	auto & p2 = vsout[vid2];

	/* first phase
	* skip invalid quad where no fragment lying in the triangle
	*/
	bool valid = false;
	for (int _i = 0; _i < 4; ++_i)
	{
		int x = qx + quad_offsets[_i].first;
		int y = qy + quad_offsets[_i].second;
		quad_masks[_i] = fragment_triangle_test<VSOut>(vsout, primitives, prim_id, culling,
			x, y, p0, p1, p2, quad_test_results[_i], barycentric_coordinates[_i]);
		/* quad_masks[_i] = {RENDER, HELPER_OUT_OF_RANGE} */
		if (quad_masks[_i] == QuadFragType::RENDER) valid = true;
	}
	if (!valid) return;

	/* second phase
	* correct the barycentric coordinates and run depth test
	*/
	valid = false;
	for (int _i = 0; _i < 4; ++_i)
	{
		int x = qx + quad_offsets[_i].first;
		int y = qy + quad_offsets[_i].second;
		QuadFragType mask = fragment_barycoord_correction_and_depth_test(fs_ins, vsout, primitives, prim_id,
			x, y, p0, p1, p2, quad_test_results[_i], barycentric_coordinates[_i], depthes[_i]);
		/* mask = {RENDER, HELPER_DEPTH_TEST_FAILED}
		* quad_masks[_i] = {RENDER, HELPER_OUT_OF_RANGE}
		*/
		quad_masks[_i] = (mask == QuadFragType::RENDER) ? quad_masks[_i] : mask;
		if (quad_masks[_i] == QuadFragType::RENDER) valid = true;
	}
	if (!valid) return;

	/* third phase
	* get texture coordinates and their derivatives (with helper fargment)
	*/
	for (int _i = 0; _i < 4; ++_i)
	{
		int x = qx + quad_offsets[_i].first;
		int y = qy + quad_offsets[_i].second;
		shader.interpolate(fs_ins.coeff_ref(x, y).content, depthes[_i],
			p0.content, p1.content, p2.content, barycentric_coordinates[_i]);
	}

	shader.quad_derivative(fs_ins.coeff_ref(qx, qy).content, fs_ins.coeff_ref(qx + 1, qy).content,
		fs_ins.coeff_ref(qx, qy + 1).content, fs_ins.coeff_ref(qx + 1, qy + 1).content);

	/* fourth phase
	* construct header of fragment shaders
	*/
	for (int _i = 0; _i < 4; ++_i)
	{
		int x = qx + quad_offsets[_i].first;
		int y = qy + quad_offsets[_i].second;

		auto & fsin = fs_ins.coeff_ref(x, y);

		if (quad_masks[_i] != QuadFragType::RENDER)
		{
			//fsin.header.prim_id = -1;
			continue;
		}

		fsin.header.prim_id = prim_id;
		fsin.header.point_coord << x, y;
		fsin.header.depth = depthes[_i];
		fsin.header.interp_coord = barycentric_coordinates[_i];

	}
}

template <typename Shader, typename VSOut, typename FSIn>
void rasterize_stage(Shader & shader, Buffer2D<IUINT32> & buffer, Buffer2D<IUINT32> & fsbuffer,
	Buffer2D<Wrapper<FSInHeader, FSIn>> & fs_ins, vector_with_eigen<Wrapper<VSOutHeader, VSOut> > & vsout, 
	std::vector<Primitive<int> > & primitives,
	FaceCulling culling = FaceCulling::FRONT_AND_BACK)
{
	enum class ClipState { INSIDE, TOO_CLOSE, TOO_FAR };

	static int w = buffer.m_width;
	static int h = buffer.m_height;
	//static Buffer2D<IUINT32> fsbuffer(w, h);
	//static Buffer2D<Wrapper<FSInHeader, FSIn>> fs_ins(w, h);

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
	for (int prim_id = 0; prim_id < primitives.size(); ++prim_id)
	{
		auto & prim = primitives[prim_id];
			
		switch (prim.m_type)
		{
		case PrimitiveType::TRIANGLE_TYPE:
		{
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
				quad_rasterize(shader, fs_ins, vsout, primitives, culling, 
					qx, qy, prim_id, vid0, vid1, vid2);
			}
		}
		break;
		case PrimitiveType::POLYGON_TYPE:
		{
			int vid0 = prim.m_vertices[0];

			for (int _i = 1; _i < prim.m_vertices.size() - 1; ++_i)
			{
				int vid1 = prim.m_vertices[_i];
				int vid2 = prim.m_vertices[_i + 1];

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
					quad_rasterize(shader, fs_ins, vsout, primitives, culling,
						qx, qy, prim_id, vid0, vid1, vid2);
				}
			}

		}
		break;
		default:
			break;
		}

	}

}

template <typename Shader, typename FSIn, typename FSOut>
void fragment_shader_stage(Buffer2D<IUINT32> & buffer, Buffer2D<IUINT32> & fsbuffer, Buffer2D<Wrapper<FSInHeader, FSIn>> & fs_ins, Shader & shader)
{
	// fragment shader
	std::cout << "fragment shader" << std::endl;
	for (int y = 0; y < fsbuffer.m_height; ++y) for (int x = 0; x < fsbuffer.m_width; ++x)
	{
		Wrapper<FSInHeader, FSIn> const & fsin = fs_ins.coeff_ref(x, y);
		if (fsin.header.prim_id < 0)
			continue;

		Wrapper<FSOutHeader, FSOut> const fsout = shader.fragment_shader(fsin);

		unsigned int r = unsigned int((std::max)(0.0f, fsout.header.color.x() * 256.f - 1.f)) & 0xff;
		unsigned int g = unsigned int((std::max)(0.0f, fsout.header.color.y() * 256.f - 1.f)) & 0xff;
		unsigned int b = unsigned int((std::max)(0.0f, fsout.header.color.z() * 256.f - 1.f)) & 0xff;

		buffer.coeff_ref(x, y) = IUINT32((r << 16) | (g << 8) | b);
	}
}

template <typename FSIn>
void clear(Buffer2D<IUINT32> & buffer, Buffer2D<IUINT32> & fsbuffer, Buffer2D<Wrapper<FSInHeader, FSIn>> & fs_ins)
{
	fsbuffer.clear(0);
	for (int y = 0; y < fs_ins.m_height; ++y) for (int x = 0; x < fs_ins.m_width; ++x)
	{
		auto & fsin = fs_ins.coeff_ref(x, y);
		fsin.header.prim_id = -1;
		//fsin.header.point_coord << x, y;
		//fsin.header.frag_coord << 0.0f, 0.0f, 0.0f, 0.0f;
		//fsin.header.depth = (std::numeric_limits<float>::max)();
		//fsin.header.interp_coord << 0.0f, 0.0f, 0.0f;
	}
}

template <typename Shader, typename VSIn, typename VSOut, typename FSIn, typename FSOut>
void pipeline(Buffer2D<IUINT32> & buffer)
{
	static float w = float(buffer.m_width);
	static float h = float(buffer.m_height);
	static Buffer2D<IUINT32> fsbuffer(w, h);
	static Buffer2D<Wrapper<FSInHeader, FSIn>> fs_ins(w, h);

	clear(buffer, fsbuffer, fs_ins);
	
	auto & vsdata = (input_assembly_stage<Shader, VSIn>())();
	Shader & shader = std::get<0>(vsdata);
	vector_with_eigen<Wrapper<VSInHeader, VSIn> > & vs_ins = std::get<1>(vsdata);
	std::vector<Primitive<int>  > & primitives = std::get<2>(vsdata);

	auto & vs_outs = vertex_shader_stage<Shader, VSIn, VSOut>(vs_ins, shader);

	//auto & primitives = primitive_assembly_stage(ebo);

	rasterize_stage<Shader, VSOut, FSIn>(shader, buffer, fsbuffer, fs_ins, vs_outs, primitives, FaceCulling::FRONT_AND_BACK);
	fragment_shader_stage<Shader, FSIn, FSOut>(buffer, fsbuffer, fs_ins, shader);

}

#endif