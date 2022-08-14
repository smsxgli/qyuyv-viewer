#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <assert.h>
#include <bits/types/struct_timeval.h>
#include <errno.h>
#include <fcntl.h>
#include <libv4l2.h>
#include <linux/videodev2.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <unistd.h>

struct buffer {
  void *start;
  size_t length;
};

struct yuv_dev {
  struct buffer *buffers;
  size_t n_buffers;
  int fd;
  __u32 width, height, pixel_format;
};

extern int yuv_dev_create(void **);
extern void yuv_dev_destroy(void *);
extern int yuv_start_capture(void *);
extern int yuv_stop_capture(void *);
// extern int yuv_loop(void *, int (*)(void *), int (*)(void *, size_t, void *),
//                     void *);
extern int yuv_loop(void *, int (*)(void *),
                    int (*)(void *, size_t, __u32, __u32, void *), void *);

#define __unlikely(_x) __builtin_expect(!!(_x), 0)
#define __likely(_x) __builtin_expect(!!(_x), 1)
#define __cl(_f) __attribute__((__cleanup__(_f)))
#define ptr_move(_ptr)                                                         \
  __extension__({                                                              \
    __auto_type _tmp = (_ptr);                                                 \
    (_ptr) = NULL;                                                             \
    _tmp;                                                                      \
  })
#define fd_move(_fd)                                                           \
  __extension__({                                                              \
    __auto_type _tmp = (_fd);                                                  \
    (_fd) = -1;                                                                \
    _tmp;                                                                      \
  })
#define V4L2_DEV_FILE "/dev/video0"

static inline void fd_cleanup(int *fd) {
  // printf("fd_cleanup! %d\n", *fd);
  if (*fd >= 0) {
    int r = close(*fd);
    if (r) {
      perror("Failed at fd_cleanup");
    }
  }
}

static inline void ptr_cleanup(void *pptr) {
  void *ptr = *(void **)pptr;
  if (ptr) {
    free(ptr);
  }
}

static int xioctl(int fd, unsigned long request, void *arg) {
  int r;
  do {
    r = ioctl(fd, request, arg);
  } while (-1 == r && EINTR == errno);

  return r;
}

