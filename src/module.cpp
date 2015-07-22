#include <node.h>

#include "gif.h"
//#include "fixed_gif_stack.h"
#include "dynamic_gif_stack.h"
#include "animated_gif.h"
#include "async_animated_gif.h"

using namespace v8;

extern "C" void
init(Handle<Object> target)
{
    Isolate *isolate = v8::Isolate::GetCurrent();
    HandleScope scope(isolate);
    Gif::Initialize(isolate, target);
    //FixedGifStack::Initialize(isolate, target);
    DynamicGifStack::Initialize(isolate, target);
    AnimatedGif::Initialize(isolate, target);
    AsyncAnimatedGif::Initialize(isolate, target);
}


void RegisterModule(Handle<Object> target) {
    init(target);
}

NODE_MODULE(gif, RegisterModule);
