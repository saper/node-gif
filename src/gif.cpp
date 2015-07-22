#include <cstdlib>
#include <cstring>
#include "common.h"
#include "gif_encoder.h"
#include "gif.h"

#include <node_buffer.h>

using namespace v8;
using namespace node;

void
Gif::Initialize(Isolate* isolate, Handle<Object> target)
{
    HandleScope scope(isolate);

    Local<FunctionTemplate> t = FunctionTemplate::New(isolate, New);
    t->InstanceTemplate()->SetInternalFieldCount(1);
    NODE_SET_PROTOTYPE_METHOD(t, "encode", GifEncodeAsync);
    NODE_SET_PROTOTYPE_METHOD(t, "encodeSync", GifEncodeSync);
    NODE_SET_PROTOTYPE_METHOD(t, "setTransparencyColor", SetTransparencyColor);
    target->Set(String::NewFromUtf8(isolate, "Gif"), t->GetFunction());
}

Gif::Gif(int wwidth, int hheight, buffer_type bbuf_type) :
  width(wwidth), height(hheight), buf_type(bbuf_type) {}

v8::Local<v8::Value>
Gif::GifEncodeSync()
{
    Isolate *isolate = Isolate::GetCurrent();
    EscapableHandleScope scope(isolate);

    Local<Value> buf_val = handle_->GetHiddenValue(String::New("buffer"));

    char *buf_data = node::Buffer::Data(buf_val->ToObject());

    try {
        GifEncoder encoder((unsigned char*)buf_data, width, height, buf_type);
        if (transparency_color.color_present) {
            encoder.set_transparency_color(transparency_color);
        }
        encoder.encode();
        int gif_len = encoder.get_gif_len();
        return scope.Escape(node::Buffer::New(isolate,
		(const char *)encoder.get_gif(), gif_len));
    }
    catch (const char *err) {
        VException(err);
    }
}

void
Gif::SetTransparencyColor(unsigned char r, unsigned char g, unsigned char b)
{
    transparency_color = Color(r, g, b, true);
}

void
Gif::New(const FunctionCallbackInfo<Value> &args)
{
    Isolate *isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);

    if (args.Length() < 3)
        VException("At least three arguments required - data buffer, width, height, [and input buffer type]");
    if (!Buffer::HasInstance(args[0]))
        VException("First argument must be Buffer.");
    if (!args[1]->IsInt32())
        VException("Second argument must be integer width.");
    if (!args[2]->IsInt32())
        VException("Third argument must be integer height.");

    buffer_type buf_type = BUF_RGB;
    if (args.Length() == 4) {
        if (!args[3]->IsString())
            VException("Fourth argument must be 'rgb', 'bgr', 'rgba' or 'bgra'.");

        String::Utf8Value bts(args[3]->ToString());
        if (!(str_eq(*bts, "rgb") || str_eq(*bts, "bgr") ||
            str_eq(*bts, "rgba") || str_eq(*bts, "bgra")))
        {
            VException("Fourth argument must be 'rgb', 'bgr', 'rgba' or 'bgra'.");
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
            VException("Fourth argument wasn't 'rgb', 'bgr', 'rgba' or 'bgra'.");
    }


    int w = args[1]->Int32Value();
    int h = args[2]->Int32Value();

    if (w < 0)
        VException("Width smaller than 0.");
    if (h < 0)
        VException("Height smaller than 0.");

    Gif *gif = new Gif(w, h, buf_type);
    gif->Wrap(args.This());

    // Save buffer.
    gif->handle_->SetHiddenValue(String::New("buffer"), args[0]);

    args.GetReturnValue().Set(args.This());
}

void
Gif::GifEncodeSync(const FunctionCallbackInfo<Value> &args)
{
    Isolate *isolate = Isolate::GetCurrent();
    EscapableHandleScope scope(isolate);

    Gif *gif = ObjectWrap::Unwrap<Gif>(args.This());
    args.GetReturnValue().Set(scope.Escape(gif->GifEncodeSync()));
}

