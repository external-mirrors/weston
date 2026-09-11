/*
 * Copyright © 2026 Collabora, Ltd.
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
#include "shared/xalloc.h"
#include "shared/weston-assert.h"
#include "libweston-internal.h"
#include "tearing-control.h"
#include "tearing-control-v1-server-protocol.h"

static void
tearing_control_base_surface_destroyed(struct wl_listener *listener, void *data)
{
	struct weston_surface *surface = data;
	struct weston_compositor *wc = surface->compositor;
	struct weston_tearing_control *tc =
		wl_container_of(listener, tc, surface_destroy_listener);

	weston_assert_ptr_eq(wc, surface, tc->surface);

	/* Surface destroyed, so tearing control becomes inert */
	tc->surface = NULL;
	wl_list_remove(&tc->surface_destroy_listener.link);
}

static void
set_presentation_hint(struct wl_client *client, struct wl_resource *resource, uint32_t hint)
{
	struct weston_tearing_control *tc = wl_resource_get_user_data(resource);
	struct weston_surface *surf = tc->surface;

	/* tearing-control is inert if the surface was destroyed */
	if (!surf)
		return;

	if (hint == WP_TEARING_CONTROL_V1_PRESENTATION_HINT_ASYNC)
		surf->pending.may_tear = true;
	else
		surf->pending.may_tear = false;

	if (surf->may_tear != surf->pending.may_tear)
		surf->pending.status |= WESTON_SURFACE_DIRTY_RATE;
}

static void
destroy_tearing_control(struct wl_client *client, struct wl_resource *res)
{
	struct weston_tearing_control *tc = wl_resource_get_user_data(res);
	struct weston_surface *surf = tc->surface;

	if (!surf)
		return;

	surf->tearing_control = NULL;
	if (surf->pending.may_tear) {
		surf->pending.may_tear = false;
		surf->pending.status |= WESTON_SURFACE_DIRTY_RATE;
	}
	wl_resource_destroy(res);
}

static const struct wp_tearing_control_v1_interface tearing_interface = {
	set_presentation_hint,
	destroy_tearing_control,
};

static void
destroy_tearing_controller(struct wl_client *client,
			   struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static void
free_tearing_control(struct wl_resource *res)
{
	struct weston_tearing_control *tc = wl_resource_get_user_data(res);
	struct weston_surface *surf = tc->surface;

	if (surf) {
		surf->tearing_control = NULL;
		wl_list_remove(&tc->surface_destroy_listener.link);
	}

	free(tc);
}

static void
get_tearing_control(struct wl_client *client,
		    struct wl_resource *resource,
		    uint32_t id,
		    struct wl_resource *surface_resource)
{
	struct wl_resource *ctl_res;
	struct weston_tearing_control *control;
	struct weston_surface *surface;
	uint32_t version;

	surface = wl_resource_get_user_data(surface_resource);
	if (surface->tearing_control) {
		wl_resource_post_error(resource,
				       WP_TEARING_CONTROL_MANAGER_V1_ERROR_TEARING_CONTROL_EXISTS,
				       "Surface already has a tearing controller");
		return;
	}

	version = wl_resource_get_version(resource);
	ctl_res = wl_resource_create(client,
				     &wp_tearing_control_v1_interface,
				     version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	control = xzalloc(sizeof *control);
	control->surface = surface;
	surface->tearing_control = control;
	control->surface_destroy_listener.notify = tearing_control_base_surface_destroyed;
	wl_signal_add(&surface->destroy_signal, &control->surface_destroy_listener);

	wl_resource_set_implementation(ctl_res, &tearing_interface,
				       control, free_tearing_control);
}

static const struct wp_tearing_control_manager_v1_interface
tearing_control_manager_implementation = {
	destroy_tearing_controller,
	get_tearing_control,
};

static void
bind_tearing_controller(struct wl_client *client, void *data,
			uint32_t version, uint32_t id)
{
	struct weston_compositor *compositor = data;
	struct wl_resource *resource;

	resource = wl_resource_create(client,
				      &wp_tearing_control_manager_v1_interface,
				      version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(resource, &tearing_control_manager_implementation,
				       compositor, NULL);
}

int
tearing_control_setup(struct weston_compositor *wc)
{
	int version = 1;

	if (!wl_global_create(wc->wl_display,
			      &wp_tearing_control_manager_v1_interface,
			      version, wc, bind_tearing_controller))
		return -1;

	return 0;
}
