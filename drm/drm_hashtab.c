/**************************************************************************
 *
 * Copyright 2006 Tungsten Graphics, Inc., Bismarck, ND. USA.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 *
 **************************************************************************/
/*
 * Simple open hash tab implementation.
 *
 * Authors:
 * Thomas Hellström <thomas-at-tungstengraphics-dot-com>
 */

#include <drm/drmP.h>
#include <drm/drm_hashtab.h>
#ifdef __linux__
#include <linux/hash.h>
#endif
#include <linux/slab.h>
#include <linux/export.h>
#ifdef __FreeBSD__
#include <sys/hash.h>
#endif

int drm_ht_create(struct drm_open_hash *ht, unsigned int order)
{
#ifdef __linux__
	unsigned int size = 1 << order;
#elif __FreeBSD__
	ht->size = 1 << order;
#endif

	ht->order = order;
	ht->table = NULL;
#ifdef __linux__
	if (size <= PAGE_SIZE / sizeof(*ht->table))
		ht->table = kcalloc(size, sizeof(*ht->table), GFP_KERNEL);
	else
		ht->table = vzalloc(size*sizeof(*ht->table));
#elif __FreeBSD__
	ht->table = hashinit_flags(ht->size, DRM_MEM_HASHTAB, &ht->mask,
	    HASH_NOWAIT);
#endif
	if (!ht->table) {
		DRM_ERROR("Out of memory for hash table\n");
		return -ENOMEM;
	}
	return 0;
}
EXPORT_SYMBOL(drm_ht_create);

void drm_ht_verbose_list(struct drm_open_hash *ht, unsigned long key)
{
	struct drm_hash_item *entry;
#ifdef __linux__
	struct hlist_head *h_list;
	struct hlist_node *list;
#elif __FreeBSD__
	struct drm_hash_item_list *h_list;
#endif
	unsigned int hashed_key;
	int count = 0;

#ifdef __linux__
	hashed_key = hash_long(key, ht->order);
#elif __FreeBSD__
	hashed_key = hash32_buf(&key, sizeof(key), ht->order);
#endif
	DRM_DEBUG("Key is 0x%08lx, Hashed key is 0x%08x\n", key, hashed_key);
#ifdef __linux__
	h_list = &ht->table[hashed_key];
	hlist_for_each_entry(entry, list, h_list, head)
		DRM_DEBUG("count %d, key: 0x%08lx\n", count++, entry->key);
#elif __FreeBSD__
	h_list = &ht->table[hashed_key & ht->mask];
	LIST_FOREACH(entry, h_list, head)
		DRM_DEBUG("count %d, key: 0x%08lx\n", count++, entry->key);
#endif
}

#ifdef __linux__
static struct hlist_node *drm_ht_find_key(struct drm_open_hash *ht,
					  unsigned long key)
#elif __FreeBSD__
static struct drm_hash_item *drm_ht_find_key(struct drm_open_hash *ht,
					  unsigned long key)
