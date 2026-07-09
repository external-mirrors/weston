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

#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <png.h>

#include "shared/string-helpers.h"
#include "shared/xalloc.h"

#define LIBPNG_AT_LEAST(a, b, c) (PNG_LIBPNG_VER >= (a) * 10000 + (b) * 100 + (c))

struct weston_png_writer_context {
	const struct weston_png_write_task *task;
	FILE *dest;

	png_structp png_wr;
	png_infop png_info;

	pixman_image_t *rowtmp;

	jmp_buf jmpbuf;
	bool failed;
};

static bool
weston_png_writer_context_fini(struct weston_png_writer_context *ctx)
{
	int ret;

	if (ctx->rowtmp)
		pixman_image_unref(ctx->rowtmp);

	png_destroy_write_struct(&ctx->png_wr, &ctx->png_info);

	ret = fflush(ctx->dest);

	return ret == 0 && !ctx->failed;
}

static void
write_error(png_structp png_wr, const char *msg)
{
	struct weston_png_writer_context *ctx = png_get_error_ptr(png_wr);

	ctx->failed = true;
	fprintf(stderr, "PNG writing error: %s\n", msg);

	longjmp(ctx->jmpbuf, 1);
}

static void
write_warning(png_structp png_wr, const char *msg)
{
	fprintf(stderr, "PNG writing warning: %s\n", msg);
}

static bool
is_format_acceptable(pixman_format_code_t fmt)
{
	int type = PIXMAN_FORMAT_TYPE(fmt);

	switch (type) {
	case PIXMAN_TYPE_ARGB:
	case PIXMAN_TYPE_ABGR:
	case PIXMAN_TYPE_BGRA:
	case PIXMAN_TYPE_RGBA:
		return true;
	default:
		return false;
	}
}

static png_color_8
to_png_significant_bits(pixman_format_code_t fmt)
{
	return (png_color_8) {
		.alpha = PIXMAN_FORMAT_A(fmt),
		.red = PIXMAN_FORMAT_R(fmt),
		.green = PIXMAN_FORMAT_G(fmt),
		.blue = PIXMAN_FORMAT_B(fmt),
	};
}

static int
to_png_bit_depth(pixman_format_code_t fmt)
{
#if HAVE_PIXMAN_16_BPC
	if (PIXMAN_FORMAT_A(fmt) > 8 ||
	    PIXMAN_FORMAT_R(fmt) > 8 ||
	    PIXMAN_FORMAT_G(fmt) > 8 ||
	    PIXMAN_FORMAT_B(fmt) > 8)
		return 16;
#endif

	return 8;
}

static int
to_png_color_type(pixman_format_code_t fmt)
{
	if (PIXMAN_FORMAT_A(fmt) > 0)
		return PNG_COLOR_TYPE_RGB_ALPHA;

	return PNG_COLOR_TYPE_RGB;
}
static void
set_sRGB(struct weston_png_writer_context *ctx)
{
	png_set_sRGB_gAMA_and_cHRM(ctx->png_wr, ctx->png_info,
				   PNG_sRGB_INTENT_PERCEPTUAL);
}

static void
set_iCCP(struct weston_png_writer_context *ctx)
{
	const struct weston_png_write_task *task = ctx->task;

	/* The size check is arbitrary */
	if (task->icc_profile_len == 0 ||
	    task->icc_profile_len > UINT32_MAX / 4) {
		png_error(ctx->png_wr, "png-writer.c: bad ICC data length");
	} else {
		png_set_iCCP(ctx->png_wr, ctx->png_info, "custom ICC",
			     PNG_COMPRESSION_TYPE_BASE,
			     task->icc_profile, task->icc_profile_len);
	}
}

static void
print_gamut(FILE *fp, const char *prefix, const struct weston_color_gamut *g)
{
	unsigned i;

	for (i = 0; i < 3; i++) {
		fprintf(fp, "%s_%c=%f %f\n", prefix, "rgb"[i],
			g->primary[i].x, g->primary[i].y);
	}
	fprintf(fp, "%s_w=%f %f\n", prefix, g->white_point.x, g->white_point.y);
}

