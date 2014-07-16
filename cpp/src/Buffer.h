#ifndef HALIDE_BUFFER_H
#define HALIDE_BUFFER_H

#include "IntrusivePtr.h"
#include "Type.h"
#include "Argument.h"
#include "Util.h"
#include "buffer_t.h"
#include <stdint.h>

/** \file 
 * Defines Buffer - A c++ wrapper around a buffer_t.
 */

namespace Halide {

namespace Internal {
struct BufferContents {
    buffer_t buf;
    Type type;
    uint8_t *allocation;
    mutable RefCount ref_count;
    std::string name;
    
    uint8_t *access_base;

    BufferContents(Type t, int x_size, int y_size, int z_size, int w_size,
                   uint8_t* data = NULL, int x_alloc = 0, int y_alloc = 0, int z_alloc = 0, int w_alloc = 0,
                   int x_min = 0, int y_min = 0, int z_min = 0, int w_min = 0) :
        type(t), allocation(NULL), name(unique_name('b')) {
        assert(t.width == 1 && "Can't create of a buffer of a vector type");
        buf.elem_size = t.bits / 8;        
		if (x_alloc == 0) x_alloc = x_size;
        if (y_alloc == 0) y_alloc = y_size;
        if (z_alloc == 0) z_alloc = z_size;
        if (w_alloc == 0) w_alloc = w_size;
        size_t size = 1;
        if (x_alloc) size *= x_alloc;
        if (y_alloc) size *= y_alloc;
        if (z_alloc) size *= z_alloc;
        if (w_alloc) size *= w_alloc;
        if (!data) {
            allocation = (uint8_t *)calloc(buf.elem_size, size + 32/buf.elem_size);
            buf.host = allocation;
            while ((size_t)(buf.host) & 0x1f) buf.host++;
        } else {
            buf.host = data;
        }
        access_base = buf.host;
        buf.host_dirty = false;
        buf.dev_dirty = false;
        buf.extent[0] = x_size;
        buf.extent[1] = y_size;
        buf.extent[2] = z_size;
        buf.extent[3] = w_size;
        buf.stride[0] = 1;
        buf.stride[1] = x_alloc;
        buf.stride[2] = x_alloc * y_alloc;
        buf.stride[3] = x_alloc * y_alloc * z_alloc;
        // Non-zero minimum does not seem to work.
        // Instead, just hack the address of the buffer.
        // Note: This means that min must be a multiple of 32.
        size_t offset = (x_min + (y_min * x_alloc) + (z_min * x_alloc * y_alloc) + (w_min * x_alloc * y_alloc * z_alloc)) * buf.elem_size;
        assert ((offset & 0x1f) == 0 && "Offset for minimum must be a multiple of 32 bytes");
        buf.host += offset;
        buf.min[0] = x_min;
        buf.min[1] = y_min;
        buf.min[2] = z_min;
        buf.min[3] = w_min;
    }

    BufferContents(Type t, const buffer_t *b) :
        type(t), allocation(NULL) {
        buf = *b;
        assert(t.width == 1 && "Can't create of a buffer of a vector type");
    }
};
}

/** The internal representation of an image, or other dense array
 * data. The Image type provides a typed view onto a buffer for the
 * purposes of direct manipulation. A buffer may be stored in main
 * memory, or some other memory space (e.g. a gpu). If you want to use
 * this as an Image, see the Image class. Casting a Buffer to an Image
 * will do any appropriate copy-back. This class is a fairly thin
 * wrapper on a buffer_t, which is the C-style type Halide uses for
 * passing buffers around.
 */
class Buffer {
private:
    Internal::IntrusivePtr<const Internal::BufferContents> contents;
public:
    Buffer() : contents(NULL) {}

    Buffer(Type t, int x_size = 0, int y_size = 0, int z_size = 0, int w_size = 0,
           uint8_t* data = NULL, int x_alloc = 0, int y_alloc = 0, int z_alloc = 0, int w_alloc = 0,
           int x_min = 0, int y_min = 0, int z_min = 0, int w_min = 0) :
        contents(new Internal::BufferContents(t, x_size, y_size, z_size, w_size, data, x_alloc, y_alloc, z_alloc, w_alloc,
                                              x_min, y_min, z_min, w_min)) {
    }
    
    Buffer(Type t, const buffer_t *buf) : 
        contents(new Internal::BufferContents(t, buf)) {
    }

    void *host_ptr() const {
        assert(defined());
        return (void *)contents.ptr->buf.host; // Offset for the minimum
    }
    
    void *access_ptr() const {
        assert(defined());
        return (void *)contents.ptr->access_base; // Not offset for minimum
    }
    
    const buffer_t *raw_buffer() const {
        assert(defined());
        return &(contents.ptr->buf);
    }
    
    uint64_t device_handle() const {
        assert(defined());
        return contents.ptr->buf.dev;
    }
    
    bool host_dirty() const {
        assert(defined());
        return contents.ptr->buf.host_dirty;
    }
    
    bool device_dirty() const {
        assert(defined());
        return contents.ptr->buf.dev_dirty;
    }
    
    int dimensions() const {
        for (int i = 0; i < 4; i++) {
            if (extent(i) == 0) return i;
        }
        return 4;
    }

    int extent(int dim) const {
        assert(defined());
        assert(dim >= 0 && dim < 4 && "We only support 4-dimensional buffers for now");
        return contents.ptr->buf.extent[dim];
    }
    
    int stride(int dim) const {
        assert(defined());
        assert(dim >= 0 && dim < 4 && "We only support 4-dimensional buffers for now");
        return contents.ptr->buf.stride[dim];
    }
    
    int min(int dim) const {
        assert(defined());
        assert(dim >= 0 && dim < 4 && "We only support 4-dimensional buffers for now");
        return contents.ptr->buf.min[dim];
    }
    
    Type type() const {
        assert(defined());
        return contents.ptr->type;
    }    

    bool same_as(const Buffer &other) const {
        return contents.same_as(other.contents);
    }

    bool defined() const {
        return contents.defined();
    }

    const std::string &name() const {
        return contents.ptr->name;
    }

    operator Argument() const {
        return Argument(name(), true, type());
    }

};

namespace Internal {
template<>
inline RefCount &ref_count<BufferContents>(const BufferContents *p) {
    return p->ref_count;
}

template<>
inline void destroy<BufferContents>(const BufferContents *p) {    
    if (p->allocation) free(p->allocation);
    delete p;
}
}

}

#endif
