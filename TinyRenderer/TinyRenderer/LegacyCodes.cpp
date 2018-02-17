#ifdef USE_LEGACY_CODES

template <typename EigenType>
Vec3f barycentric(EigenType q, EigenType p0, EigenType p1, EigenType p2)
{
	EigenType v0 = p1 - p0, v1 = p2 - p0, v2 = q - p0;
	float d00 = v0.transpose() * v0;
	float d01 = v0.transpose() * v1;
	float d11 = v1.transpose() * v1;
	float d20 = v2.transpose() * v0;
	float d21 = v2.transpose() * v1;

	float determine = d00 * d11 - d01 * d01;
	float a = (d11 * d20 - d01 * d21) / determine;
	float b = (d00 * d21 - d01 * d20) / determine;
	float c = 1.0f - a - b;

	return Vec3f(a, b, c);
}


inline void draw_point(Buffer2D<IUINT32> & buffer, int x, int y,
	int min_clip_x, int min_clip_y, int max_clip_x, int max_clip_y)
{
	if (x < min_clip_x || x > max_clip_x || y < min_clip_y || y > max_clip_y)
		return;
	buffer.coeff_ref(x, y) = 1;
}

inline void draw_scanline(Buffer2D<IUINT32> & buffer, int x_start, int x_end, int y)
{
	for (int x = x_start; x <= x_end; ++x)
	{
		buffer.coeff_ref(x, y) = 1;
	}
}

void draw_bottom_tri(Buffer2D<IUINT32> & buffer,
	int x0, int y0, int x1, int y1, int x2, int y2,
	int min_clip_x, int min_clip_y, int max_clip_x, int max_clip_y)
{
	if (x1 > x2)
		std::swap(x1, x2);

	float height = float(y2 - y0);
	float inv_k_left = (x1 - x0) / height, inv_k_right = (x2 - x0) / height;
	float x_start = float(x0), x_end = float(x0) + 0.5f;

	if (y0 < min_clip_y)
	{
		x_start += inv_k_left * (float)(min_clip_y - y0);
		x_end += inv_k_right * (float)(min_clip_y - y0);

		y0 = float(min_clip_y);
	}

	y2 = (std::min)(y2, max_clip_y);

	for (int y = y0; y <= y2; ++y)
	{
		draw_scanline(buffer, (std::max)(min_clip_x, int(x_start)), (std::min)(max_clip_x, int(x_end)), y);
		x_start += inv_k_left;
		x_end += inv_k_right;
	}

}

void draw_top_tri(Buffer2D<IUINT32> & buffer,
	int x0, int y0, int x1, int y1, int x2, int y2,
	int min_clip_x, int min_clip_y, int max_clip_x, int max_clip_y)
{
	if (x0 > x1)
		std::swap(x0, x1);

	float height = float(y2 - y0);
	float inv_k_left = (x2 - x0) / height, inv_k_right = (x2 - x1) / height;
	float x_start = float(x0), x_end = float(x1) + 0.5f;

	if (y0 < min_clip_y)
	{
		x_start += inv_k_left * (float)(min_clip_y - y0);
		x_end += inv_k_right * (float)(min_clip_y - y0);

		y0 = float(min_clip_y);
	}

	y2 = (std::min)(y2, max_clip_y);

	for (int y = y0; y <= y2; ++y)
	{
		draw_scanline(buffer, (std::max)(min_clip_x, int(x_start)), (std::min)(max_clip_x, int(x_end)), y);
		x_start += inv_k_left;
		x_end += inv_k_right;
	}
}

void draw_primitive(Buffer2D<IUINT32> & buffer,
	int x0, int y0, int x1, int y1, int x2, int y2,
	int min_clip_x, int min_clip_y, int max_clip_x, int max_clip_y)
{
	if (y0 > y1)
	{
		std::swap(x0, x1);
		std::swap(y0, y1);
	}
	if (y0 > y2)
	{
		std::swap(x0, x2);
		std::swap(y0, y2);
	}
	if (y1 > y2)
	{
		std::swap(x1, x2);
		std::swap(y1, y2);
	}
	if (y0 < min_clip_y || y2 > max_clip_y ||
		(x0 < min_clip_x && x1 < min_clip_x && x2 < min_clip_x) ||
		(x0 < min_clip_x && x1 < min_clip_x && x2 < min_clip_x))
		return;
	if (y0 == y1)
	{
		draw_top_tri(buffer, x0, y0, x1, y1, x2, y2, min_clip_x, min_clip_y, max_clip_x, max_clip_y);
	}
	else if (y1 == y2)
	{
		draw_bottom_tri(buffer, x0, y0, x1, y1, x2, y2, min_clip_x, min_clip_y, max_clip_x, max_clip_y);
	}
	else
	{
		int new_x = x0 + int(0.5f + float((y1 - y0) * (x2 - x0)) / float(y2 - y0));
		draw_bottom_tri(buffer, x0, y0, x1, y1, new_x, y1, min_clip_x, min_clip_y, max_clip_x, y1 - 1);
		draw_top_tri(buffer, new_x, y1, x1, y1, x2, y2, min_clip_x, min_clip_y, max_clip_x, max_clip_y);
	}
}

