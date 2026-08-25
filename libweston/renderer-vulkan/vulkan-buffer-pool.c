/*
 * Copyright © 2026 Erico Nunes
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

#include <vulkan/vulkan.h>
#include <stdint.h>

#include "shared/helpers.h"
#include "shared/xalloc.h"
#include "vulkan-renderer-internal.h"

/*
 * Allocates new buffers on demand or reuse current buffers if
 * there is still space available.
 */
struct vulkan_buffer_pool_entry *
vulkan_buffer_pool_alloc(struct vulkan_renderer *vr,
			 struct vulkan_buffer_pool *pool,
			 uint64_t size)
{
	const uint32_t base_size = 4096;
	VkResult result;

	if (!wl_list_empty(&pool->entry_list)) {
		struct vulkan_buffer_pool_entry *first = wl_container_of(pool->entry_list.next, first, link);
		if (first->size >= first->offset + size)
			return first;
	}

	struct vulkan_buffer_pool_entry *entry = xzalloc(sizeof(*entry));

	VkDeviceSize buffer_size = MAX(base_size, round_up_pow2_32(size));
	entry->size = buffer_size;
	entry->offset = 0;

	vulkan_create_buffer(vr, entry->size,
			     VK_BUFFER_USAGE_TRANSFER_DST_BIT | pool->usage,
			     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			     &entry->buffer, &entry->memory);

	result = vkMapMemory(vr->dev, entry->memory, 0, VK_WHOLE_SIZE, 0, &entry->map);
	check_vk_success(result, "vkMapMemory");

	wl_list_insert(&pool->entry_list, &entry->link);

	return entry;
}

/*
 * Resets pool buffer offset so it can be reused, or coalesces multiple
 * entries into a single larger new one if multiple were dynamically
 * allocated in the previous use of this pool
 */
void
vulkan_buffer_pool_reset(struct vulkan_renderer *vr,
			 struct vulkan_buffer_pool *pool)
{
	if (wl_list_empty(&pool->entry_list))
		return;

	if (wl_list_length(&pool->entry_list) == 1) {
		struct vulkan_buffer_pool_entry *first = wl_container_of(pool->entry_list.next, first, link);
		first->offset = 0;
		return;
	}

	struct vulkan_buffer_pool_entry *entry, *tmp;
	uint64_t total_size = 0;
	wl_list_for_each_safe(entry, tmp, &pool->entry_list, link) {
		total_size += entry->size;
		wl_list_remove(&entry->link);
		vulkan_destroy_buffer(vr->dev, entry->buffer, entry->memory);
		free(entry);
	}

	total_size = round_up_pow2_32(total_size);

	vulkan_buffer_pool_alloc(vr, pool, total_size);
	struct vulkan_buffer_pool_entry *first = wl_container_of(pool->entry_list.next, first, link);
	first->offset = 0;
}

void
vulkan_buffer_pool_destroy(struct vulkan_renderer *vr,
			   struct vulkan_buffer_pool *pool)
{
	struct vulkan_buffer_pool_entry *entry, *tmp;
	wl_list_for_each_safe(entry, tmp, &pool->entry_list, link) {
		vulkan_destroy_buffer(vr->dev, entry->buffer, entry->memory);
		wl_list_remove(&entry->link);
		free(entry);
	}
}
