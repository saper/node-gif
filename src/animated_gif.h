#ifndef ANIMATED_GIF_H
#define ANIMATED_GIF_H

#include <node.h>
#include <node_object_wrap.h>

#include "gif_encoder.h"
#include "common.h"

class AnimatedGif : public node::ObjectWrap {
    int width, height;
    buffer_type buf_type;

    AnimatedGifEncoder gif_encoder;
    unsigned char *data;
    Color transparency_color;

public:

    v8::Persistent<v8::Function> ondata;

    static void Initialize(v8::Isolate *isolate, v8::Handle<v8::Object> target);

    AnimatedGif(int wwidth, int hheight, buffer_type bbuf_type);
    void Push(unsigned char *data_buf, int x, int y, int w, int h);
    void EndPush();

    static void New(const v8::FunctionCallbackInfo<v8::Value> &args);
    static void Push(const v8::FunctionCallbackInfo<v8::Value> &args);
    static void EndPush(const v8::FunctionCallbackInfo<v8::Value> &args);
    static void End(const v8::FunctionCallbackInfo<v8::Value> &args);
    static void GetGif(const v8::FunctionCallbackInfo<v8::Value> &args);
    static void SetOutputFile(const v8::FunctionCallbackInfo<v8::Value> &args);
    static void SetOutputCallback(const v8::FunctionCallbackInfo<v8::Value> &args);
};

#endif