int yuv_dev_create(void **ph) {
  if (__unlikely(NULL == ph)) {
    return EINVAL;
  }
  __cl(fd_cleanup) int fd = open(V4L2_DEV_FILE, O_RDWR | O_CLOEXEC);
  if (__unlikely(fd < 0)) {
    perror("Failed at open file " V4L2_DEV_FILE);
    return errno;
  }

  struct v4l2_capability cap;
  if (__unlikely(-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap))) {
    perror("Failed at ioctl(), VIDIOC_QUERYCAP");
    return errno;
  } else {
    printf("%s%s%s%s%s%s%s%d%s%x\n", V4L2_DEV_FILE " info:\n\tdriver:\t",
           cap.driver, "\n\tcard:\t", cap.card, "\n\tbus info:\t", cap.bus_info,
           "\n\tversion:\t", cap.version, "\n\tcapability:\t",
           cap.capabilities);
  }
  if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
    fprintf(stderr, V4L2_DEV_FILE " is not a video capture device!\n");
    return ENOSYS;
  }
  if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
    fprintf(stderr, V4L2_DEV_FILE "is not a video streaming device!\n");
    return ENOSYS;
  }

  struct v4l2_fmtdesc fmt_desc = {0};
  fmt_desc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  __u32 max_width = 0;
  __u32 max_height = 0;
  __u32 max_pixelformat;
  printf("%s", V4L2_DEV_FILE " enum resolution(s):\n");
  while (0 == xioctl(fd, VIDIOC_ENUM_FMT, &fmt_desc)) {
    printf("%s%s\n", "\tdescription:\t", fmt_desc.description);
    struct v4l2_frmsizeenum frm_size = {0};
    frm_size.pixel_format = fmt_desc.pixelformat;
    while (0 == xioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frm_size)) {
      if (V4L2_FRMSIZE_TYPE_DISCRETE == frm_size.type) {
        printf("%s%u%s%u\n", "\t\twidth:\t", frm_size.discrete.width,
               "\n\t\theight:\t", frm_size.discrete.height);
        if (frm_size.discrete.width >= max_width) {
          max_width = frm_size.discrete.width;
          max_height = frm_size.discrete.height;
          max_pixelformat = frm_size.pixel_format;
        }
        frm_size.index++;
      } else if (V4L2_FRMSIZE_TYPE_STEPWISE == frm_size.type ||
                 V4L2_FRMSIZE_TYPE_CONTINUOUS == frm_size.type) {
        printf("%s%u%s%u%s%u%s%u%s%u%s%u\n", "\t\tmin_width:\t",
               frm_size.stepwise.min_width, "\n\t\tmin_height:\t",
               frm_size.stepwise.min_height, "\n\t\tmax_width:\t",
               frm_size.stepwise.max_width, "\n\t\tmax_height:\t",
               frm_size.stepwise.max_height, "\n\t\tstep_width:\t",
               frm_size.stepwise.step_width, "\n\t\tstep_height:\t",
               frm_size.stepwise.step_height);
        if (frm_size.stepwise.max_width >= max_width) {
          max_width = frm_size.stepwise.max_width;
          max_height = frm_size.stepwise.max_height;
          max_pixelformat = frm_size.pixel_format;
        }
        break;
      }
    }
    fmt_desc.index++;
  }
  if (__unlikely(0 == max_width)) {
    fprintf(stderr, "Failed to query max resolution, set to default.\n");
    max_width = 640;
    max_height = 480;
    max_pixelformat = V4L2_PIX_FMT_YUYV;
  }
  assert(V4L2_PIX_FMT_YUYV == max_pixelformat);
  // __auto_type PIX_FMT = max_pixelformat;
  // const char *cptr = (const char *)&PIX_FMT;
  // printf("max pixel format:\n\t");
  // for (size_t i = 0; i < sizeof(PIX_FMT); i++) {
  //   printf("%c", cptr[i]);
  // }
  // printf("\n");

  struct v4l2_cropcap cropcap = {0};
  struct v4l2_crop crop = {0};
  cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (0 == xioctl(fd, VIDIOC_CROPCAP, &cropcap)) {
    crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    crop.c = cropcap.defrect;                   /* reset to default */
    (void)xioctl(fd, VIDIOC_CROPCAP, &cropcap); /* ignore error */
  }

  struct v4l2_format fmt = {0};
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width = max_width;
  fmt.fmt.pix.height = max_height;
  fmt.fmt.pix.pixelformat = max_pixelformat;
  fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
  if (__unlikely(-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))) {
    perror("Failed at ioctl(), VIDIOC_S_FMT\n");
    return errno;
  }
  /* Buggy driver paranoia. */
  __auto_type min = fmt.fmt.pix.width * 2;
  if (fmt.fmt.pix.bytesperline < min) {
    fmt.fmt.pix.bytesperline = min;
  }
  min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
  if (fmt.fmt.pix.sizeimage < min) {
    fmt.fmt.pix.sizeimage = min;
  }

  /* init mmap */
  struct v4l2_requestbuffers req = {0};
  req.count = 4;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;

  if (__unlikely(-1 == xioctl(fd, VIDIOC_REQBUFS, &req))) {
    if (EINVAL == errno) {
      fprintf(stderr, "%s", V4L2_DEV_FILE " does not support mmap!\n");
    } else {
      perror("Failed at ioctl, VIDIOC_REQBUFS");
    }

    return errno;
  }

  if (__unlikely(req.count < 2)) {
    fprintf(stderr, "Insufficient buffer memory on " V4L2_DEV_FILE);

    return ENOBUFS;
  }

  __cl(ptr_cleanup) struct buffer *buffers =
      calloc(req.count, sizeof(*buffers));
  if (__unlikely(NULL == buffers)) {
    fprintf(stderr, "OOM!\n");

    return ENOMEM;
  }

  for (__typeof__(req.count) i = 0; i < req.count; i++) {
    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;

    if (__unlikely(-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))) {
      perror("Failed at ioctl, VIDIOC_QUERYBUF");

      return errno;
    }

    buffers[i].length = buf.length;
    buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                            MAP_SHARED, fd, buf.m.offset);
    if (__unlikely(MAP_FAILED == buffers[i].start)) {
      fprintf(stderr, "%s%u\n", "Failed at mmap, idx: ", i);

      return EIO;
    }
  }

  __cl(ptr_cleanup) struct yuv_dev *dev = malloc(sizeof(*dev));
  if (__unlikely(NULL == dev)) {
    fprintf(stderr, "OOM!\n");

    return ENOMEM;
  }
  dev->buffers = buffers;
  buffers = NULL;
  dev->n_buffers = req.count;
  dev->fd = fd;
  fd = -1;
  dev->width = max_width;
  dev->height = max_height;
  dev->pixel_format = max_pixelformat;
  *ph = dev;
  dev = NULL;

  return 0;
}

void yuv_dev_destroy(void *ph) {
  if (__unlikely(NULL == ph)) {
    return;
  }
  struct yuv_dev *dev = ph;
  for (size_t i = 0; i < dev->n_buffers; i++) {
    if (__unlikely(-1 ==
                   munmap(dev->buffers[i].start, dev->buffers[i].length))) {
      perror("Failed at munmap");

      return;
    }
  }
  free(dev->buffers);
  dev->buffers = NULL;
  dev->n_buffers = 0;
  if (__unlikely(-1 == close(dev->fd))) {
    perror("Failed at close");

    return;
  }
  dev->fd = -1;
}

