/*
 * Copyright 2026 Collabora, Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "config.h"

#include "png-writer.h"

#include <stdio.h>
#include <sys/stat.h>
#include <pixman.h>

#include "shared/helpers.h"
#include "shared/xalloc.h"

#define WHITE(x) ((pixman_color_t){ x, x, x, 65535 })
#define RED(x) ((pixman_color_t){ x, 0, 0, 65535 })
#define GREEN(x) ((pixman_color_t){ 0, x, 0, 65535 })
#define BLUE(x) ((pixman_color_t){ 0, 0, x, 65535 })

struct blob {
	void *data;
	size_t len;
};

static struct blob
load_file(const char *path)
{
	FILE *fp;
	struct stat info;
	struct blob blob;
	size_t count;

	fp = fopen(path, "rb");
	if (!fp) {
		perror("Could not open file");
		exit(1);
	}

	if (fstat(fileno(fp), &info) != 0) {
		perror("stat() fail");
		exit(1);
	}

	blob.len = info.st_size;
	blob.data = malloc(blob.len);

	for (count = 0; count < blob.len; ) {
		size_t r;

		r = fread(blob.data + count, 1, blob.len - count, fp);
		if (r == 0)
			break;

		count += r;
	}

	if (ferror(fp)) {
		perror("File reading error");
		exit(1);
	}

	if (count != blob.len) {
		fprintf(stderr, "File got truncated while reading?\n");
		exit(1);
	}

	fclose(fp);

	return blob;
}

int
main(int argc, char *argv[])
{
	pixman_image_t *img;
	const unsigned w = 801;
	const unsigned h = 777;

	img = pixman_image_create_bits(PIXMAN_a2r10g10b10, w, h, NULL, 0);
	abort_oom_if_null(img);

	pixman_image_t *fill;
	fill = pixman_image_create_solid_fill(&WHITE(10000));
	pixman_image_composite32(PIXMAN_OP_SRC, fill, NULL, img,
				 0, 0, 0, 0, 0, 0, w - 20, h);
	pixman_image_unref(fill);

	pixman_gradient_stop_t white_stops[] = {
		{ pixman_double_to_fixed(0.0), WHITE(0) },
		{ pixman_double_to_fixed(1.0), WHITE(65535) },
	};
	pixman_gradient_stop_t red_stops[] = {
		{ pixman_double_to_fixed(0.0), WHITE(0) },
		{ pixman_double_to_fixed(0.5), RED(65535) },
		{ pixman_double_to_fixed(1.0), WHITE(65535) },
	};
	pixman_gradient_stop_t green_stops[] = {
		{ pixman_double_to_fixed(0.0), WHITE(0) },
		{ pixman_double_to_fixed(0.5), GREEN(65535) },
		{ pixman_double_to_fixed(1.0), WHITE(65535) },
	};
	pixman_gradient_stop_t blue_stops[] = {
		{ pixman_double_to_fixed(0.0), WHITE(0) },
		{ pixman_double_to_fixed(0.5), BLUE(65535) },
		{ pixman_double_to_fixed(1.0), WHITE(65535) },
	};
	struct gradient {
		const pixman_gradient_stop_t *stops;
		unsigned stops_len;
	} gradients[] = {
		{ white_stops, ARRAY_LENGTH(white_stops) },
		{ red_stops, ARRAY_LENGTH(red_stops) },
		{ green_stops, ARRAY_LENGTH(green_stops) },
		{ blue_stops, ARRAY_LENGTH(blue_stops) },
	};

	unsigned N = ARRAY_LENGTH(gradients);
	for (unsigned i = 0; i < N; i++) {
		pixman_image_t *grad;
		int margin = 80;
		int wid = w - 20 - 2 * margin;
		int hei = (h - (N + 1) * margin) / N;
		int y = margin + i * (hei + margin);
		pixman_point_fixed_t p1 = {
			pixman_int_to_fixed(0),
			pixman_int_to_fixed(0),
		};
		pixman_point_fixed_t p2 = {
			pixman_int_to_fixed(wid),
			pixman_int_to_fixed(0),
		};
		const struct gradient *g = &gradients[i];

		grad = pixman_image_create_linear_gradient(&p1, &p2, g->stops, g->stops_len);
		pixman_image_composite32(PIXMAN_OP_SRC, grad, NULL, img,
					 0, 0,
					 0, 0,
					 margin, y,
					 wid, hei);
		pixman_image_unref(grad);
	}

	struct blob icc_data = {};

	if (0)
		icc_data = load_file("/usr/share/color/icc/colord/ProPhotoRGB.icc");

	struct weston_color_profile_params *parametric = NULL;

	if (1) {
		struct weston_color_profile_param_builder *b;
		char *errmsg = NULL;

		b = weston_color_profile_param_builder_create(NULL);
		weston_color_profile_param_builder_set_tf_named(b, WESTON_TF_ST2084_PQ);
		weston_color_profile_param_builder_set_primaries_named(b, WESTON_PRIMARIES_BT2020);
		weston_color_profile_param_builder_set_primary_luminance(b, 100.0f, 0.0f, 10000.0f);
		weston_color_profile_param_builder_set_target_luminance(b, 0.5f, 1000.0f);
		weston_color_profile_param_builder_set_maxCLL(b, 670.0f);
		weston_color_profile_param_builder_set_maxFALL(b, 85.0f);

		parametric = weston_color_profile_param_builder_create_params(b, NULL, &errmsg);
		if (!parametric) {
			fprintf(stderr, "Failed to create a parametric profile: %s\n", errmsg);
			free(errmsg);
			exit(1);
		}
	}

	const char *fname = "writer-test.png";
	struct weston_png_write_task task = {
		.img = img,
		.icc_profile = icc_data.data,
		.icc_profile_len = icc_data.len,
		.parametric = parametric,
	};

	char *errmsg = NULL;
	if (!weston_png_write_path(fname, &task, &errmsg)) {
		fprintf(stderr, "Writing %s failed: %s\n", fname, errmsg);
		free(errmsg);
		exit(1);
	}

	free(icc_data.data);
	free(parametric);
	pixman_image_unref(img);

	return 0;
}
