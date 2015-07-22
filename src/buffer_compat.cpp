#include <node.h>
#include <node_buffer.h>
#include <node_version.h>
#include <v8.h>
#include "buffer_compat.h"

char *BufferData(node::Buffer *b) {
  return node::Buffer::Data(b->handle_);
}


size_t BufferLength(node::Buffer *b) {
  return node::Buffer::Length(b->handle_);
}


char *BufferData(v8::Local<v8::Object> buf_obj) {
  v8::HandleScope scope;
  return node::Buffer::Data(buf_obj);
}


size_t BufferLength(v8::Local<v8::Object> buf_obj) {
  v8::HandleScope scope;
  return node::Buffer::Length(buf_obj);
}

