/*
 * Copyright (C) 2026 Amazon.com, Inc. or its affiliates
 * Copyright © 2019 Collabora, Ltd.
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

#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>

#include <libweston/libweston.h>
#include "linux-dmabuf.h"
#include "weston-restricted-buffer-server-protocol.h"
#include "libweston-internal.h"

static void
restricted_buffer_enable(struct wl_client *client,
			 struct wl_resource *resource,
			 struct wl_resource *dmabuf_res)
{
	struct linux_dmabuf_buffer *dmabuf;

	dmabuf = wl_resource_get_user_data(dmabuf_res);
	if (!dmabuf) {
		wl_resource_post_error(resource, WESTON_RESTRICTED_BUFFER_V1_ERROR_PARAMS_ALREADY_USED,
				       "params was already used to create a wl_buffer");
		return;
	}

	if (dmabuf->restriction != WESTON_BUFFER_RESTRICTION_NO) {
		wl_resource_post_error(resource, WESTON_RESTRICTED_BUFFER_V1_ERROR_ALREADY_SET,
				       "restriction already set for params");
		return;
	}

	dmabuf->restriction = WESTON_BUFFER_RESTRICTION_YES;
}

static void
restricted_buffer_destroy(struct wl_client *client,
			  struct wl_resource *global_resource)
{
	wl_resource_destroy(global_resource);
}

static const struct weston_restricted_buffer_v1_interface
	weston_restricted_buffer_interface_v1 = {
		.enable = restricted_buffer_enable,
		.destroy = restricted_buffer_destroy,
};

static void
bind_restricted_buffer(struct wl_client *client, void *data,
		       uint32_t version, uint32_t id)
{
	struct wl_resource *resource;
	struct weston_compositor *ec = data;

	resource = wl_resource_create(client,
			&weston_restricted_buffer_v1_interface,
			version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(resource,
				       &weston_restricted_buffer_interface_v1,
				       ec, NULL);
}

WL_EXPORT int
weston_restricted_buffer_setup(struct weston_compositor *ec)
{
	if (!wl_global_create(ec->wl_display,
			      &weston_restricted_buffer_v1_interface, 1,
			      ec, bind_restricted_buffer))
		return -1;

	return 0;
}
