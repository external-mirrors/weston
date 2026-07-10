/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

/* This code was taken from the Mesa project, and heavily modified to
 * suit weston's needs.
 */

#ifndef WESTON_TRACE_H
#define WESTON_TRACE_H

#include "config.h"

#if defined(HAVE_PERFETTO)

#include "perfetto/annotations.h"
#include "perfetto/trace-helpers.h"
#include "perfetto/u_perfetto.h"
#include "shared/weston-assert.h"
#include <string.h>

#if !defined(HAVE___BUILTIN_EXPECT)
#  define __builtin_expect(x, y) (x)
#endif

#ifndef likely
#  ifdef HAVE___BUILTIN_EXPECT
#    define likely(x)   __builtin_expect(!!(x), 1)
#    define unlikely(x) __builtin_expect(!!(x), 0)
#  else
#    define likely(x)   (x)
#    define unlikely(x) (x)
#  endif
#endif

/* maximum allowed debug annotations */
#define WESTON_MAX_DEBUG_ANNOTS      128

/* maximum key length */
#define WESTON_TRACE_MAX_KEY_LENGTH  40

/* note that util_perfetto_is_tracing_enabled always returns false until
 * util_perfetto_init is called
 */
#define _WESTON_TRACE_IS_TRACING() unlikely(util_perfetto_is_tracing_enabled())

#define _WESTON_TRACE_BEGIN(name)                                             \
	do {                                                                  \
		if (_WESTON_TRACE_IS_TRACING())                               \
			util_perfetto_trace_begin(name);                      \
	} while (0)

#define _WESTON_TRACE_END(track_id)                                           \
	do {                                                                  \
		if (_WESTON_TRACE_IS_TRACING())                               \
			util_perfetto_trace_end(track_id);                    \
	} while (0)

#define _WESTON_TRACE_SET_COUNTER(name, value)                                \
	do {                                                                  \
		if (_WESTON_TRACE_IS_TRACING())                               \
			util_perfetto_counter_set(name, value);               \
	} while (0)

#define _WESTON_TRACE_TIMESTAMP_BEGIN(name, track_id, flow_id, clock, timestamp) \
	do {                                                                     \
		if (_WESTON_TRACE_IS_TRACING())                                  \
			util_perfetto_trace_full_begin(name, track_id, flow_id,  \
						       clock, timestamp);        \
	} while (0)

#define _WESTON_TRACE_TIMESTAMP_END(track_id, clock, timestamp)               \
	do {                                                                  \
		if (_WESTON_TRACE_IS_TRACING())                               \
			util_perfetto_trace_full_end(track_id, clock,         \
						     timestamp);              \
	} while (0)

#define _WESTON_TRACE_BEGIN_ANNOTATION()                                        \
	struct weston_debug_annotation __pd_annot[WESTON_MAX_DEBUG_ANNOTS];     \
	struct weston_debug_annotations __pd_annots = {                         \
		.annots = __pd_annot,                                           \
		.count = 0,                                                     \
		.track_id = 0,                                                  \
	}

#define _WESTON_TRACE_ANNOTATE_ADD_GENERIC(k, v)                                          \
		static_assert(sizeof(k) < WESTON_TRACE_MAX_KEY_LENGTH,                    \
			      "Key exceeds maximum length");                              \
		_Generic((v),                                                             \
			struct weston_trace_flow *: perfetto_annotate_flow,               \
			const struct weston_trace_flow *: perfetto_annotate_flow_const,   \
			int: perfetto_annotate_int,                                       \
			bool: perfetto_annotate_bool,                                     \
			unsigned int: perfetto_annotate_int,                              \
			float: perfetto_annotate_float,                                   \
			double: perfetto_annotate_double,                                 \
			char *: perfetto_annotate_string,                                 \
			const char *: perfetto_annotate_string,                           \
			weston_trace_time_since *: perfetto_annotate_time_since,          \
			struct weston_buffer *: perfetto_annotate_buffer,                 \
			const struct weston_buffer *: perfetto_annotate_buffer,           \
			struct weston_trace_track *: perfetto_annotate_track,             \
			const struct weston_trace_track *: perfetto_annotate_track        \
		) (&__pd_annots, k, sizeof(k), v);

