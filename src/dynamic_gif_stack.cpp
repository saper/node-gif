#include "common.h"
#include "gif_encoder.h"
#include "dynamic_gif_stack.h"

#include <node_buffer.h>

using namespace v8;
using namespace node;

std::pair<Point, Point>
DynamicGifStack::optimal_dimension()
{
    Point top(-1, -1), bottom(-1, -1);
    for (GifUpdates::iterator it = gif_stack.begin(); it != gif_stack.end(); ++it) {
        GifUpdate *gif = *it;
        if (top.x == -1 || gif->x < top.x)
            top.x = gif->x;
        if (top.y == -1 || gif->y < top.y)
            top.y = gif->y;
        if (bottom.x == -1 || gif->x + gif->w > bottom.x)
            bottom.x = gif->x + gif->w;
        if (bottom.y == -1 || gif->y + gif->h > bottom.y)
            bottom.y = gif->y + gif->h;
    }

    return std::make_pair(top, bottom);
}

void
DynamicGifStack::construct_gif_data(unsigned char *data, Point &top)
{
    switch (buf_type) {
    case BUF_RGB:
    case BUF_RGBA:
        for (GifUpdates::iterator it = gif_stack.begin(); it != gif_stack.end(); ++it) {
            GifUpdate *gif = *it;
            int start = (gif->y - top.y)*width*3 + (gif->x - top.x)*3;
            unsigned char *gifdatap = gif->data;
            for (int i = 0; i < gif->h; i++) {
                unsigned char *datap = &data[start + i*width*3];
                for (int j = 0; j < gif->w; j++) {
                    *datap++ = *gifdatap++;
                    *datap++ = *gifdatap++;
                    *datap++ = *gifdatap++;
                    if (buf_type == BUF_RGBA) gifdatap++;
                }
            }
        }
        break;

    case BUF_BGR:
    case BUF_BGRA:
        for (GifUpdates::iterator it = gif_stack.begin(); it != gif_stack.end(); ++it) {
            GifUpdate *gif = *it;
            int start = (gif->y - top.y)*width*3 + (gif->x - top.x)*3;
            unsigned char *gifdatap = gif->data;
            for (int i = 0; i < gif->h; i++) {
                unsigned char *datap = &data[start + i*width*3];
                for (int j = 0; j < gif->w; j++) {
                    *datap++ = *(gifdatap + 2);
                    *datap++ = *(gifdatap + 1);
                    *datap++ = *gifdatap;
                    gifdatap += (buf_type == BUF_BGRA) ? 4 : 3;
                }
            }
        }
        break;

    default:
        throw "Unexpected buf_type in DynamicGifStack::GifEncode";
    }
}

void
DynamicGifStack::Initialize(Isolate* isolate, Handle<Object> target)
{
    HandleScope scope(isolate);

    Local<FunctionTemplate> t = FunctionTemplate::New(isolate, New);
    t->InstanceTemplate()->SetInternalFieldCount(1);
    NODE_SET_PROTOTYPE_METHOD(t, "push", Push);
    NODE_SET_PROTOTYPE_METHOD(t, "encode", GifEncodeAsync);
    NODE_SET_PROTOTYPE_METHOD(t, "encodeSync", GifEncodeSync);
    NODE_SET_PROTOTYPE_METHOD(t, "dimensions", Dimensions);
    target->Set(String::NewFromUtf8(isolate, "DynamicGifStack"), t->GetFunction());
}

DynamicGifStack::DynamicGifStack(buffer_type bbuf_type) :
    buf_type(bbuf_type), transparency_color(0xFF, 0xFF, 0xFE) {}

DynamicGifStack::~DynamicGifStack()
{
    for (GifUpdates::iterator it = gif_stack.begin(); it != gif_stack.end(); ++it)
        delete *it;
}

void
DynamicGifStack::Push(unsigned char *buf_data, size_t buf_len, int x, int y, int w, int h)
{
    try {
        GifUpdate *gif_update = new GifUpdate(buf_data, buf_len, x, y, w, h);
        gif_stack.push_back(gif_update);
    }
    catch (const char *e) {
        VException(e);
    }
}