void draw_line(IUINT32 * const * buffer, int x0, int y0, int x1, int y1/*,
																	   int min_clip_x, int min_clip_y, int max_clip_x, int max_clip_y*/)
{
	/*if ((x0 < min_clip_x && x1 < min_clip_x) ||
	(x0 > max_clip_x && x1 > max_clip_x) ||
	(y0 < min_clip_y && y1 < min_clip_y) ||
	(y0 > max_clip_y && y1 > max_clip_y))
	return;*/

	/* https://segmentfault.com/a/1190000002700500
	* f(x, y) = y * delta_x - x * delta_y + x_0 * delta_y - y_0 * delta_x
	*/
	int delta_x = x1 - x0, delta_y = y1 - y0;
	int x = x0, y = y0;
	int x_end = x1, y_end = y1;
	int x_dir = x1 < x0 ? -1 : 1, y_dir = y1 < y0 ? -1 : 1;

	bool xyswaped = false;
	int f_implicit = 0;

	if ((std::abs)(delta_x) < (std::abs)(delta_y))
	{
		std::swap(delta_x, delta_y);
		std::swap(x, y);
		std::swap(x_end, y_end);
		std::swap(x_dir, y_dir);
		xyswaped = true;
	}

	if (delta_x < 0)
	{
		delta_x = -delta_x;
		delta_y = -delta_y;
		std::swap(x, x_end);
		std::swap(y, y_end);
		x_dir = -x_dir;
		y_dir = -y_dir;
	}

	for (; x <= x_end; x += x_dir)
	{
		if (xyswaped) buffer[y][x] = 1;
		else buffer[x][y] = 1;
		/* f(x+1, y) + f(x+1, y+1) < 0 => draw (x+1, y) */
		if ((y_dir >= 0) && (f_implicit + delta_y) + (f_implicit + delta_y - delta_x) < 0)
			f_implicit += delta_y;
		/* f(x+1, y) + f(x+1, y-1) > 0 => draw (x+1, y) */
		else if ((y_dir < 0) && (f_implicit + delta_y) + (f_implicit + delta_y + delta_x) > 0)
			f_implicit += delta_y;
		/* draw (x+1, y+-1) */
		else
		{
			f_implicit += delta_y - ((y_dir < 0) ? -delta_x : delta_x);
			y += y_dir;
		}
	}
}

void render(Buffer2D<IUINT32> & buffer)
{
	static float time = 4.0f;
	int w = buffer.m_width;
	int h = buffer.m_height;

	time += 0.1f;

	Vec3f p0(400.0f, 400.0f, 0.0f);
	Vec3f r1(170.0f * (std::sin)(time), 150.0f * (std::cos)(time), 0.0f);
	Vec3f r2(210.0f * (std::cos)(time * 0.5f), 130.0f * (std::sin)(time * 0.5f), 0.0f);
	Vec3f p1 = p0 + r1;
	Vec3f p2 = p0 + r2;
	Vec3f col0(100.0f, 154.0f, 230.0f);
	Vec3f col1(230.0f, 14.0f, 135.0f);
	Vec3f col2(100.0f, 221.0f, 31.0f);

	Vec3f barycentric_coordinate;

	int minx = (std::max)(0, (int)(std::min)(p0.x(), (std::min)(p1.x(), p2.x())));
	int miny = (std::max)(0, (int)(std::min)(p0.y(), (std::min)(p1.y(), p2.y())));
	int maxx = (std::min)(w - 1, (int)(std::max)(p0.x(), (std::max)(p1.x(), p2.x())));
	int maxy = (std::min)(h - 1, (int)(std::max)(p0.y(), (std::max)(p1.y(), p2.y())));

	draw_point(buffer, p1.x(), p1.y(), 0, 0, w, h);
	draw_point(buffer, p2.x(), p2.y(), 0, 0, w, h);

	for (int x = minx; x <= maxx; ++x) for (int y = miny; y <= maxy; ++y)
	{
		auto res = fragment_triangle_test(x, y,
			p0.x(), p0.y(),
			p1.x(), p1.y(),
			p2.x(), p2.y(),
			barycentric_coordinate);

		if (res == PositionTriangleTestResult::OUTSIDE ||
			res == PositionTriangleTestResult::INSIDE)
			continue;

		barycentric_coordinate = barycentric_coordinate.cwiseAbs();
		barycentric_coordinate = barycentric_coordinate /
			(barycentric_coordinate.x() + barycentric_coordinate.y() + barycentric_coordinate.z());

		auto & color = barycentric_coordinate.x() * col0 +
			barycentric_coordinate.y() * col1 +
			barycentric_coordinate.z() * col2;

		unsigned int r = color.x();
		unsigned int g = color.y();
		unsigned int b = color.z();

		buffer.coeff_ref(x, y) = ((r << 16) | (g << 8) | b);
	}

	draw_point(buffer, p1.x(), p1.y(), 0, 0, w, h);
	draw_point(buffer, p2.x(), p2.y(), 0, 0, w, h);

}

#endif