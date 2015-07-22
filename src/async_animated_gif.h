#ifndef ASYNC_ANIMATED_GIF_H
#define ASYNC_ANIMATED_GIF_H

#include <string>

#include <node.h>
#include <node_object_wrap.h>
#include <uv.h>

#include "gif_encoder.h"
#include "common.h"

struct push_request {
    unsigned int push_id;
    unsigned int fragment_id;
    const char *tmp_dir;
    unsigned char *data;
    int data_size;
    int x, y, w, h;
};

class AsyncAnimatedGif;

struct async_encode_request {
    AsyncAnimatedGif *gif_obj;
    v8::Persistent<v8::Function> callback;
    char *error;
};

class AsyncAnimatedGif : public node::ObjectWrap {
    int width, height;
    buffer_type buf_type;

    Color transparency_color;

    unsigned int push_id, fragment_id;
    std::string tmp_dir, output_file;

    static void EIO_Push(uv_work_t *req);
    static void EIO_PushAfter(uv_work_t *req, int status);

    static void EIO_Encode(uv_work_t *req);
    static void EIO_EncodeAfter(uv_work_t *req, int status);

    static unsigned char *init_frame(int width, int height, Color &transparency_color);
    static void push_fragment(unsigned char *frame, int width, int height, buffer_type buf_type,
        unsigned char *fragment, int x, int y, int w, int h);
    static Rect rect_dims(const char *fragment_name);

public:
    static void Initialize(v8::Isolate* isolate, v8::Handle<v8::Object> target);

    AsyncAnimatedGif(int wwidth, int hheight, buffer_type bbuf_type);
    void Push(unsigned char *data_buf, int x, int y, int w, int h);
    void EndPush();

    static void New(const v8::FunctionCallbackInfo<v8::Value> &args);
    static void Push(const v8::FunctionCallbackInfo<v8::Value> &args);
    static void Encode(const v8::FunctionCallbackInfo<v8::Value> &args);
    static void EndPush(const v8::FunctionCallbackInfo<v8::Value> &args);
    static void SetOutputFile(const v8::FunctionCallbackInfo<v8::Value> &args);
    static void SetTmpDir(const v8::FunctionCallbackInfo<v8::Value> &args);
};

#endif

