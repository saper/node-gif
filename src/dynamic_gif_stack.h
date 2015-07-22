#ifndef DYNAMIC_gif_STACK_H
#define DYNAMIC_gif_STACK_H

#include <node.h>
#include <node_object_wrap.h>
#include <uv.h>

#include <utility>
#include <vector>

#include <cstdlib>

#include "common.h"

struct GifUpdate {
    int len, x, y, w, h;
    unsigned char *data;

    GifUpdate(unsigned char *ddata, int llen, int xx, int yy, int ww, int hh) :
        len(llen), x(xx), y(yy), w(ww), h(hh)
    {
        data = (unsigned char *)malloc(sizeof(*data)*len);
        if (!data) throw "malloc failed in DynamicGifStack::GifUpdate";
        memcpy(data, ddata, len);
    }

    ~GifUpdate() {
        free(data);
    }
};

class DynamicGifStack : public node::ObjectWrap {
    typedef std::vector<GifUpdate *> GifUpdates;
    GifUpdates gif_stack;

    Point offset;
    int width, height;
    buffer_type buf_type;
    Color transparency_color;

    std::pair<Point, Point> optimal_dimension();

    static void EIO_GifEncode(uv_work_t *req);
    static void EIO_GifEncodeAfter(uv_work_t *req, int status);
    void construct_gif_data(unsigned char *data, Point &top);

public:
    static void Initialize(v8::Isolate* isolate, v8::Handle<v8::Object> target);
    DynamicGifStack(buffer_type bbuf_type);
    ~DynamicGifStack();

    void Push(unsigned char *buf_data, size_t buf_len, int x, int y, int w, int h);
    v8::Local<v8::Value> Dimensions(void);
    v8::Local<v8::Value> GifEncodeSync(void);

    static void New(const v8::FunctionCallbackInfo<v8::Value> &args);
    static void Push(const v8::FunctionCallbackInfo<v8::Value> &args);
    static void Dimensions(const v8::FunctionCallbackInfo<v8::Value> &args);
    static void GifEncodeSync(const v8::FunctionCallbackInfo<v8::Value> &args);
    static void GifEncodeAsync(const v8::FunctionCallbackInfo<v8::Value> &args);
};

#endif