#endif
{
	struct drm_hash_item *entry;
#ifdef __linux__
	struct hlist_head *h_list;
	struct hlist_node *list;
#elif __FreeBSD__
	struct drm_hash_item_list *h_list;
#endif
	unsigned int hashed_key;

#ifdef __linux__
	hashed_key = hash_long(key, ht->order);
	h_list = &ht->table[hashed_key];
	hlist_for_each_entry(entry, list, h_list, head) {
#elif __FreeBSD__
	hashed_key = hash32_buf(&key, sizeof(key), ht->order);
	h_list = &ht->table[hashed_key & ht->mask];
	LIST_FOREACH(entry, h_list, head) {
#endif
		if (entry->key == key)
#ifdef __linux__
			return list;
#elif __FreeBSD__
			return entry;
#endif
		if (entry->key > key)
			break;
	}
	return NULL;
}

#ifdef __linux__
static struct hlist_node *drm_ht_find_key_rcu(struct drm_open_hash *ht,
					      unsigned long key)
{
	struct drm_hash_item *entry;
	struct hlist_head *h_list;
	struct hlist_node *list;
	unsigned int hashed_key;

	hashed_key = hash_long(key, ht->order);
	h_list = &ht->table[hashed_key];
	hlist_for_each_entry_rcu(entry, list, h_list, head) {
		if (entry->key == key)
			return list;
		if (entry->key > key)
			break;
	}
	return NULL;
}
#endif

int drm_ht_insert_item(struct drm_open_hash *ht, struct drm_hash_item *item)
{
#ifdef __linux__
	struct drm_hash_item *entry;
	struct hlist_head *h_list;
	struct hlist_node *list, *parent;
#elif __FreeBSD__
	struct drm_hash_item *entry, *parent;
	struct drm_hash_item_list *h_list;
#endif
	unsigned int hashed_key;
	unsigned long key = item->key;

#ifdef __linux__
	hashed_key = hash_long(key, ht->order);
	h_list = &ht->table[hashed_key];
#elif __FreeBSD__
	hashed_key = hash32_buf(&key, sizeof(key), ht->order);
	h_list = &ht->table[hashed_key & ht->mask];
#endif
	parent = NULL;
#ifdef __linux__
	hlist_for_each_entry(entry, list, h_list, head) {
#elif __FreeBSD__
	LIST_FOREACH(entry, h_list, head) {
#endif
		if (entry->key == key)
			return -EINVAL;
		if (entry->key > key)
			break;
#ifdef __linux__
		parent = list;
#elif __FreeBSD__
		parent = entry;
#endif
	}
	if (parent) {
#ifdef __linux__
		hlist_add_after_rcu(parent, &item->head);
#elif __FreeBSD__
		LIST_INSERT_AFTER(parent, item, head);
#endif
	} else {
#ifdef __linux__
		hlist_add_head_rcu(&item->head, h_list);
#elif __FreeBSD__
		LIST_INSERT_HEAD(h_list, item, head);
#endif
	}
	return 0;
}
EXPORT_SYMBOL(drm_ht_insert_item);

/*
 * Just insert an item and return any "bits" bit key that hasn't been
 * used before.
 */
int drm_ht_just_insert_please(struct drm_open_hash *ht, struct drm_hash_item *item,
			      unsigned long seed, int bits, int shift,
			      unsigned long add)
{
	int ret;
	unsigned long mask = (1 << bits) - 1;
	unsigned long first, unshifted_key = 0;

#ifdef __linux__
	unshifted_key = hash_long(seed, bits);
#elif __FreeBSD__
	unshifted_key = hash32_buf(&seed, sizeof(seed), unshifted_key);
#endif
	first = unshifted_key;
	do {
		item->key = (unshifted_key << shift) + add;
		ret = drm_ht_insert_item(ht, item);
		if (ret)
			unshifted_key = (unshifted_key + 1) & mask;
	} while(ret && (unshifted_key != first));

	if (ret) {
		DRM_ERROR("Available key bit space exhausted\n");
		return -EINVAL;
	}
	return 0;
}
EXPORT_SYMBOL(drm_ht_just_insert_please);

int drm_ht_find_item(struct drm_open_hash *ht, unsigned long key,
		     struct drm_hash_item **item)
{
#ifdef __linux__
	struct hlist_node *list;
#elif __FreeBSD__
	struct drm_hash_item *list;
#endif

#ifdef __linux__
	list = drm_ht_find_key_rcu(ht, key);
#elif __FreeBSD__
	list = drm_ht_find_key(ht, key);
#endif
	if (!list)
		return -EINVAL;

#ifdef __linux__
	*item = hlist_entry(list, struct drm_hash_item, head);
#elif __FreeBSD__
	*item = list;
#endif
	return 0;
}
EXPORT_SYMBOL(drm_ht_find_item);

int drm_ht_remove_key(struct drm_open_hash *ht, unsigned long key)
{
#ifdef __linux__
	struct hlist_node *list;
#elif __FreeBSD__
	struct drm_hash_item *list;
#endif

	list = drm_ht_find_key(ht, key);
	if (list) {
#ifdef __linux__
		hlist_del_init_rcu(list);
#elif __FreeBSD__
		LIST_REMOVE(list, head);
#endif
		return 0;
	}
	return -EINVAL;
}

int drm_ht_remove_item(struct drm_open_hash *ht, struct drm_hash_item *item)
{
#ifdef __linux__
	hlist_del_init_rcu(&item->head);
#elif __FreeBSD__
	LIST_REMOVE(item, head);
#endif
	return 0;
}
EXPORT_SYMBOL(drm_ht_remove_item);

void drm_ht_remove(struct drm_open_hash *ht)
{
	if (ht->table) {
#ifdef __linux__
		if ((PAGE_SIZE / sizeof(*ht->table)) >> ht->order)
			kfree(ht->table);
		else
			vfree(ht->table);
#elif __FreeBSD__
		hashdestroy(ht->table, DRM_MEM_HASHTAB, ht->mask);
#endif
		ht->table = NULL;
	}
}
EXPORT_SYMBOL(drm_ht_remove);
