/*
 * Copyright (C) 2026 Amazon.com, Inc. or its affiliates
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

#pragma once

#include "perfetto/u_perfetto.h"

struct weston_presentation_feedback;

void
weston_trace_flow_start(struct weston_trace_flow *flow);

void
weston_trace_flow_join(struct weston_trace_flow *target,
		       struct weston_trace_flow *flow);

void
weston_trace_client_init(struct weston_client *client);

void
weston_trace_client_fini(struct weston_client *client);

struct weston_trace_flow
weston_trace_client_action(struct wl_client *wclient,
			   const char *action);

void
weston_trace_output_init(struct weston_output *output);

void
weston_trace_output_fini(struct weston_output *output);

void
weston_trace_surface_init(struct weston_surface *surface,
			  struct weston_client *client);

void
weston_trace_surface_update(struct weston_surface *surface,
			    const char *new_label);

void
weston_trace_surface_fini(struct weston_surface *surface);

void
weston_trace_feedback_create(struct weston_surface *surface,
			     struct weston_surface_state *state);

bool
weston_trace_feedback_discard(struct weston_presentation_feedback *feedback);

bool
weston_trace_feedback_present(struct weston_presentation_feedback *feedback,
			      struct weston_output *output,
			      uint32_t refresh_nsec,
			      const struct timespec *ts,
			      uint64_t seq,
			      uint32_t flags);
