/* Linux/glibc-only measurement helper. Enable only around a measured operation
 * on the calling thread, so event-loop/reporting work is not counted. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <libavcodec/avcodec.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

/* The application hides symbols by default; interposition and dlsym require
 * these helper entry points to remain visible. */
#pragma GCC visibility push(default)

extern void *__libc_malloc(size_t);
extern void *__libc_calloc(size_t, size_t);
extern void *__libc_realloc(void *, size_t);
extern void *__libc_memalign(size_t, size_t);

static _Thread_local int measuring;
static _Thread_local uint64_t calls, bytes, large_calls, large_bytes;
static _Thread_local uint64_t encode_ns, decode_ns;

void video_allocations_begin(void) {
	calls = bytes = large_calls = large_bytes = 0;
	encode_ns = decode_ns = 0;
	measuring             = 1;
}

void video_allocations_end(uint64_t *result) {
	measuring = 0;
	result[0] = calls;
	result[1] = bytes;
	result[2] = large_calls;
	result[3] = large_bytes;
	result[4] = encode_ns;
	result[5] = decode_ns;
}

static uint64_t now(void) {
	struct timespec value;
	clock_gettime(CLOCK_MONOTONIC, &value);
	return (uint64_t) value.tv_sec * 1000000000 + (uint64_t) value.tv_nsec;
}

/* Time the actual production codec calls, without changing capture/receiver
 * interfaces or including screen grabs, conversions, packetization or hashing. */
#define WRAP_CODEC(name, data_type, counter)                                           \
	int name(AVCodecContext *context, data_type data) {                                \
		static int (*original)(AVCodecContext *, data_type);                           \
		if (!original) {                                                               \
			original = (int (*)(AVCodecContext *, data_type)) dlsym(RTLD_NEXT, #name); \
		}                                                                              \
		if (!measuring) {                                                              \
			return original(context, data);                                            \
		}                                                                              \
		const uint64_t started = now();                                                \
		const int result       = original(context, data);                              \
		counter += now() - started;                                                    \
		return result;                                                                 \
	}

WRAP_CODEC(avcodec_send_frame, const AVFrame *, encode_ns)
WRAP_CODEC(avcodec_receive_packet, AVPacket *, encode_ns)
WRAP_CODEC(avcodec_send_packet, const AVPacket *, decode_ns)
WRAP_CODEC(avcodec_receive_frame, AVFrame *, decode_ns)

static void count(size_t size) {
	if (measuring) {
		++calls;
		bytes += size;
		if (size >= 1024 * 1024) {
			++large_calls;
			large_bytes += size;
		}
	}
}

void *malloc(size_t size) {
	count(size);
	return __libc_malloc(size);
}

void *calloc(size_t n, size_t size) {
	count(n * size);
	return __libc_calloc(n, size);
}

void *realloc(void *pointer, size_t size) {
	count(size);
	return __libc_realloc(pointer, size);
}

void *aligned_alloc(size_t alignment, size_t size) {
	count(size);
	return __libc_memalign(alignment, size);
}

int posix_memalign(void **pointer, size_t alignment, size_t size) {
	if (alignment < sizeof(void *) || (alignment & (alignment - 1))) {
		return EINVAL;
	}
	count(size);
	void *allocation = __libc_memalign(alignment, size);
	if (!allocation) {
		return ENOMEM;
	}
	*pointer = allocation;
	return 0;
}

#pragma GCC visibility pop
