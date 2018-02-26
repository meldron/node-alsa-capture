#include <unistd.h>
#include <string>
#include <sstream>
#include <thread>

#define ALSA_PCM_NEW_HW_PARAMS_API

#include <alsa/asoundlib.h>

#include "node_modules/nan/nan.h"
#include "streaming-worker.h"

class Capture : public StreamingWorker
{
  public:
    Capture(Callback *data, Callback *complete, Callback *error_callback, v8::Local<v8::Object> &options)
        : StreamingWorker(data, complete, error_callback)
    {
        channels = 2;
        device = "default";
        format = SND_PCM_FORMAT_S16_LE;
        period_size = 32;
        period_time = 0;
        rate = 44100;

        error_init = false;
        debug = false;

        if (options->IsObject())
        {
            {
                v8::Local<v8::Value> channels_ = options->Get(New<v8::String>("channels").ToLocalChecked());

                if (!channels_->IsUndefined())
                {
                    bool channels_okay = true;

                    if (channels_->IsNumber())
                    {
                        channels = channels_->NumberValue();
                    }
                    else
                    {
                        channels_okay = false;
                    }

                    if (!channels_okay || channels < 0)
                    {
                        error_init = true;
                        Nan::ThrowError("channels has to be a positive number");
                        return;
                    }
                }
            }

            {
                v8::Local<v8::Value> debug_ = options->Get(New<v8::String>("debug").ToLocalChecked());
                if (!debug_->IsUndefined())
                {
                    if (debug_->IsBoolean())
                    {
                        debug = debug_->BooleanValue();
                    }
                    else
                    {
                        error_init = true;
                        Nan::ThrowError("debug has to be a bool");
                        return;
                    }
                }
            }

            {
                v8::Local<v8::Value> device_ = options->Get(New<v8::String>("device").ToLocalChecked());
                if (!device_->IsUndefined())
                {
                    if (device_->IsString())
                    {
                        v8::String::Utf8Value deviceUTF8(device_);
                        device = std::string(*deviceUTF8);
                    }
                    else
                    {
                        error_init = true;
                        Nan::ThrowError("device has to be a string");
                        return;
                    }
                }
            }

            {
                std::string format_name_;
                v8::Local<v8::Value> format_ = options->Get(New<v8::String>("format").ToLocalChecked());

                if (!format_->IsUndefined())
                {
                    if (format_->IsString())
                    {
                        v8::String::Utf8Value formatUTF8(format_);
                        format_name_ = std::string(*formatUTF8);
                    }
                    else
                    {
                        error_init = true;
                        Nan::ThrowError("format has to be a string");
                        return;
                    }

                    bool found = false;
                    std::string all_formats = "";
                    for (int fn = 0; fn < SND_PCM_FORMAT_LAST; fn++)
                    {
                        auto format_enum = static_cast<_snd_pcm_format>(fn);
                        const char *format_name = snd_pcm_format_name(format_enum);

                        if (!format_name)
                        {
                            continue;
                        }

                        if (strcmp(format_name, format_name_.c_str()) == 0)
                        {
                            found = true;
                            format = format_enum;
                            break;
                        }

                        if (!(!snd_pcm_format_linear(format) &&
                              !(format == SND_PCM_FORMAT_FLOAT_LE ||
                                format == SND_PCM_FORMAT_FLOAT_BE)))
                        {
                            all_formats += std::string(format_name);
                            all_formats += " ";
                        }
                    }

                    if (!found)
                    {
                        error_init = true;
                        auto error_msg = std::string("format not supported; supported: ");
                        error_msg += all_formats;
                        Nan::ThrowError(error_msg.c_str());
                        return;
                    }

                    if (!snd_pcm_format_linear(format) &&
                        !(format == SND_PCM_FORMAT_FLOAT_LE ||
                          format == SND_PCM_FORMAT_FLOAT_BE))
                    {
                        error_init = true;
                        Nan::ThrowError("Invalid (non-linear/float) format");
                        return;
                    }
                }
            }

            {
                v8::Local<v8::Value> period_size_ = options->Get(New<v8::String>("periodSize").ToLocalChecked());

                if (!period_size_->IsUndefined())
                {
                    bool period_size_okay = true;

                    if (period_size_->IsNumber())
                    {
                        period_size = period_size_->NumberValue();
                    }
                    else
                    {
                        period_size_okay = false;
                    }

                    if (!period_size_okay || period_size < 0)
                    {
                        error_init = true;
                        Nan::ThrowError("period_size has to be a positive number");
                        return;
                    }
                }
            }

            {
                v8::Local<v8::Value> period_time_ = options->Get(New<v8::String>("periodTime").ToLocalChecked());

                if (!period_time_->IsUndefined())
                {
                    bool period_time_okay = true;

                    if (period_time_->IsNumber())
                    {
                        period_time = period_time_->NumberValue();
                    }
                    else
                    {
                        period_time_okay = false;
                    }

                    if (!period_time_okay || period_time < 0)
                    {
                        error_init = true;
                        Nan::ThrowError("period_time has to be a positive number");
                        return;
                    }
                }
            }

            {
                v8::Local<v8::Value> rate_ = options->Get(New<v8::String>("rate").ToLocalChecked());

                if (!rate_->IsUndefined())
                {
                    bool rate_okay = true;

                    if (rate_->IsNumber())
                    {
                        rate = rate_->NumberValue();
                    }
                    else
                    {
                        rate_okay = false;
                    }

                    if (!rate_okay || (rate < 4000 || rate > 196000))
                    {
                        error_init = true;
                        Nan::ThrowError("rate has to be a value between 4000 and 196000");
                        return;
                    }
                }
            }
        }

        // int size = (period_size * channels * snd_pcm_format_physical_width(format)) / 8;

        // nothing needs to be here - just make sure you call base constructor
        // The options parameter is for your JavaScript code to pass in
        // an options object.  You can use this for whatever you want.
    }