static char *
weston_color_profile_params_to_string(const struct weston_color_profile_params *p)
{
	char *str;
	size_t str_size;
	FILE *fp;
	unsigned i, n;

	fp = open_memstream(&str, &str_size);
	if (!fp)
		return NULL;

	fprintf(fp, "tf=%s", weston_color_tf_info_get_codeword(p->tf.info));
	n = weston_color_tf_info_get_parameter_count(p->tf.info);
	for (i = 0; i < n; i++)
		fprintf(fp, " %f", p->tf.params[i]);
	fputs("\n", fp);

	if (p->primaries_info) {
		fprintf(fp, "primaries=%s\n",
			weston_color_primaries_info_get_codeword(p->primaries_info));
	} else {
		print_gamut(fp, "pri", &p->primaries);
	}

	fprintf(fp,
		"min_lum=%f\n"
		"max_lum=%f\n"
		"ref_lum=%f\n"
		"tgt_min_lum=%f\n"
		"tgt_max_lum=%f\n",
		p->min_luminance,
		p->max_luminance,
		p->reference_white_luminance,
		p->target_min_luminance,
		p->target_max_luminance);

	print_gamut(fp, "tgt", &p->target_primaries);

	if (p->maxCLL > 0.0f)
		fprintf(fp, "maxCLL=%f\n", p->maxCLL);
	if (p->maxFALL > 0.0f)
		fprintf(fp, "maxFALL=%f\n", p->maxFALL);

	if (fclose(fp) != 0)
		return NULL;

	return str;
}

static void
set_parametric(struct weston_png_writer_context *ctx)
{
	const struct weston_color_profile_params *p = ctx->task->parametric;
	bool cICP_ok = false;

#if LIBPNG_AT_LEAST(1, 6, 45)
	uint8_t colour_primaries = 0;
	uint8_t transfer_function;

	transfer_function = weston_color_tf_info_get_cicp(p->tf.info);
	if (p->primaries_info)
		colour_primaries = weston_color_primaries_info_get_cicp(p->primaries_info);

	if (transfer_function != 0 &&
	    colour_primaries != 0) {
		png_set_cICP(ctx->png_wr, ctx->png_info,
			     colour_primaries,
			     transfer_function,
			     0, 1);
		cICP_ok = true;
	}
#endif

	enum weston_transfer_function tf;

	tf = weston_color_tf_info_get_enum(p->tf.info);
	switch (tf) {
	case WESTON_TF_GAMMA22:
		png_set_gAMA(ctx->png_wr, ctx->png_info, 2.2);
		break;
	case WESTON_TF_GAMMA28:
		png_set_gAMA(ctx->png_wr, ctx->png_info, 2.8);
		break;
	case WESTON_TF_POWER:
		png_set_gAMA(ctx->png_wr, ctx->png_info, p->tf.params[0]);
		break;
	default:
		if (!cICP_ok)
			png_error(ctx->png_wr, "png-writer.c: could not store transfer function");
		break;
	}

	png_set_cHRM(ctx->png_wr, ctx->png_info,
		     p->primaries.white_point.x, p->primaries.white_point.y,
		     p->primaries.primary[0].x, p->primaries.primary[0].y,
		     p->primaries.primary[1].x, p->primaries.primary[1].y,
		     p->primaries.primary[2].x, p->primaries.primary[2].y);

#if LIBPNG_AT_LEAST(1, 6, 48)
	png_set_mDCV(ctx->png_wr, ctx->png_info,
		     p->target_primaries.white_point.x, p->target_primaries.white_point.y,
		     p->target_primaries.primary[0].x, p->target_primaries.primary[0].y,
		     p->target_primaries.primary[1].x, p->target_primaries.primary[1].y,
		     p->target_primaries.primary[2].x, p->target_primaries.primary[2].y,
		     p->target_max_luminance, p->target_min_luminance);
#endif

#if LIBPNG_AT_LEAST(1, 6, 46)
	double maxCLL = p->maxCLL > 0.0 ? p->maxCLL : 0.0;
	double maxFALL = p->maxFALL > 0.0 ? p->maxFALL : 0.0;

	if (maxCLL > 0.0 || maxFALL > 0.0)
		png_set_cLLI(ctx->png_wr, ctx->png_info, maxCLL, maxFALL);
#endif

	char *imdesc_str = weston_color_profile_params_to_string(p);
	if (!imdesc_str)
		png_error(ctx->png_wr, "png-writer.c: parametric image description string formatting failed");

	png_text imdesc = {
		.compression = PNG_TEXT_COMPRESSION_NONE,
		.key = "Weston parametric image description",
		.text = imdesc_str,
		.text_length = strlen(imdesc_str),
	};
	png_set_text(ctx->png_wr, ctx->png_info, &imdesc, 1);
	free(imdesc_str);
}

