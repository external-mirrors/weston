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

#include "config.h"
#include <libweston/libweston.h>

#include "perfetto/annotations.h"
#include "perfetto/trace-helpers.h"
#include "shared/xalloc.h"
#include "shared/timespec-util.h"
#include "weston-trace.h"

static void
weston_trace_track_clear(struct weston_trace_track *track)
{
	if (!track->id)
		return;

	/* This is a bit nasty... We don't track whether we have a stack
	 * of ongoing events on a track. Currently we never put more than
	 * one event on our custom tracks, so just send an end in case.
	 *
	 * This prevents a client exit from leaving a smear of unended
	 * events to eternity.
	 *
	 * In the future if we start making more complicated custom tracks
	 * we may need to keep a count and close them all.
	 */
	util_perfetto_trace_end(track->id);
	util_perfetto_track_clear(track->id);
	track->id = 0;
}

static void
weston_trace_client_setup_name(struct weston_client *client)
{
	char friendly_name[WESTON_TRACE_NAME_SIZE];
	char pathname[60];
	FILE *procfile;
	char *start;
	int len;

	/* Try to grab a filename from proc. if we fail for any reason just
	 * use the internal id.
	 */
	snprintf(pathname, sizeof(pathname), "/proc/%d/cmdline", client->pid);
	procfile = fopen(pathname, "r");
	if (!procfile)
		goto fail;

	if (!fgets(friendly_name, sizeof(friendly_name), procfile))
		goto fail;

	start = strrchr(friendly_name, '/');
	if (!start)
		start = friendly_name;
	else
		start++;

	len = snprintf(client->trace.track_name,
		       sizeof(client->trace.track_name),
		       "%s (%s)", friendly_name, client->internal_name);
	if (len < 0 || (size_t)len >= sizeof(client->trace.track_name))
		goto fail;
	return;
fail:
	snprintf(client->trace.track_name, sizeof(client->trace.track_name),
		 "%s", client->internal_name);
}

void
weston_trace_flow_start(struct weston_trace_flow *flow)
{
	/* We just reset the flow to 0, and it will be assigned one next
	 * time it's touched.
	 */
	flow->id = 0;
}

void
weston_trace_flow_join(struct weston_trace_flow *target,
		       struct weston_trace_flow *flow)
{
	flow->id = target->id;
}

void
weston_trace_client_init(struct weston_client *client)
{
	struct weston_trace_flow creation_flow = { 0 };
	WESTON_TRACE_FUNC(("creation flow", &creation_flow));

	weston_trace_client_setup_name(client);

	client->trace.track.id = util_perfetto_new_track(client->trace.track_name);
	WESTON_TRACE_ANNOTATE(("client track", &client->trace.track),
			      ("creation flow", &creation_flow));
	WESTON_TRACE_COMMIT_ANNOTATION("connect");
}

void
weston_trace_client_fini(struct weston_client *client)
{
	struct weston_trace_flow destruction_flow = { 0 };
	WESTON_TRACE_FUNC(("destruction flow", &destruction_flow));

	WESTON_TRACE_ANNOTATE(("client track", &client->trace.track),
			      ("destruction flow", &destruction_flow));
	WESTON_TRACE_COMMIT_ANNOTATION("disconnect");

	weston_trace_track_clear(&client->trace.track);
}

struct weston_trace_flow
weston_trace_client_action(struct wl_client *wclient,
			   const char *action)
{
	struct weston_trace_flow client_flow = { 0 };
	struct weston_client *client = weston_compositor_get_client(NULL, wclient);

	WESTON_TRACE_BEGIN_ANNOTATION();
	WESTON_TRACE_ANNOTATE(("client track", &client->trace.track),
			      ("client flow", &client_flow));
	WESTON_TRACE_COMMIT_ANNOTATION(action);

	return client_flow;
}