    ~Capture()
    {
    }

    void Execute(const AsyncProgressWorker::ExecutionProgress &progress)
    {
        int rc;
        int size;
        snd_pcm_t *handle;
        snd_pcm_hw_params_t *params;
        unsigned int val;
        int dir;
        snd_pcm_uframes_t frames;

        if (error_init)
        {
            return;
        }

        rc = snd_pcm_open(&handle, device.c_str(), SND_PCM_STREAM_CAPTURE, 0);

        if (rc < 0)
        {
            std::ostringstream pcmDeviceError;
            pcmDeviceError << "Unable to open PCM device: " << snd_strerror(rc) << "\n";
            SetErrorMessage(pcmDeviceError.str().c_str());
            return;
        }

        /* Allocate a hardware parameters object. */
        snd_pcm_hw_params_alloca(&params);

        /* Fill it in with default values. */
        snd_pcm_hw_params_any(handle, params);

        /* Set the desired hardware parameters. */

        /* Interleaved mode */
        snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);

        /* Signed 16-bit little-endian format */
        snd_pcm_hw_params_set_format(handle, params, format);
        // snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);

        /* Two channels (stereo) */
        snd_pcm_hw_params_set_channels(handle, params, channels);
        // snd_pcm_hw_params_set_channels(handle, params, 2);

        /* 44100 bits/second sampling rate (CD quality) */
        val = static_cast<unsigned int>(rate);
        if (debug)
        {
            fprintf(stderr, "Rate: %d\n", val);
        }
        snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);

        frames = period_size;
        snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);

        auto frames_time = static_cast<unsigned int>(period_time);

        if (frames_time > 0)
        {
            if (debug)
            {
                fprintf(stderr, "Set period time near: %d\n", period_time);
            }
            snd_pcm_hw_params_set_period_time_near(handle, params, &frames_time, &dir);
        }

        /* Write the parameters to the driver */
        rc = snd_pcm_hw_params(handle, params);

        if (rc < 0)
        {
            std::ostringstream hwError;
            hwError << "Unable to set HW parameters: " << snd_strerror(rc) << "\n";
            SetErrorMessage(hwError.str().c_str());
            return;
        }

        // auto sizeOld = frames * 4;
        // fprintf(stderr, "Frames: %d; SizeOld: %d, SizeNew: %d\n", frames, sizeOld, size);

        unsigned int actualRate;
        snd_pcm_hw_params_get_rate(params, &actualRate, &dir);
        if (static_cast<unsigned int>(rate) != actualRate)
        {
            if (debug)
            {
                fprintf(stderr, "Requested rate != actual rate: %d != %u\n", rate, actualRate);
            }
            Message rateDeviating("rateDeviating", std::to_string(actualRate), "");
            writeToNode(progress, rateDeviating);
        }

        snd_pcm_hw_params_get_period_size(params, &frames, &dir);
        if (frames != static_cast<unsigned long>(period_size))
        {
            if (debug)
            {
                fprintf(stderr, "Requested period size != actual period size: %d != %lu\n", period_size, frames);
            }
            Message periodSizeDeviating("periodSizeDeviating", std::to_string(frames), "");
            writeToNode(progress, periodSizeDeviating);
        }

        unsigned int actual_period_time;
        snd_pcm_hw_params_get_period_time(params, &actual_period_time, &dir);
        if (debug)
        {
            fprintf(stderr, "Actual period time: %u\n", actual_period_time);
        }
        Message periodTimeMessage("periodTime", std::to_string(actual_period_time), "");
        writeToNode(progress, periodTimeMessage);

        size = (frames * channels * snd_pcm_format_physical_width(format)) / 8;

        if (debug)
        {
            fprintf(stderr, "Actual rate: %d\n", actualRate);
            fprintf(stderr, "Buffer size: %d\n", size);
        }

        char buffer[size];
        while (!closed())
        {
            rc = snd_pcm_readi(handle, buffer, frames);
            if (rc == -EPIPE)
            {
                /* EPIPE means overrun */
                if (debug)
                {
                    fprintf(stderr, "overrun occurred\n");
                }

                Message overrun("overrun", "overrun occurred", "");
                writeToNode(progress, overrun);

                snd_pcm_prepare(handle);
            }
            else if (rc < 0)
            {
                if (debug)
                {
                    fprintf(stderr, "Error from read: %s\n", snd_strerror(rc));
                }

                Message readError("readError", std::string(snd_strerror(rc)), "");
                writeToNode(progress, readError);
            }
            else if (rc != (int)frames)
            {
                if (debug)
                {
                    fprintf(stderr, "Short read, read %d frames\n", rc);
                }

                Message shortRead("shortRead", std::to_string(rc), "");
                writeToNode(progress, shortRead);
            }

            string bufferStringRaw(buffer, size);
            Message tosend("audio", "", bufferStringRaw);
            writeToNode(progress, tosend);
        }

        snd_pcm_drain(handle);
        snd_pcm_close(handle);
    }

  private:
    int channels;
    std::string device;
    _snd_pcm_format format;
    int period_size;
    int period_time;
    int rate;
    bool error_init;
    bool debug;
};

StreamingWorker *create_worker(Callback *data, Callback *complete, Callback *error_callback, v8::Local<v8::Object> &options)
{
    return new Capture(data, complete, error_callback, options);
}

NODE_MODULE(capture, StreamWorkerWrapper::Init)