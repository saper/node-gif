#include <cstdlib>

#include "common.h"
#include "gif_encoder.h"
#include "animated_gif.h"
#include "buffer_compat.h"

#include <iostream>

using namespace v8;
using namespace node;

void
AnimatedGif::Initialize(Isolate *isolate, Handle<Object> target)
{
    HandleScope scope(isolate);

    Local<FunctionTemplate> t = FunctionTemplate::New(isolate, New);
    t->InstanceTemplate()->SetInternalFieldCount(1);
    NODE_SET_PROTOTYPE_METHOD(t, "push", Push);
    NODE_SET_PROTOTYPE_METHOD(t, "endPush", EndPush);
    NODE_SET_PROTOTYPE_METHOD(t, "getGif", GetGif);
    NODE_SET_PROTOTYPE_METHOD(t, "end", End);
    NODE_SET_PROTOTYPE_METHOD(t, "setOutputFile", SetOutputFile);
    NODE_SET_PROTOTYPE_METHOD(t, "setOutputCallback", SetOutputCallback);
    target->Set(String::NewFromUtf8(isolate, "AnimatedGif"), t->GetFunction());
}

AnimatedGif::AnimatedGif(int wwidth, int hheight, buffer_type bbuf_type) :
    width(wwidth), height(hheight), buf_type(bbuf_type),
    gif_encoder(wwidth, hheight, BUF_RGB), transparency_color(0xFF, 0xFF, 0xFE),
    data(NULL)
{
    gif_encoder.set_transparency_color(transparency_color);
}

void
AnimatedGif::Push(unsigned char *data_buf, int x, int y, int w, int h)
{
    if (!data) {
        data = (unsigned char *)malloc(sizeof(*data)*width*height*3);
        if (!data) throw "malloc in AnimatedGif::Push failed";

        unsigned char *datap = data;
        for (int i = 0; i < width*height; i++) {
            *datap++ = transparency_color.r;
            *datap++ = transparency_color.g;
            *datap++ = transparency_color.b;
        }
    }

    int start = y*width*3 + x*3;

    unsigned char *data_bufp = data_buf;

    switch (buf_type) {
    case BUF_RGB:
        for (int i = 0; i < h; i++) {
            unsigned char *datap = &data[start + i*width*3];
            for (int j = 0; j < w; j++) {
                *datap++ = *data_bufp++;
                *datap++ = *data_bufp++;
                *datap++ = *data_bufp++;
            }
        }
        break;
    case BUF_BGR:
        for (int i = 0; i < h; i++) {
            unsigned char *datap = &data[start + i*width*3];
            for (int j = 0; j < w; j++) {
                *datap++ = *(data_bufp + 2);
                *datap++ = *(data_bufp + 1);
                *datap++ = *data_bufp;
                data_bufp += 3;
            }
        }
        break;
    }
}

void
AnimatedGif::EndPush()
{
    gif_encoder.new_frame(data);
    free(data);
    data = NULL;
}

void
AnimatedGif::New(const FunctionCallbackInfo<Value>& args)
{
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);

    if (args.Length() < 2)
        VException("At least two arguments required - width, height, [and input buffer type]");
    if (!args[0]->IsInt32())
        VException("First argument must be integer width.");
    if (!args[1]->IsInt32())
        VException("Second argument must be integer height.");

    buffer_type buf_type = BUF_RGB;
    if (args.Length() == 3) {
        if (!args[2]->IsString())
            VException("Third argument must be 'rgb', 'bgr', 'rgba' or 'bgra'.");

        String::Utf8Value bts(args[2]->ToString());
        if (!(str_eq(*bts, "rgb") || str_eq(*bts, "bgr") ||
            str_eq(*bts, "rgba") || str_eq(*bts, "bgra")))
        {
            VException("Third argument must be 'rgb', 'bgr', 'rgba' or 'bgra'.");
        }

        if (str_eq(*bts, "rgb"))
            buf_type = BUF_RGB;
        else if (str_eq(*bts, "bgr"))
            buf_type = BUF_BGR;
        else if (str_eq(*bts, "rgba"))
            buf_type = BUF_RGBA;
        else if (str_eq(*bts, "bgra"))
            buf_type = BUF_BGRA;
        else
            VException("Third argument wasn't 'rgb', 'bgr', 'rgba' or 'bgra'.");
    }

    int w = args[0]->Int32Value();
    int h = args[1]->Int32Value();

    if (w < 0)
        VException("Width smaller than 0.");
    if (h < 0)
        VException("Height smaller than 0.");

    AnimatedGif *gif = new AnimatedGif(w, h, buf_type);
    gif->Wrap(args.This());
    args.GetReturnValue().Set(args.This());
}

