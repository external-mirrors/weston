/*
 * Copyright (C) 2026 Amazon.com, Inc. or its affiliates
 * Copyright © 2018 Collabora, Ltd.
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

#include <string.h>
#include <unistd.h>

#include "weston-test-client-helper.h"
#include "wayland-server-protocol.h"
#include "weston-test-fixture-compositor.h"
#include "weston-test-assert.h"

static enum test_result_code
fixture_setup(struct weston_test_harness *harness)
{
	struct compositor_setup setup;

	compositor_setup_defaults(&setup);

	setup.shell = SHELL_TEST_DESKTOP;
	setup.refresh = HIGHEST_OUTPUT_REFRESH;

	/* We need to use the pixman renderer, since a few of the tests depend
	 * on the renderer holding onto a surface buffer until the next one
	 * is committed, which the noop renderer doesn't do. */
	setup.renderer = WESTON_RENDERER_PIXMAN;

	return weston_test_harness_execute_as_client(harness, &setup);
}
DECLARE_FIXTURE_SETUP(fixture_setup);

static struct client *
create_test_client(void)
{
	struct client *cl = create_client_and_test_surface(0, 0, 100, 100);
	test_assert_ptr_not_null(cl);
	return cl;
}

static enum test_result_code
get_release_without_buffer_raises_commit_error(struct wet_testsuite_data *suite_data)
{
	struct client *client = create_test_client();
	struct wl_surface *surface = client->surface->wl_surface;
	struct wl_callback *buffer_release;

	buffer_release = wl_surface_get_release(surface);
	wl_surface_commit(surface);
	expect_protocol_error(
		client,
		&wl_surface_interface,
		WL_SURFACE_ERROR_NO_BUFFER);

	wl_callback_destroy(buffer_release);
	client_destroy(client);

	return RESULT_OK;
}

static enum test_result_code
get_release_after_commit_succeeds(struct wet_testsuite_data *suite_data)
{
	struct client *client = create_test_client();
	struct wl_surface *surface = client->surface->wl_surface;
	struct buffer *buf1;
	pixman_color_t black;
	struct wl_callback *buffer_release1;
	struct wl_callback *buffer_release2;

	color_rgb888(&black, 0, 0, 0);
	buf1 = create_shm_buffer_solid(client, 100, 100, &black);
	buffer_release1 = wl_surface_get_release(surface);
	client_roundtrip(client);

	wl_surface_attach(surface, buf1->proxy, 0, 0);
	wl_surface_commit(surface);

	buffer_release2 = wl_surface_get_release(surface);
	client_roundtrip(client);

	buffer_destroy(buf1);
	wl_callback_destroy(buffer_release2);
	wl_callback_destroy(buffer_release1);
	client_destroy(client);

	return RESULT_OK;
}


static void
buffer_release_callback_handler(void *data,
				struct wl_callback *buffer_release,
				uint32_t zero)
{
	int *released = data;

	test_assert_int_eq(zero, 0);

	*released += 1;
}

struct wl_callback_listener buffer_release_listener = {
	buffer_release_callback_handler
};

/* The following release event tests depend on the behavior of the used
 * backend, in this case the pixman backend. This doesn't limit their
 * usefulness, though, since it allows them to check if, given a typical
 * backend implementation, weston core supports the per commit nature of the
 * release events.
 */

static enum test_result_code
get_release_events_are_emitted_for_different_buffers(struct wet_testsuite_data *suite_data)
{
	struct client *client = create_test_client();
	struct buffer *buf1;
	struct buffer *buf2;
	pixman_color_t black;
	struct wl_surface *surface = client->surface->wl_surface;
	struct wl_callback *buffer_release1;
	struct wl_callback *buffer_release2;
	int buf_released1 = 0;
	int buf_released2 = 0;
	int frame;

	color_rgb888(&black, 0, 0, 0);
	buf1 = create_shm_buffer_solid(client, 100, 100, &black);
	buf2 = create_shm_buffer_solid(client, 100, 100, &black);

	buffer_release1 = wl_surface_get_release(client->surface->wl_surface);
	wl_callback_add_listener(buffer_release1, &buffer_release_listener,
				 &buf_released1);
	wl_surface_attach(surface, buf1->proxy, 0, 0);
	frame_callback_set(surface, &frame);
	wl_surface_commit(surface);
	frame_callback_wait(client, &frame);
	/* No release event should have been emitted yet (we are using the
	 * pixman renderer, which holds buffers until they are replaced). */
	test_assert_int_eq(buf_released1, 0);

	buffer_release2 = wl_surface_get_release(client->surface->wl_surface);
	wl_callback_add_listener(buffer_release2, &buffer_release_listener,
				 &buf_released2);
	wl_surface_attach(surface, buf2->proxy, 0, 0);
	frame_callback_set(surface, &frame);
	wl_surface_commit(surface);
	frame_callback_wait(client, &frame);
	/* Check that exactly one buffer_release event was emitted for the
	 * previous commit (buf1). */
	test_assert_int_eq(buf_released1, 1);
	test_assert_int_eq(buf_released2, 0);

	wl_surface_attach(surface, buf1->proxy, 0, 0);
	frame_callback_set(surface, &frame);
	wl_surface_commit(surface);
	frame_callback_wait(client, &frame);
	/* Check that exactly one buffer_release event was emitted for the
	 * previous commit (buf2). */
	test_assert_int_eq(buf_released1, 1);
	test_assert_int_eq(buf_released2, 1);

	buffer_destroy(buf2);
	buffer_destroy(buf1);
	wl_callback_destroy(buffer_release2);
	wl_callback_destroy(buffer_release1);
	client_destroy(client);

	return RESULT_OK;
}

