#ifndef TUTORIAL_LINUX_UIO_H
#define TUTORIAL_LINUX_UIO_H

#include <linux/types.h>

/* Bara en sammanhängande sändningsbuffer, inte riktiga iovec-listor. */
struct iov_iter {
	const u8 *ubuf;
	size_t count;
};

int import_ubuf(const void *buf, size_t len, struct iov_iter *iter);
size_t copy_from_iter(void *to, size_t len, struct iov_iter *iter);

#endif
