#ifndef HALIDE_bilateral_grid
#define HALIDE_bilateral_grid
#ifndef BUFFER_T_DEFINED
#define BUFFER_T_DEFINED
#include <stdint.h>
typedef struct buffer_t {
    uint8_t* host;
    uint64_t dev;
    bool host_dirty;
    bool dev_dirty;
    int32_t extent[4];
    int32_t stride[4];
    int32_t min[4];
    int32_t elem_size;
} buffer_t;
#endif
extern "C" void bilateral_grid(float p1, const buffer_t *p0, const buffer_t *bilateral_grid);
#endif