void
Gif::SetTransparencyColor(const FunctionCallbackInfo<Value> &args)
{
    Isolate *isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);

    if (args.Length() != 3)
        VException("Three arguments required - r, g, b");

    if (!args[0]->IsInt32())
        VException("First argument must be integer red.");
    if (!args[1]->IsInt32())
        VException("Second argument must be integer green.");
    if (!args[2]->IsInt32())
        VException("Third argument must be integer blue.");

    unsigned char r = args[0]->Int32Value();
    unsigned char g = args[1]->Int32Value();
    unsigned char b = args[2]->Int32Value();

    Gif *gif = ObjectWrap::Unwrap<Gif>(args.This());
    gif->SetTransparencyColor(r, g, b);
}

void
Gif::EIO_GifEncode(uv_work_t *req)
{
    encode_request *enc_req = (encode_request *)req->data;
    Gif *gif = (Gif *)enc_req->gif_obj;

    try {
        GifEncoder encoder((unsigned char *)enc_req->buf_data, gif->width, gif->height, gif->buf_type);
        if (gif->transparency_color.color_present) {
            encoder.set_transparency_color(gif->transparency_color);
        }
        encoder.encode();
        enc_req->gif_len = encoder.get_gif_len();
        enc_req->gif = (char *)malloc(sizeof(*enc_req->gif)*enc_req->gif_len);
        if (!enc_req->gif) {
            enc_req->error = strdup("malloc in Gif::EIO_GifEncode failed.");
            return;
        }
        else {
            memcpy(enc_req->gif, encoder.get_gif(), enc_req->gif_len);
        }
    }
    catch (const char *err) {
        enc_req->error = strdup(err);
    }

    return;
}

void
Gif::EIO_GifEncodeAfter(uv_work_t *req, int status)
{
    Isolate *isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);

    //ev_unref(EV_DEFAULT_UC);
    encode_request *enc_req = (encode_request *)req->data;

    Handle<Value> argv[2];

    if (enc_req->error) {
        argv[0] = Undefined(isolate);
        argv[1] = ErrorException(enc_req->error);
    }
    else {
        argv[0] = node::Buffer::New(isolate, enc_req->gif, enc_req->gif_len);
        argv[1] = Undefined(isolate);
    }

    TryCatch try_catch; // don't quite see the necessity of this

    Local<Function> cb = Local<Function>::New(isolate, enc_req->callback);
    cb->Call(isolate->GetCurrentContext()->Global(), 2, argv);

    if (try_catch.HasCaught())
        FatalException(try_catch);

    enc_req->callback.Reset();
    free(enc_req->gif);
    free(enc_req->error);

    ((Gif *)enc_req->gif_obj)->Unref();
    free(enc_req);

    //return 0;
}

void
Gif::GifEncodeAsync(const FunctionCallbackInfo<Value> &args)
{
    Isolate *isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);

    if (args.Length() != 1)
        VException("One argument required - callback function.");

    if (!args[0]->IsFunction())
        VException("First argument must be a function.");

    Gif *gif = ObjectWrap::Unwrap<Gif>(args.This());

    encode_request *enc_req = (encode_request *)malloc(sizeof(*enc_req));
    if (!enc_req)
        VException("malloc in Gif::GifEncodeAsync failed.");

    enc_req->callback.Reset(isolate, args[0].As<Function>());
    enc_req->gif_obj = gif;
    enc_req->gif = NULL;
    enc_req->gif_len = 0;
    enc_req->error = NULL;

    // We need to pull out the buffer data before
    // we go to the thread pool.
    Local<Value> buf_val = gif->handle_->GetHiddenValue(String::New("buffer"));

    enc_req->buf_data = node::Buffer::Data(buf_val->ToObject());

    uv_work_t *req = new uv_work_t;

    req->data = enc_req;
    
    //eio_custom(EIO_GifEncode, EIO_PRI_DEFAULT, EIO_GifEncodeAfter, enc_req);

    uv_queue_work(uv_default_loop(), req, EIO_GifEncode, EIO_GifEncodeAfter);

    //ev_ref(EV_DEFAULT_UC);
    
    gif->Ref();
}