void
AnimatedGif::Push(const FunctionCallbackInfo<Value>& args)
{
    Isolate *isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);

    if (!Buffer::HasInstance(args[0]))
        VException("First argument must be Buffer.");
    if (!args[1]->IsInt32())
        VException("Second argument must be integer x.");
    if (!args[2]->IsInt32())
        VException("Third argument must be integer y.");
    if (!args[3]->IsInt32())
        VException("Fourth argument must be integer w.");
    if (!args[4]->IsInt32())
        VException("Fifth argument must be integer h.");

    AnimatedGif *gif = ObjectWrap::Unwrap<AnimatedGif>(args.This());
    int x = args[1]->Int32Value();
    int y = args[2]->Int32Value();
    int w = args[3]->Int32Value();
    int h = args[4]->Int32Value();

    if (x < 0)
        VException("Coordinate x smaller than 0.");
    if (y < 0)
        VException("Coordinate y smaller than 0.");
    if (w < 0)
        VException("Width smaller than 0.");
    if (h < 0)
        VException("Height smaller than 0.");
    if (x >= gif->width)
        VException("Coordinate x exceeds AnimatedGif's dimensions.");
    if (y >= gif->height)
        VException("Coordinate y exceeds AnimatedGif's dimensions.");
    if (x+w > gif->width)
        VException("Pushed fragment exceeds AnimatedGif's width.");
    if (y+h > gif->height)
        VException("Pushed fragment exceeds AnimatedGif's height.");

    try {
        char *buf_data = BufferData(args[0]->ToObject());

        gif->Push((unsigned char*)buf_data, x, y, w, h);
    }
    catch (const char *err) {
        VException(err);
    }
}

void
AnimatedGif::EndPush(const FunctionCallbackInfo<Value>& args)
{
    Isolate *isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);

    try {
        AnimatedGif *gif = ObjectWrap::Unwrap<AnimatedGif>(args.This());
        gif->EndPush();
    }
    catch (const char *err) {
        VException(err);
    }
}

void
AnimatedGif::GetGif(const FunctionCallbackInfo<Value>& args)
{
    Isolate *isolate = Isolate::GetCurrent();
    EscapableHandleScope scope(isolate);

    AnimatedGif *gif = ObjectWrap::Unwrap<AnimatedGif>(args.This());
    gif->gif_encoder.finish();
    int gif_len = gif->gif_encoder.get_gif_len();

    args.GetReturnValue().Set(scope.Escape(node::Buffer::New(isolate, (const char *)gif->gif_encoder.get_gif(), gif_len)));
}

void
AnimatedGif::End(const FunctionCallbackInfo<Value>& args)
{
    Isolate *isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);

    AnimatedGif *gif = ObjectWrap::Unwrap<AnimatedGif>(args.This());
    gif->gif_encoder.finish();
}

void
AnimatedGif::SetOutputFile(const FunctionCallbackInfo<Value>& args)
{
    Isolate *isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);

    if (args.Length() != 1)
        VException("One argument required - path to output file.");

    if (!args[0]->IsString())
        VException("First argument must be string.");

    String::Utf8Value file_name(args[0]->ToString());

    AnimatedGif *gif = ObjectWrap::Unwrap<AnimatedGif>(args.This());
    gif->gif_encoder.set_output_file(*file_name);
}

int
stream_writer(GifFileType *gif_file, const GifByteType *data, int size)
{
    Isolate *isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    Local<Function> onData;

    AnimatedGif *gif = (AnimatedGif *)gif_file->UserData;
    onData = Local<Function>::Cast(gif->ondata);
    Handle<Value> argv[1] = { node::Buffer::New(isolate, (const char *)data, size) };
    onData->Call(isolate, Context::GetCurrent()->Global(), 1, argv);
    return size;
}

void
AnimatedGif::SetOutputCallback(const FunctionCallbackInfo<Value>& args)
{
    Isolate *isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);

    if (args.Length() != 1)
        VException("One argument required - path to output file.");

    if (!args[0]->IsFunction())
        VException("First argument must be a function name.");

    AnimatedGif *gif = ObjectWrap::Unwrap<AnimatedGif>(args.This());
    gif->ondata.Reset(isolate, args[0].As<Function>());
    gif->gif_encoder.set_output_func(stream_writer, (void*)gif);
}