#define _WESTON_TRACE_ANNOTATE_ADD(k, v)                  \
	do {                                              \
		_WESTON_TRACE_ANNOTATE_ADD_GENERIC(k, v); \
	} while (0)

#define _WESTON_TRACE_COMMIT_ANNOTATION_DO(name)                                                        \
	do {                                                                                            \
		_weston_trace_scope_annotate_commit(name, &__pd_annots);                                \
	} while (0)

/* Allow either 1 or 0 parameters, if 0 use __func__. This works by
 * _WESTON_TRACE_COMMIT_ANNOTATION evaluating to:
 * _PICK_HELPER(, _WESTON_TRACE_COMMIT_ANNOTATION_1, WESTON_TRACE_COMMIT_ANNOTATION_0)(...)
 * when there are no arguments, or
 * _PICK_HELPER(_WESTON_TRACE_COMMIT_ANNOTATION_1, WESTON_TRACE_COMMIT_ANNOTATION_0)(...)
 * When there are arguments. _PICK_HELPER then evaluates to the second argument, and it
 * gets handed the (...)
 * _WESTON_TRACE_COMMIT_ANNOTATION_1(...) or _WESTON_TRACE_COMMIT_ANNOTATION_0()
 */
#define _WESTON_TRACE_COMMIT_ANNOTATION_1(name) _WESTON_TRACE_COMMIT_ANNOTATION_DO(name)
#define _WESTON_TRACE_COMMIT_ANNOTATION_0() _WESTON_TRACE_COMMIT_ANNOTATION_DO(__func__)
#define _PICK_HELPER(_discard, FINAL_MACRO, ...) FINAL_MACRO
#define _WESTON_TRACE_COMMIT_ANNOTATION(...)                         \
	_PICK_HELPER(__VA_OPT__(,)                                   \
		     _WESTON_TRACE_COMMIT_ANNOTATION_1,              \
		     _WESTON_TRACE_COMMIT_ANNOTATION_0)(__VA_ARGS__)

/* annotated funcs */
#define _WESTON_TRACE_ANNOTATE_FUNC_BEGIN(name, annots)                                                 \
	do {                                                                                            \
		if (_WESTON_TRACE_IS_TRACING()) {                                                       \
			util_perfetto_trace_commit_annotate_func(name, annots);                         \
		}                                                                                       \
	} while (0)

#define _WESTON_TRACE_ANNOTATE(...) \
	do {                                                                \
		_WESTON_TRACE_EXPAND(_WESTON_TRACE_ITER_HELPER(             \
			_WESTON_TRACE_ANNOTATE_PAIR, __VA_ARGS__))          \
	} while (0)

#define _WESTON_TRACE_INIT() util_perfetto_init()

/* Helpers macros for recursive variadic expansion, never to
 * be used outside of this header.
 */
#define _WESTON_TRACE_EXPAND(arg)                          \
	_WESTON_TRACE_EXPAND1(_WESTON_TRACE_EXPAND1(       \
	_WESTON_TRACE_EXPAND1(_WESTON_TRACE_EXPAND1(arg))))
#define _WESTON_TRACE_EXPAND1(arg)                         \
	_WESTON_TRACE_EXPAND2(_WESTON_TRACE_EXPAND2(       \
	_WESTON_TRACE_EXPAND2(_WESTON_TRACE_EXPAND2(arg))))
#define _WESTON_TRACE_EXPAND2(arg) arg

#define _WESTON_TRACE_ITER_HELPER(operation, pair, ...)                     \
	operation(pair)                                                     \
	__VA_OPT__(_WESTON_TRACE_ITER_AGAIN                                 \
		   _WESTON_TRACE_FORCE_RECURSE (operation, __VA_ARGS__))