v8::Local<v8::Value>
DynamicGifStack::GifEncodeSync()
{
    Isolate *isolate = Isolate::GetCurrent();
    EscapableHandleScope scope(isolate);

    std::pair<Point, Point> optimal = optimal_dimension();
    Point top = optimal.first, bot = optimal.second;

    offset = top;
    width = bot.x - top.x;
    height = bot.y - top.y;

    unsigned char *data = (unsigned char*)malloc(sizeof(*data)*width*height*3);
    if (!data) VException("malloc failed in DynamicGifStack::GifEncode");

    unsigned char *datap = data;
    for (int i = 0; i < width*height*3; i+=3) {
        *datap++ = transparency_color.r;
        *datap++ = transparency_color.g;
        *datap++ = transparency_color.b;
    }

    construct_gif_data(data, top);

    try {
        GifEncoder encoder(data, width, height, BUF_RGB);
        encoder.set_transparency_color(transparency_color);
        encoder.encode();
        free(data);
        int gif_len = encoder.get_gif_len();
	return scope.Escape(node::Buffer::New(isolate,
		(const char *)encoder.get_gif(), gif_len));
    }
    catch (const char *err) {
        VException(err);
    }
}

v8::Local<v8::Value>
DynamicGifStack::Dimensions()
{
    Isolate *isolate = Isolate::GetCurrent();
    EscapableHandleScope scope(isolate);

    Local<Object> dim = Object::New(isolate);
    dim->Set(String::NewFromUtf8(isolate, "x"), Integer::New(isolate, offset.x));
    dim->Set(String::NewFromUtf8(isolate, "y"), Integer::New(isolate, offset.y));
    dim->Set(String::NewFromUtf8(isolate, "width"), Integer::New(isolate, width));
    dim->Set(String::NewFromUtf8(isolate, "height"), Integer::New(isolate, height));

    return scope.Escape(dim);
}

void
DynamicGifStack::New(const FunctionCallbackInfo<Value> &args)
{
    Isolate *isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);

    buffer_type buf_type = BUF_RGB;
    if (args.Length() == 1) {
        if (!args[0]->IsString())
            VException("First argument must be 'rgb', 'bgr', 'rgba' or 'bgra'.");

        String::Utf8Value bts(args[0]->ToString());
        if (!(str_eq(*bts, "rgb") || str_eq(*bts, "bgr") ||
            str_eq(*bts, "rgba") || str_eq(*bts, "bgra")))
        {
            VException("First argument must be 'rgb', 'bgr', 'rgba' or 'bgra'.");
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
            VException("First argument wasn't 'rgb', 'bgr', 'rgba' or 'bgra'.");
    }

    DynamicGifStack *gif_stack = new DynamicGifStack(buf_type);
    gif_stack->Wrap(args.This());
    args.GetReturnValue().Set(args.This());
}

void
DynamicGifStack::Push(const FunctionCallbackInfo<Value> &args)
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

    DynamicGifStack *gif_stack = ObjectWrap::Unwrap<DynamicGifStack>(args.This());

    Local<Object> buf_obj = args[0]->ToObject();
    char *buf_data = node::Buffer::Data(buf_obj);
    size_t buf_len = node::Buffer::Length(buf_obj);

    gif_stack->Push((unsigned char*)buf_data, buf_len, x, y, w, h);
}

void
DynamicGifStack::Dimensions(const FunctionCallbackInfo<Value> &args)
{
    Isolate *isolate = Isolate::GetCurrent();
    EscapableHandleScope scope(isolate);

    DynamicGifStack *gif_stack = ObjectWrap::Unwrap<DynamicGifStack>(args.This());
    args.GetReturnValue().Set(scope.Escape(gif_stack->Dimensions()));
}

void
DynamicGifStack::GifEncodeSync(const FunctionCallbackInfo<Value> &args)
{
    Isolate *isolate = Isolate::GetCurrent();
    EscapableHandleScope scope(isolate);

    DynamicGifStack *gif_stack = ObjectWrap::Unwrap<DynamicGifStack>(args.This());
    args.GetReturnValue().Set(scope.Escape(gif_stack->GifEncodeSync()));
}