static void
write_pixel_data(struct weston_png_writer_context *ctx)
{
	pixman_image_t *img = ctx->task->img;
	pixman_format_code_t pixfmt;
	pixman_format_code_t dstfmt;
	uint32_t width;
	uint32_t height;
	uint32_t stride_bytes;
	const uint8_t *pixdata;
	const uint8_t *rowdata = NULL;
	uint32_t i;

	width = pixman_image_get_width(img);
	height = pixman_image_get_height(img);
	stride_bytes = pixman_image_get_stride(img);
	pixfmt = pixman_image_get_format(img);
	pixdata = (const void *)pixman_image_get_data(img);

	/*
	 * PNG file format stores channels in R, G, B, A order. Pixman formats
	 * are CPU-endian. On little-endian CPU PIXMAN_a8b8g8r8 stores R first,
	 * then G, B, A and matches 8-bit PNG format directly.
	 *
	 * On big-endian CPU PIXMAN_a8b8g8r8 stores A first, then B, G, R. There
	 * the ordering must be reversed.
	 */

#if HAVE_PIXMAN_16_BPC
	if (to_png_bit_depth(pixfmt) > 8) {
		dstfmt = PIXMAN_a16b16g16r16;
#  if __BYTE_ORDER == __LITTLE_ENDIAN
		png_set_swap(ctx->png_wr);
#  endif
	} else
#endif
	if (PIXMAN_FORMAT_A(pixfmt) > 0) {
		dstfmt = PIXMAN_a8b8g8r8;
	} else {
		dstfmt = PIXMAN_b8g8r8;
	}

#if __BYTE_ORDER == __BIG_ENDIAN
	png_set_bgr(ctx->png_wr); /* Reorder RGB as BGR */
	if (PIXMAN_FORMAT_A(dstfmt) > 0)
		png_set_swap_alpha(ctx->png_wr); /* Reorder abcA as Aabc */
#endif

	if (dstfmt != pixfmt) {
		ctx->rowtmp = pixman_image_create_bits(dstfmt, width, 1, NULL, 0);
		abort_oom_if_null(ctx->rowtmp);
		rowdata = (const void *)pixman_image_get_data(ctx->rowtmp);
	}

	for (i = 0; i < height; i++) {
		const uint8_t *row;

		if (dstfmt == pixfmt) {
			row = pixdata + i * stride_bytes;
		} else {
			pixman_image_composite32(PIXMAN_OP_SRC, img, NULL, ctx->rowtmp,
						 0, i, 0, 0, 0, 0, width, 1);
			row = rowdata;
		}

		png_write_row(ctx->png_wr, row);
	}
}

/*
 * This function exists to convert a longjmp() into a normal return.
 * Libpng mandates that the error handler function must not return.
 * Hence, it uses longjmp().
 *
 * Here we choose to immediately 'return' when setjmp() returns non-zero.
 * That way there is no need to figure out which variables need to be
 * 'volatile'.
 */
