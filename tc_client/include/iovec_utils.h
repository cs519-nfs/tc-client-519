/**
 * Copyright (C) Stony Brook University 2016
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */


#ifndef __TC_IOVEC_UTILS_H__
#define __TC_IOVEC_UTILS_H__

#include "tc_api.h"

#ifdef __cplusplus
extern "C" {
#endif

const size_t TC_SPLIT_THRESHOLD = 4096;

/**
 * Split an array of viovec specified by "iova" to multiple arrays of
 * viovec so that the total size of viovecs in each array is no larger than
 * size_limit.  The caller own the returned array of viov_array, and is
 * responsible for freeing them by calling vrestore_iov_array().
 *
 * @iova: the input viov_array to be split
 * @size_limit: size limit of each resultant viov_array
 * @nparts: the number of viov_arrays "iova" is split into
 * Returns an array of viov_array.
 *
 */
struct viov_array *tc_split_iov_array(const struct viov_array *iova,
					int size_limit, int *nparts);

/**
 * Update the original "iova" from the results of its split parts.
 *
 * It also release the memory allocated for "parts".
 *
 * Return success or failure.
 */
bool vrestore_iov_array(struct viov_array *iova,
			  struct viov_array **parts, int nparts);

bool tc_merge_iov_array(struct viov_array *iova);

#ifdef __cplusplus
}
#endif

#endif  // __TC_IOVEC_UTILS_H__