void
weston_trace_compositor_init(struct weston_compositor *compositor)
{
	compositor->trace.repaint_track.id =
		util_perfetto_new_track("repaint");

	compositor->trace.timer_track.id =
		util_perfetto_new_nested_track("repaint timer",
					       compositor->trace.repaint_track.id);
}

void
weston_trace_compositor_fini(struct weston_compositor *compositor)
{
	weston_trace_track_clear(&compositor->trace.repaint_track);
	weston_trace_track_clear(&compositor->trace.timer_track);
}

void
weston_trace_output_init(struct weston_output *output)
{
	struct weston_compositor *compositor = output->compositor;

	output->trace.track.id =
		util_perfetto_new_nested_track(output->name,
					       compositor->trace.repaint_track.id);

	output->trace.gpu_track.id =
		util_perfetto_new_nested_track("GPU activity",
					       output->trace.track.id);

	output->trace.paint_track.id =
		util_perfetto_new_nested_track("paint",
						output->trace.track.id);

	output->trace.presentation_track.id =
		util_perfetto_new_nested_track("present",
					       output->trace.track.id);
}

void
weston_trace_output_fini(struct weston_output *output)
{
	weston_trace_track_clear(&output->trace.track);
	weston_trace_track_clear(&output->trace.gpu_track);
	weston_trace_track_clear(&output->trace.paint_track);
	weston_trace_track_clear(&output->trace.presentation_track);
}

void
weston_trace_surface_init(struct weston_surface *surface,
			  struct weston_client *client)
{
	/* We may have a NULL client if the surface is a shell curtain */
	if (client)
		surface->trace.client_track = client->trace.track;
	else
		surface->trace.client_track.id = util_perfetto_top_track();
}

void
weston_trace_surface_update(struct weston_surface *surface,
			    const char *new_label)
{
	struct weston_trace_surface *trace = &surface->trace;
	char track_name[600];

	if (surface->label && !new_label) {
		/* We're unmapping the surface, so take a hammer to any
		 * currently open events.
		 *
		 * Even if there aren't open events, it seems to be harmless
		 * to send a bogus end.
		 */
		util_perfetto_trace_end(trace->damage_track.id);
		util_perfetto_trace_end(trace->fifo_track.id);
		util_perfetto_trace_end(trace->real_track.id);
		util_perfetto_trace_end(trace->ideal_track.id);
		return;
	}

	if (!new_label)
		return;

	/* Same label that was in use already is a no-op for us. */
	if (surface->label && strcmp(surface->label, new_label) == 0)
		return;

	/* If we're just re-using the most recently used name, we've
	 * probably just unmapped and remapped. This happens all the
	 * time for cursors.
	 */
	if (trace->label && strcmp(trace->label, new_label) == 0)
		return;

	/* We can't change the name of a perfetto track in a useful way,
	 * so create a new one when we change the name.
	 */
	free(trace->label);
	trace->label = strdup(new_label);

	weston_trace_track_clear(&trace->damage_track);
	weston_trace_track_clear(&trace->fifo_track);
	weston_trace_track_clear(&trace->real_track);
	weston_trace_track_clear(&trace->ideal_track);

	snprintf(track_name, sizeof(track_name), "%s #%d",
		 new_label, surface->s_id);

	trace->damage_track.id =
		util_perfetto_new_nested_track(track_name,
					       trace->client_track.id);
	trace->fifo_track.id =
		util_perfetto_new_nested_track("FIFO barriers",
					       trace->damage_track.id);
	trace->real_track.id =
		util_perfetto_new_nested_track("Time:real",
					       trace->damage_track.id);
	trace->ideal_track.id =
		util_perfetto_new_nested_track("Time:ideal",
					       trace->damage_track.id);
}

void
weston_trace_surface_fini(struct weston_surface *surface)
{
	weston_trace_track_clear(&surface->trace.damage_track);
	weston_trace_track_clear(&surface->trace.fifo_track);
	weston_trace_track_clear(&surface->trace.real_track);
	weston_trace_track_clear(&surface->trace.ideal_track);
	free(surface->trace.label);
}