void
DynamicGifStack::EIO_GifEncode(uv_work_t *req)
{
    encode_request *enc_req = (encode_request *)req->data;
    DynamicGifStack *gif = (DynamicGifStack *)enc_req->gif_obj;

    std::pair<Point, Point> optimal = gif->optimal_dimension();
    Point top = optimal.first, bot = optimal.second;

    gif->offset = top;
    gif->width = bot.x - top.x;
    gif->height = bot.y - top.y;

    unsigned char *data = (unsigned char*)malloc(sizeof(*data)*gif->width*gif->height*3);
    if (!data) {
        enc_req->error = strdup("malloc failed in DynamicGifStack::EIO_GifEncode.");
        return;
    }

    unsigned char *datap = data;
    for (int i = 0; i < gif->width*gif->height; i++) {
        *datap++ = gif->transparency_color.r;
        *datap++ = gif->transparency_color.g;
        *datap++ = gif->transparency_color.b;
    }

    gif->construct_gif_data(data, top);

    buffer_type pbt = (gif->buf_type == BUF_BGR || gif->buf_type == BUF_BGRA) ?
        BUF_BGRA : BUF_RGBA;

    try {
        GifEncoder encoder(data, gif->width, gif->height, BUF_RGB);
        encoder.set_transparency_color(gif->transparency_color);
        encoder.encode();
        free(data);
        enc_req->gif_len = encoder.get_gif_len();
        enc_req->gif = (char *)calloc(enc_req->gif_len, sizeof(*enc_req->gif));
        if (!enc_req->gif) {
            enc_req->error = strdup("malloc in DynamicGifStack::EIO_GifEncode failed.");
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
DynamicGifStack::EIO_GifEncodeAfter(uv_work_t *req, int status)
{
    Isolate *isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);

    //ev_unref(EV_DEFAULT_UC);
    
    encode_request *enc_req = (encode_request *)req->data;
    DynamicGifStack *gif = (DynamicGifStack *)enc_req->gif_obj;

    Handle<Value> argv[3];

    if (enc_req->error) {
        argv[0] = Undefined(isolate);
        argv[1] = Undefined(isolate);
        argv[2] = ErrorException(enc_req->error);
    }
    else {
        argv[0] = node::Buffer::New(isolate, (const char *)enc_req->gif, enc_req->gif_len);
        argv[1] = gif->Dimensions();
        argv[2] = Undefined(isolate);
    }

    TryCatch try_catch; // don't quite see the necessity of this

    Local<Function> cb = Local<Function>::New(isolate, enc_req->callback);
    cb->Call(isolate->GetCurrentContext()->Global(), 3, argv);

    if (try_catch.HasCaught())
        FatalException(try_catch);

    enc_req->callback.Reset();
    free(enc_req->gif);
    free(enc_req->error);

    gif->Unref();
    free(enc_req);

    //return 0;
}

void
DynamicGifStack::GifEncodeAsync(const FunctionCallbackInfo<Value> &args)
{
    Isolate *isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);

    if (args.Length() != 1)
        VException("One argument required - callback function.");

    if (!args[0]->IsFunction())
        VException("First argument must be a function.");

    DynamicGifStack *gif = ObjectWrap::Unwrap<DynamicGifStack>(args.This());

    encode_request *enc_req = (encode_request *)calloc(1, sizeof(*enc_req));
    if (!enc_req)
        VException("malloc in DynamicGifStack::GifEncodeAsync failed.");

    enc_req->callback.Reset(isolate, args[0].As<Function>());
    enc_req->gif_obj = gif;
    enc_req->gif = NULL;
    enc_req->gif_len = 0;
    enc_req->error = NULL;

    uv_work_t * req = new uv_work_t;
    req->data = enc_req;

    //eio_custom(EIO_GifEncode, EIO_PRI_DEFAULT, EIO_GifEncodeAfter, enc_req);
    uv_queue_work(uv_default_loop(), req, &EIO_GifEncode, &EIO_GifEncodeAfter);
    //ev_ref(EV_DEFAULT_UC);
    gif->Ref();
}