#define _WESTON_TRACE_ITER_AGAIN() _WESTON_TRACE_ITER_HELPER

#define _WESTON_TRACE_FORCE_RECURSE ()
#define _WESTON_TRACE_ANNOTATE_PAIR(pair) _WESTON_TRACE_ANNOTATE_ADD_GENERIC pair

/* end of helper section */
#define _WESTON_TRACE_INSTANT_TIMESTAMP(name, track_id, id, clock, timestamp) \
	do {                                                                  \
		if (WESTON_TRACE_IS_TRACING())                                \
			_weston_trace_instant_timestamp(name, track_id, id,   \
							clock, timestamp);    \
	} while (0)

#if __has_attribute(cleanup) && __has_attribute(unused)

#define _WESTON_TRACE_SCOPE_VAR_CONCAT(name, suffix) name##suffix
#define _WESTON_TRACE_SCOPE_VAR(suffix)                                       \
	_WESTON_TRACE_SCOPE_VAR_CONCAT(_weston_trace_scope_, suffix)

/* This must expand to a single non-scoped statement for
 *
 *    if (cond)
 *       _WESTON_TRACE_SCOPE(...)
 *
 * to work.
 */
#define _WESTON_TRACE_SCOPE(name)                                             \
	uint64_t _WESTON_TRACE_SCOPE_VAR(__LINE__)                            \
		__attribute__((cleanup(_weston_trace_scope_end))) =           \
			_weston_trace_annotate_func_begin(name, &__pd_annots)

static inline void
_weston_trace_scope_annotate_commit(const char *name,
				    struct weston_debug_annotations *annots)
{
	util_perfetto_trace_commit_debug_annots(name, annots);

	annots->count = 0;
	annots->track_id = 0;
}

static inline uint64_t
_weston_trace_annotate_func_begin(const char *name,
				  struct weston_debug_annotations *annots)
{
	uint64_t track_id = annots->track_id;

	_WESTON_TRACE_ANNOTATE_FUNC_BEGIN(name, annots);

	annots->count = 0;
	annots->track_id = 0;
	return track_id;
}

static inline int
_weston_trace_instant_timestamp(const char *name, uint64_t track_id, uint64_t id,
				clock_t clock, uint64_t ts)
{
	weston_assert_u64_gt(NULL, id, 0);
	util_perfetto_trace_instant_timestamp(name, track_id, id, clock, ts);
	return 0;
}

static inline void
_weston_trace_scope_end(uint64_t *scope)
{
	_WESTON_TRACE_END(*scope);
}

#else

#define _WESTON_TRACE_SCOPE(name)

#endif /* __has_attribute(cleanup) && __has_attribute(unused) */

#define _WESTON_TRACE_CLIENT_INIT(client) weston_trace_client_init(client)
#define _WESTON_TRACE_CLIENT_FINI(client) weston_trace_client_fini(client)

#define _WESTON_TRACE_OUTPUT_INIT(output) weston_trace_output_init(output)
#define _WESTON_TRACE_OUTPUT_FINI(output) weston_trace_output_fini(output)

#define _WESTON_TRACE_SURFACE_INIT(surface, client) weston_trace_surface_init(surface, client)
#define _WESTON_TRACE_SURFACE_UPDATE(surface, label) weston_trace_surface_update(surface, label)
#define _WESTON_TRACE_SURFACE_FINI(surface) weston_trace_surface_fini(surface)

#else /* No perfetto, make these all do nothing */

#define _WESTON_TRACE_SCOPE(name)
#define _WESTON_TRACE_FUNC()
#define _WESTON_TRACE_SET_COUNTER(name, value)
#define _WESTON_TRACE_TIMESTAMP_BEGIN(name, track_id, flow_id, clock, timestamp)
#define _WESTON_TRACE_TIMESTAMP_END(track_id, clock, timestamp)
#define _WESTON_TRACE_INSTANT_TIMESTAMP(name, track_id, id, clock, timestamp)

