/*
Copyright (c) 2017 Scott Frees (original streaming worker)
Copyright (c) 2018 Bernd Kaiser, feinarbyte GmbH (adjusts for ArrayBuffers and error handling)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#ifndef ____StreamingWorker__
#define ____StreamingWorker__
#include <iostream>
#include <string>
#include <algorithm>
#include <iterator>
#include <thread>
#include <deque>
#include <mutex>
#include <chrono>
#include <condition_variable>

DISABLE_WCAST_FUNCTION_TYPE
#include <nan.h>
DISABLE_WCAST_FUNCTION_TYPE_END

using namespace Nan;
using namespace std;

template <typename Data>
class PCQueue
{
public:
  void write(Data data)
  {
    while (true)
    {
      std::unique_lock<std::mutex> locker(mu);
      buffer_.push_back(data);
      locker.unlock();
      cond.notify_all();
      return;
    }
  }
  Data read()
  {
    while (true)
    {
      std::unique_lock<std::mutex> locker(mu);
      cond.wait(locker, [this]() { return buffer_.size() > 0; });
      Data back = buffer_.front();
      buffer_.pop_front();
      locker.unlock();
      cond.notify_all();
      return back;
    }
  }
  void readAll(std::deque<Data> &target)
  {
    std::unique_lock<std::mutex> locker(mu);
    std::copy(buffer_.begin(), buffer_.end(), std::back_inserter(target));
    buffer_.clear();
    locker.unlock();
  }
  PCQueue() {}

private:
  std::mutex mu;
  std::condition_variable cond;
  std::deque<Data> buffer_;
};

class Message
{
public:
  string name;
  string data;
  string binary;
  Message(string name, string data, string binary) : name(name), data(data), binary(binary) {}
};

class StreamingWorker : public AsyncProgressWorker
{
public:
  StreamingWorker(
      Callback *progress,
      Callback *callback,
      Callback *error_callback)
      : AsyncProgressWorker(callback), progress(progress), error_callback(error_callback)
  {
    input_closed = false;
  }

  ~StreamingWorker()
  {
    delete progress;
    delete error_callback;
  }

  void HandleErrorCallback()
  {
    HandleScope scope;

    v8::Local<v8::Value> argv[] = {
        v8::Exception::Error(New<v8::String>(ErrorMessage()).ToLocalChecked())};

    Nan::AsyncResource async_resource("streaming-worker:error");
    error_callback->Call(1, argv, &async_resource);
  }

  void HandleOKCallback()
  {
    drainQueue();
    Nan::AsyncResource async_resource("streaming-worker:okay");
    callback->Call(0, NULL, &async_resource);
  }

  void HandleProgressCallback(const char *data, size_t size)
  {
    drainQueue();
  }

  void close()
  {
    input_closed = true;
  }

  PCQueue<Message> fromNode;

protected:
  void writeToNode(const AsyncProgressWorker::ExecutionProgress &progress, const Message &msg)
  {
    toNode.write(msg);
    progress.Send(reinterpret_cast<const char *>(&toNode), sizeof(toNode));
  }

  bool closed()
  {
    return input_closed;
  }

  Callback *progress;
  Callback *error_callback;
  PCQueue<Message> toNode;
  bool input_closed;

private:
  void drainQueue()
  {
    HandleScope scope;

    // drain the queue - since we might only get called once for many writes
    std::deque<Message> contents;
    toNode.readAll(contents);

    for (Message &msg : contents)
    {
      auto eventName = New<v8::String>(msg.name.c_str()).ToLocalChecked();
      auto eventMessage = New<v8::String>(msg.data.c_str()).ToLocalChecked();

      if (msg.binary.length() > 0)
      {
        v8::Local<v8::Value> argv[] = {
            eventName,
            eventMessage,
            CopyBuffer(&msg.binary[0], msg.binary.length()).ToLocalChecked()};
        Nan::AsyncResource async_resource("streaming-worker:binary-message");
        progress->Call(3, argv, &async_resource);
      }
      else
      {
        v8::Local<v8::Value> argv[] = {
            eventName,
            eventMessage};
        Nan::AsyncResource async_resource("streaming-worker:message");
        progress->Call(2, argv, &async_resource);
      }
    }
  }
};

StreamingWorker *create_worker(Callback *, Callback *, Callback *, v8::Local<v8::Object> &);

class StreamWorkerWrapper : public Nan::ObjectWrap
{
public:
  static NAN_MODULE_INIT(Init)
  {
    v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
    tpl->SetClassName(Nan::New("StreamingWorker").ToLocalChecked());
    tpl->InstanceTemplate()->SetInternalFieldCount(2);

    // SetPrototypeMethod(tpl, "sendToAddon", sendToAddon);
    SetPrototypeMethod(tpl, "closeInput", closeInput);

    constructor().Reset(Nan::GetFunction(tpl).ToLocalChecked());
    Nan::Set(target, Nan::New("StreamingWorker").ToLocalChecked(),
             Nan::GetFunction(tpl).ToLocalChecked());
  }

private:
  explicit StreamWorkerWrapper(StreamingWorker *worker) : _worker(worker) {}
  ~StreamWorkerWrapper() {}

  static NAN_METHOD(New)
  {
    if (info.IsConstructCall())
    {
      if (info[0]->IsUndefined() || !info[0]->IsFunction())
      {
        ThrowError("No data callback");
        return;
      }

      Callback *data_callback = new Callback(info[0].As<v8::Function>());

      if (info[1]->IsUndefined() || !info[1]->IsFunction())
      {
        ThrowError("No complete callback");
        return;
      }

      Callback *complete_callback = new Callback(info[1].As<v8::Function>());

      if (info[2]->IsUndefined() || !info[2]->IsFunction())
      {
        ThrowError("No error callback");
        return;
      }

      Callback *error_callback = new Callback(info[2].As<v8::Function>());

      v8::Isolate *isolate = info.GetIsolate();
      v8::Local<v8::Object> options;

      if (!info[3]->IsUndefined() && info[3]->IsObject())
      {
        options = info[3].As<v8::Object>();
      }
      else
      {
        options = v8::Object::New(isolate);
      }

      StreamWorkerWrapper *obj = new StreamWorkerWrapper(
          create_worker(
              data_callback,
              complete_callback,
              error_callback, options));

      obj->Wrap(info.This());
      info.GetReturnValue().Set(info.This());

      // start the worker
      AsyncQueueWorker(obj->_worker);
    }
    else
    {
      const int argc = 3;
      v8::Local<v8::Value> argv[argc] = {info[0], info[1], info[2]};
      v8::Local<v8::Function> cons = Nan::New(constructor());
      v8::Local<v8::Object> instance = Nan::NewInstance(cons, argc, argv).ToLocalChecked();
      info.GetReturnValue().Set(instance);
    }
  }

  // static NAN_METHOD(sendToAddon) {
  //   v8::String::Utf8Value name(info[0]->ToString());
  //   v8::String::Utf8Value data(info[1]->ToString());
  //   StreamWorkerWrapper* obj = Nan::ObjectWrap::Unwrap<StreamWorkerWrapper>(info.Holder());
  //   obj->_worker->fromNode.write(Message(*name, *data));
  // }

  static NAN_METHOD(closeInput)
  {
    StreamWorkerWrapper *obj = Nan::ObjectWrap::Unwrap<StreamWorkerWrapper>(info.Holder());
    obj->_worker->close();
  }

  static inline Nan::Persistent<v8::Function> &constructor()
  {
    static Nan::Persistent<v8::Function> my_constructor;
    return my_constructor;
  }

  StreamingWorker *_worker;
};
#endif // ____StreamingWorker__