void
weston_trace_feedback_create(struct weston_surface *surface,
			     struct weston_surface_state *state)
{
	struct weston_presentation_feedback *feedback;
	struct weston_trace_presentation_feedback *trace;

	surface->trace.buffer_count++;
	surface->trace.buffer_count %= 10000;

	feedback = xzalloc(sizeof(*feedback));
	wl_list_init(&feedback->link);

	trace = &feedback->trace;

	trace->synthetic = true;
	trace->damage_track = surface->trace.damage_track;
	trace->real_track = surface->trace.real_track;
	trace->ideal_track = surface->trace.ideal_track;
	trace->update_time = state->update_time;
	trace->flow = surface->trace.flow;
	trace->buffer_number = surface->trace.buffer_count;

	WESTON_TRACE_BEGIN_ANNOTATION();
	WESTON_TRACE_ANNOTATE(("surface track", &trace->damage_track),
			      ("feedback flow", &trace->flow),
			      ("real flow", &trace->real_flow),
			      ("ideal flow", &trace->ideal_flow));
	WESTON_TRACE_COMMIT_ANNOTATION("prepare feedback request");

	wl_list_insert(&state->feedback_list, &feedback->link);
}

bool
weston_trace_feedback_discard(struct weston_presentation_feedback *feedback)
{
	struct weston_trace_presentation_feedback *trace = &feedback->trace;
	struct weston_trace_flow ideal_flow = { 0 };

	if (!feedback->trace.synthetic)
		return false;

	WESTON_TRACE_BEGIN_ANNOTATION();
	WESTON_TRACE_ANNOTATE(("surface track", &trace->damage_track),
			      ("feedback flow", &trace->flow),
			      ("ideal flow", &ideal_flow));

	/* If we have a time, use it, otherwise we'll log this event at
	 * the current time.
	 */
	if (trace->update_time.valid)
		WESTON_TRACE_ANNOTATE(("paint time", trace->update_time.time));

	WESTON_TRACE_COMMIT_ANNOTATION("discard");

	wl_list_remove(&feedback->link);
	free(feedback);

	return true;
}

bool
weston_trace_feedback_present(struct weston_presentation_feedback *feedback,
			      struct weston_output *output,
			      uint32_t refresh_nsec,
			      const struct timespec *ts,
			      uint64_t seq,
			      uint32_t flags)
{
	struct weston_trace_presentation_feedback *trace = &feedback->trace;
	uint64_t ideal_ns, real_ns;
	double time_error_ms;
	char buffer_count[20];

	if (!trace->synthetic)
		return false;

	real_ns = timespec_to_nsec(ts);

	if (trace->update_time.valid)
		ideal_ns = timespec_to_nsec(&trace->update_time.time);
	else
		ideal_ns = real_ns;

	snprintf(buffer_count, sizeof(buffer_count), "%" PRIu64, trace->buffer_number);
	WESTON_TRACE_TIMESTAMP_END(trace->real_track.id, CLOCK_MONOTONIC, real_ns);
	WESTON_TRACE_TIMESTAMP_BEGIN(buffer_count, trace->real_track.id, trace->real_flow.id, CLOCK_MONOTONIC, real_ns);

	WESTON_TRACE_TIMESTAMP_END(trace->ideal_track.id, CLOCK_MONOTONIC, ideal_ns);
	WESTON_TRACE_TIMESTAMP_BEGIN(buffer_count, trace->ideal_track.id, trace->ideal_flow.id, CLOCK_MONOTONIC, ideal_ns);

	time_error_ms = real_ns;
	time_error_ms -= ideal_ns;
	time_error_ms /= 1000000.0;
	WESTON_TRACE_SET_COUNTER(trace->damage_track, "Time:Error(ms)", time_error_ms);

	wl_list_remove(&feedback->link);
	free(feedback);

	return true;
}
