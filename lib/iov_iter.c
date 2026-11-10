#include <linux/uio.h>
#include <errno.h>
#include <string.h>

int import_ubuf(const void *buf, size_t len, struct iov_iter *iter)
{
	if (len && !buf)
		return -EFAULT;
	/* Beskriver minnet; kopieringen sker senare i TCP. Ingen MM-validering. */
	*iter = (struct iov_iter) { .ubuf = buf, .count = len };
	return 0;
}

size_t copy_from_iter(void *to, size_t len, struct iov_iter *iter)
{
	if (len > iter->count)
		len = iter->count;
	if (len) {
		memcpy(to, iter->ubuf, len);
		iter->ubuf += len;
		iter->count -= len;
	}
	return len;
}