static void
png_write_throws(struct weston_png_writer_context *ctx)
{
	const struct weston_png_write_task *task = ctx->task;
	pixman_image_t *img = task->img;
	pixman_format_code_t pixfmt;
	png_color_8 significant_bits;

	pixfmt = pixman_image_get_format(img);

	if (setjmp(ctx->jmpbuf))
		return;

	png_init_io(ctx->png_wr, ctx->dest);

	png_set_IHDR(ctx->png_wr, ctx->png_info,
		     pixman_image_get_width(img),
		     pixman_image_get_height(img),
		     to_png_bit_depth(pixfmt),
		     to_png_color_type(pixfmt),
		     PNG_INTERLACE_NONE,
		     PNG_COMPRESSION_TYPE_DEFAULT,
		     PNG_FILTER_TYPE_DEFAULT);

	significant_bits = to_png_significant_bits(pixfmt);
	png_set_sBIT(ctx->png_wr, ctx->png_info, &significant_bits);

	if (!task->parametric && !task->icc_profile)
		set_sRGB(ctx);

	if (task->icc_profile)
		set_iCCP(ctx);

	if (task->parametric)
		set_parametric(ctx);

	png_write_info(ctx->png_wr, ctx->png_info);

	write_pixel_data(ctx);
	png_write_end(ctx->png_wr, ctx->png_info);
}

/**
 * Write a PNG file into a FILE stream
 *
 * \param destination The stream to write into.
 * \param task Image details.
 * \param errmsg Pointer to a place where to store a malloc'd pointer
 * if this function fails. The malloc'd memory must be freed by the caller.
 * \return True for success, \c errmsg will be untouched. False for failure,
 * an error string is returned through \c errmsg .
 */
bool
weston_png_write_stream(FILE *destination,
			const struct weston_png_write_task *task,
			char **errmsg)
{
	struct weston_png_writer_context ctx = {
		.task = task,
		.dest = destination,
		.failed = false,
	};
	pixman_format_code_t pixfmt;

	pixfmt = pixman_image_get_format(task->img);
	if (!is_format_acceptable(pixfmt)) {
		str_printf(errmsg, "unsupported pixman format 0x%08x", pixfmt);
		return false;
	}

	if (task->icc_profile && task->parametric) {
		str_printf(errmsg, "cannot use both ICC profile and parametric simultaneously");
		return false;
	}

	ctx.png_wr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
					     &ctx, write_error, write_warning);
	if (!ctx.png_wr)
		return false;

	ctx.png_info = png_create_info_struct(ctx.png_wr);
	if (!ctx.png_info) {
		weston_png_writer_context_fini(&ctx);
		return false;
	}

	png_write_throws(&ctx);

	if (weston_png_writer_context_fini(&ctx)) {
		return true;
	} else {
		str_printf(errmsg, "translating image to PNG data failed");
		return false;
	}
}

/**
 * Write a PNG file by file name
 *
 * \param fname The path to the file to create or replace.
 * \param task Image details.
 * \param errmsg Pointer to a place where to store a malloc'd pointer
 * if this function fails. The malloc'd memory must be freed by the caller.
 * \return True for success, \c errmsg will be untouched. False for failure,
 * an error string is returned through \c errmsg .
 */
bool
weston_png_write_path(const char *fname,
		      const struct weston_png_write_task *task,
		      char **errmsg)
{
	FILE *dest;
	bool ret;

	dest = fopen(fname, "wb");
	if (!dest)
		return errno_error(errmsg, "trying to create PNG file '%s'", fname);

	ret = weston_png_write_stream(dest, task, errmsg);
	if (fclose(dest) != 0 && ret)
		ret = errno_error(errmsg, "trying to write PNG file '%s'", fname);

	if (!ret)
		unlink(fname);

	return ret;
}