#define _WESTON_TRACE_BEGIN_ANNOTATION()
#define _WESTON_TRACE_COMMIT_ANNOTATION(name)
#define _WESTON_TRACE_ANNOTATE(...)

#define _WESTON_TRACE_INIT()
#define _WESTON_TRACE_IS_TRACING() (false)

#define _WESTON_TRACE_CLIENT_INIT(client)
#define _WESTON_TRACE_CLIENT_FINI(client)

#define _WESTON_TRACE_OUTPUT_INIT(output)
#define _WESTON_TRACE_OUTPUT_FINI(output)

#define _WESTON_TRACE_SURFACE_INIT(surface, client)
#define _WESTON_TRACE_SURFACE_UPDATE(surface, label)
#define _WESTON_TRACE_SURFACE_FINI(surface)

#endif /* HAVE_PERFETTO */

#define WESTON_TRACE_SCOPE(name) _WESTON_TRACE_SCOPE(name)
#define WESTON_TRACE_SET_COUNTER(name, value) _WESTON_TRACE_SET_COUNTER(name, value)
#define WESTON_TRACE_TIMESTAMP_BEGIN(name, track_id, flow_id, clock, timestamp) \
	_WESTON_TRACE_TIMESTAMP_BEGIN(name, track_id, flow_id, clock, timestamp)
#define WESTON_TRACE_TIMESTAMP_END(track_id, clock, timestamp) \
	_WESTON_TRACE_TIMESTAMP_END(track_id, clock, timestamp)
#define WESTON_TRACE_INSTANT_TIMESTAMP(name, track_id, id, clock, timestamp) \
	_WESTON_TRACE_INSTANT_TIMESTAMP(name, track_id, id, clock, timestamp)


#define WESTON_TRACE_BEGIN_ANNOTATION() \
        _WESTON_TRACE_BEGIN_ANNOTATION()

#define WESTON_TRACE_COMMIT_ANNOTATION(name) \
        _WESTON_TRACE_COMMIT_ANNOTATION(name)

#define WESTON_TRACE_FUNC(...)                          \
	WESTON_TRACE_BEGIN_ANNOTATION();                \
	__VA_OPT__(WESTON_TRACE_ANNOTATE(__VA_ARGS__)); \
        _WESTON_TRACE_SCOPE(__func__)

/* Adds a series of annotations of the form '("key string", value)' separated
 * by commas.
 *
 * Brackets are necessary, and the value can be any type understood by the
 * _Generic block in _WESTON_TRACE_ANNOTATE_ADD
 */
#define WESTON_TRACE_ANNOTATE(...)                                          \
	_WESTON_TRACE_ANNOTATE(__VA_ARGS__)

#define WESTON_TRACE_INIT() _WESTON_TRACE_INIT()
#define WESTON_TRACE_IS_TRACING() _WESTON_TRACE_IS_TRACING()

#define WESTON_TRACE_CLIENT_INIT(client) _WESTON_TRACE_CLIENT_INIT(client)
#define WESTON_TRACE_CLIENT_FINI(client) _WESTON_TRACE_CLIENT_FINI(client)

#define WESTON_TRACE_OUTPUT_INIT(output) _WESTON_TRACE_OUTPUT_INIT(output)
#define WESTON_TRACE_OUTPUT_FINI(output) _WESTON_TRACE_OUTPUT_FINI(output)

#define WESTON_TRACE_SURFACE_INIT(surface, client) _WESTON_TRACE_SURFACE_INIT(surface, client)
#define WESTON_TRACE_SURFACE_UPDATE(surface, label) _WESTON_TRACE_SURFACE_UPDATE(surface, label)
#define WESTON_TRACE_SURFACE_FINI(surface) _WESTON_TRACE_SURFACE_FINI(surface)

#endif /* WESTON_TRACE_H */