int yuv_start_capture(void *ph) {
  if (__unlikely(NULL == ph)) {
    return EINVAL;
  }
  struct yuv_dev *dev = ph;
  for (size_t i = 0; i < dev->n_buffers; i++) {
    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;

    if (__unlikely(-1 == xioctl(dev->fd, VIDIOC_QBUF, &buf))) {
      perror("Failed at ioctl, VIDIOC_QBUF");

      return errno;
    }
  }
  __auto_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (__unlikely(-1 == xioctl(dev->fd, VIDIOC_STREAMON, &type))) {
    perror("Failed at ioctl, VIDIOC_STREAMON");

    return errno;
  }

  return 0;
}

int yuv_stop_capture(void *ph) {
  if (__unlikely(NULL == ph)) {
    return EINVAL;
  }
  struct yuv_dev *dev = ph;
  __auto_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (__unlikely(-1 == xioctl(dev->fd, VIDIOC_STREAMOFF, &type))) {
    perror("Failed at ioctl, VIDIOC_STREAMOFF");

    return errno;
  }

  return 0;
}

int yuv_loop(void *ph, int (*key)(void *),
             int (*cb)(void *, size_t, __u32, __u32, void *), void *priv) {
  if (__unlikely(NULL == ph)) {
    return EINVAL;
  }
  struct yuv_dev *dev = ph;
  while (key(priv)) {
    for (;;) {
      fd_set fds;
      FD_ZERO(&fds);
      FD_SET(dev->fd, &fds);

      struct timeval tv = {.tv_sec = 2};

      int r = select(dev->fd + 1, &fds, NULL, NULL, &tv);
      if (__unlikely(-1 == r)) {
        if (EINTR == errno) {
          continue;
        } else {
          perror("Failed at select");

          return errno;
        }
      } else if (__unlikely(0 == r)) {
        fprintf(stderr, "select timeout!\n");

        return ETIMEDOUT;
      } else {
        struct v4l2_buffer buf = {0};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (__unlikely(-1 == xioctl(dev->fd, VIDIOC_DQBUF, &buf))) {
          if (EAGAIN == errno) {
            continue;
          } else {
            perror("Failed at ioctrl, VIDIOC_DQBUF");

            return errno;
          }
        }

        assert(dev->n_buffers > buf.index);
        /* TODO: translate YUVY format to packed yuv444 */
        char *yuv444_start;
        size_t yuv444_length;
        {
          __cl(ptr_cleanup) char *yuv_buf =
              malloc((size_t)(dev->width * dev->height) * 3);
          if (NULL == yuv_buf) {
            fprintf(stderr, "OOM!\n");
            return ENOMEM;
          }

          struct yuv_pack {
            uint8_t y, u, v;
          } __attribute__((__packed__));
          struct yuv_pack *pack = (struct yuv_pack *)yuv_buf;
          struct yuyv_pack {
            uint8_t y, u;
          } __attribute__((__packed__));
          struct yuyv_pack *src_pack = dev->buffers[buf.index].start;

          for (__u32 i = 0; i < dev->height; i++) {
            for (__u32 j = 0; j < dev->width; j += 2) {
              pack[i * dev->width + j].y = src_pack[i * dev->width + j].y;
              pack[i * dev->width + j].u = src_pack[i * dev->width + j].u;
              pack[i * dev->width + j].v =
                  src_pack[i * dev->width + j + 1]
                      .u; /* this will not out of bound */
            }
            for (__u32 j = 1; j < dev->width - 1; j++) {
              pack[i * dev->width + j].y = src_pack[i * dev->width + j].y;
              pack[i * dev->width + j].u = (pack[i * dev->width + j - 1].u +
                                            pack[i * dev->width + j + 1].u) /
                                           2;
              pack[i * dev->width + j].v = (pack[i * dev->width + j - 1].v +
                                            pack[i * dev->width + j + 1].v) /
                                           2;
            }
            pack[i * dev->width + dev->width - 1].y =
                src_pack[i * dev->width + dev->width - 1].y;
            pack[i * dev->width + dev->width - 1].u =
                pack[i * dev->width + dev->width - 2].u;
            pack[i * dev->width + dev->width - 1].v =
                pack[i * dev->width + dev->width - 2].v;
          }

          yuv444_start = ptr_move(yuv_buf);
          yuv444_length = (size_t)(dev->width * dev->height) * 3;
        }
        // int ur = cb(dev->buffers[buf.index].start, buf.bytesused, dev->width,
        //             dev->height, priv);
        int ur = cb(yuv444_start, yuv444_length, dev->width, dev->height, priv);
        if (__unlikely(ur)) {
          fprintf(stderr, "Failed at user cb\n");

          return ur;
        }
        if (__unlikely(-1 == xioctl(dev->fd, VIDIOC_QBUF, &buf))) {
          perror("Failed at ioctl, VIDIOC_QBUF");

          return errno;
        }

        break;
      }
    }
  }
  return 0;
}
