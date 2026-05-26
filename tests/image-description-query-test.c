/*
 * Copyright 2023 Collabora, Ltd.
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

#include "color-manager-client.h"
#include "weston-test-client-helper.h"
#include "weston-test-fixture-compositor.h"
#include "weston-test-assert.h"

#include "color-management-v1-client-protocol.h"

static struct color_manager_client *
color_manager_get(struct client *client)
{
	struct color_manager_client *cm = client_get_color_manager(client, 1);

	/* Weston supports all color features. */
	test_assert_u32_eq(cm->supported_features,
			   (1 << WP_COLOR_MANAGER_V1_FEATURE_ICC_V2_V4) |
			   (1 << WP_COLOR_MANAGER_V1_FEATURE_PARAMETRIC) |
			   (1 << WP_COLOR_MANAGER_V1_FEATURE_SET_PRIMARIES) |
			   (1 << WP_COLOR_MANAGER_V1_FEATURE_SET_TF_POWER) |
			   (1 << WP_COLOR_MANAGER_V1_FEATURE_SET_LUMINANCES) |
			   (1 << WP_COLOR_MANAGER_V1_FEATURE_SET_MASTERING_DISPLAY_PRIMARIES) |
			   (1 << WP_COLOR_MANAGER_V1_FEATURE_EXTENDED_TARGET_VOLUME));

	/* Weston supports all rendering intents. */
	test_assert_u32_eq(cm->supported_rendering_intents,
			   (1 << WP_COLOR_MANAGER_V1_RENDER_INTENT_PERCEPTUAL) |
			   (1 << WP_COLOR_MANAGER_V1_RENDER_INTENT_RELATIVE) |
			/* (1 << WP_COLOR_MANAGER_V1_RENDER_INTENT_SATURATION) | */
			   (1 << WP_COLOR_MANAGER_V1_RENDER_INTENT_ABSOLUTE) |
			   (1 << WP_COLOR_MANAGER_V1_RENDER_INTENT_RELATIVE_BPC));

	test_assert_true(cm->init_done);

	return cm;
}

static enum test_result_code
fixture_setup(struct weston_test_harness *harness)
{
	struct compositor_setup setup;

	compositor_setup_defaults(&setup);
        setup.renderer = WESTON_RENDERER_GL;
	setup.shell = SHELL_TEST_DESKTOP;
	setup.refresh = HIGHEST_OUTPUT_REFRESH;

	weston_ini_setup(&setup,
			 cfgln("[core]"),
			 cfgln("color-management=true"));

	return weston_test_harness_execute_as_client(harness, &setup);
}
DECLARE_FIXTURE_SETUP(fixture_setup);

static enum test_result_code
output_get_image_description(struct wet_testsuite_data *suite_data)
{
	struct client *client;
	struct color_manager_client *cm;
	struct image_description *image_descr;
	struct image_description_info *info;

	client = create_client_and_test_surface(100, 100, 100, 100);
	cm = color_manager_get(client);

	/* Get image description from output */
	image_descr = image_description_create_for_output(cm, client->output);
	image_description_wait_until_ready(client, image_descr);

	/* Get output image description information */
	info = image_description_get_information(client, image_descr);

	image_description_info_destroy(info);
	image_description_destroy(image_descr);
	client_destroy(client);

	return RESULT_OK;
}

static enum test_result_code
surface_get_preferred_image_description(struct wet_testsuite_data *suite_data)
{
	struct client *client;
	struct color_manager_client *cm;
	struct image_description *image_descr;
	struct image_description_info *info;

	client = create_client_and_test_surface(100, 100, 100, 100);
	cm = color_manager_get(client);

	/* Get preferred image description from surface */
	image_descr = image_description_create_for_preferred(cm, client->surface);
	image_description_wait_until_ready(client, image_descr);

	/* Get surface image description information */
	info = image_description_get_information(client, image_descr);

	image_description_info_destroy(info);
	image_description_destroy(image_descr);
	client_destroy(client);

	return RESULT_OK;
}

DECLARE_TEST_LIST(
	TESTFN(output_get_image_description),
	TESTFN(surface_get_preferred_image_description),
);