static enum test_result_code
get_release_events_are_emitted_for_same_buffer_on_surface(struct wet_testsuite_data *suite_data)
{
	struct client *client = create_test_client();
	struct buffer *buf;
	pixman_color_t black;
	struct wl_surface *surface = client->surface->wl_surface;
	struct wl_callback *buffer_release1;
	struct wl_callback *buffer_release2;
	int buf_released1 = 0;
	int buf_released2 = 0;
	int frame;

	color_rgb888(&black, 0, 0, 0);
	buf = create_shm_buffer_solid(client, 100, 100, &black);
	buffer_release1 = wl_surface_get_release(surface);
	wl_callback_add_listener(buffer_release1, &buffer_release_listener,
				 &buf_released1);
	wl_surface_attach(surface, buf->proxy, 0, 0);
	frame_callback_set(surface, &frame);
	wl_surface_commit(surface);
	frame_callback_wait(client, &frame);
	/* No release event should have been emitted yet (we are using the
	 * pixman renderer, which holds buffers until they are replaced). */
	test_assert_int_eq(buf_released1, 0);

	buffer_release2 = wl_surface_get_release(surface);
	wl_callback_add_listener(buffer_release2, &buffer_release_listener,
				 &buf_released2);
	wl_surface_attach(surface, buf->proxy, 0, 0);
	frame_callback_set(surface, &frame);
	wl_surface_commit(surface);
	frame_callback_wait(client, &frame);
	/* Check that exactly one buffer_release event was emitted for the
	 * previous commit (buf). */
	test_assert_int_eq(buf_released1, 1);
	test_assert_int_eq(buf_released2, 0);

	wl_surface_attach(surface, buf->proxy, 0, 0);
	frame_callback_set(surface, &frame);
	wl_surface_commit(surface);
	frame_callback_wait(client, &frame);
	/* Check that exactly one buffer_release event was emitted for the
	 * previous commit (buf again). */
	test_assert_int_eq(buf_released1, 1);
	test_assert_int_eq(buf_released2, 1);

	buffer_destroy(buf);
	wl_callback_destroy(buffer_release2);
	wl_callback_destroy(buffer_release1);
	client_destroy(client);

	return RESULT_OK;
}

static enum test_result_code
get_release_events_are_emitted_for_same_buffer_on_different_surfaces(struct wet_testsuite_data *suite_data)
{
	struct client *client = create_test_client();
	struct surface *other_surface = create_test_surface(client);
	struct wl_surface *surface1 = client->surface->wl_surface;
	struct wl_surface *surface2 = other_surface->wl_surface;
	struct buffer *buf1;
	struct buffer *buf2;
	pixman_color_t black;
	struct wl_callback *buffer_release1;
	struct wl_callback *buffer_release2;
	int buf_released1 = 0;
	int buf_released2 = 0;
	int frame;

	color_rgb888(&black, 0, 0, 0);
	buf1 = create_shm_buffer_solid(client, 100, 100, &black);
	buf2 = create_shm_buffer_solid(client, 100, 100, &black);

	weston_test_move_surface(client->test->weston_test, surface2, 0, 0);

	/* Attach buf1 to both surface1 and surface2. */
	buffer_release1 = wl_surface_get_release(surface1);
	wl_callback_add_listener(buffer_release1,
				 &buffer_release_listener,
				 &buf_released1);
	wl_surface_attach(surface1, buf1->proxy, 0, 0);
	frame_callback_set(surface1, &frame);
	wl_surface_commit(surface1);
	frame_callback_wait(client, &frame);

	buffer_release2 = wl_surface_get_release(surface2);
	wl_callback_add_listener(buffer_release2, &buffer_release_listener,
				 &buf_released2);
	wl_surface_attach(surface2, buf1->proxy, 0, 0);
	frame_callback_set(surface2, &frame);
	wl_surface_commit(surface2);
	frame_callback_wait(client, &frame);

	test_assert_int_eq(buf_released1, 0);
	test_assert_int_eq(buf_released2, 0);

	/* Attach buf2 to surface1, and check that a buffer_release event for
	 * the previous commit (buf1) for that surface is emitted. */
	wl_surface_attach(surface1, buf2->proxy, 0, 0);
	frame_callback_set(surface1, &frame);
	wl_surface_commit(surface1);
	frame_callback_wait(client, &frame);

	test_assert_int_eq(buf_released1, 1);
	test_assert_int_eq(buf_released2, 0);

	/* Attach buf2 to surface2, and check that a buffer_release event for
	 * the previous commit (buf1) for that surface is emitted. */
	wl_surface_attach(surface2, buf2->proxy, 0, 0);
	frame_callback_set(surface2, &frame);
	wl_surface_commit(surface2);
	frame_callback_wait(client, &frame);

	test_assert_int_eq(buf_released1, 1);
	test_assert_int_eq(buf_released2, 1);

	buffer_destroy(buf2);
	buffer_destroy(buf1);
	wl_callback_destroy(buffer_release2);
	wl_callback_destroy(buffer_release1);
	surface_destroy(other_surface);
	client_destroy(client);

	return RESULT_OK;
}

DECLARE_TEST_LIST(
	TESTFN(get_release_without_buffer_raises_commit_error),
	TESTFN(get_release_after_commit_succeeds),
	TESTFN(get_release_events_are_emitted_for_different_buffers),
	TESTFN(get_release_events_are_emitted_for_same_buffer_on_surface),
	TESTFN(get_release_events_are_emitted_for_same_buffer_on_different_surfaces),
);
