#ifndef NODE_GIF_H
#define NODE_GIF_H

#include <node.h>
#include <node_object_wrap.h>
#include <node_buffer.h>
#include <uv.h>

#include "common.h"

class Gif : public node::ObjectWrap {
    int width, height;
    buffer_type buf_type;
    v8::Persistent<v8::Object> framebuffer;
    Color transparency_color;

    static void EIO_GifEncode(uv_work_t *req);
    static void EIO_GifEncodeAfter(uv_work_t *req, int status);

public:
    Gif(int wwidth, int hheight, buffer_type bbuf_type);
    static void Initialize(v8::Isolate *isolate, v8::Handle<v8::Object> target);
    v8::Local<v8::Value> GifEncodeSync();
    void SetTransparencyColor(unsigned char r, unsigned char g, unsigned char b);

    static void New(const v8::FunctionCallbackInfo<v8::Value> &args);
    static void GifEncodeSync(const v8::FunctionCallbackInfo<v8::Value> &args);
    static void GifEncodeAsync(const v8::FunctionCallbackInfo<v8::Value> &args);
    static void SetTransparencyColor(const v8::FunctionCallbackInfo<v8::Value> &args);
};

#endif

