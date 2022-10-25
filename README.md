# node-alsa-capture

[![NPM](https://nodei.co/npm/alsa-capture.png)](https://www.npmjs.com/package/alsa-capture)

Node.js module to record PCM audio data from ALSA capture devices (e.g., microphones).

Emits events about **overruns**, **short reads**, deviating **sample rates** or **period sizes**.

## Compatibility

Tested (module compiles and records audio) with the following Node versions:

-   `v6.17.1 (lts/boron)`
-   `v8.9.4`
-   `v8.11.3`
-   `v8.17.0 (lts/carbon)`
-   `v10.22.1 (lts/dubnium)`
-   `v12.14.1`
-   `v12.18.4 (lts/erbium)`
-   `v14.16.0 (lts/fermium)`
-   `v15.11.0`
-   `v16.13.0 (lts/gallium)`
-   `v17.1.0`
-   `v18.11.0` (successfully built, but audio recording not tested)
-   `v19.0.0 (current)`

## Dependencies

To build native Node.js modules, this projects uses [node-gyp](https://github.com/nodejs/node-gyp), which requires[[1]](https://github.com/nodejs/node-gyp#on-unix):

-   Python v3.6, v3.7, v3.8, or v3.9
-   make
-   A proper C/C++ compiler toolchain, like GCC (must support 'c++17')
    )

This module also requires a `libasound2-dev` for `<alsa/asoundlib.h>` and `libasound2` to link against `libasound.so.2`.

On debian derivatives system you can install all dependencies with `apt`:

```bash
sudo apt install libasound2 libasound2-dev
```

## Install

`npm install alsa-capture`

or

`yarn add alsa-capture`

## Manual build

Run `npm install`, which will execute a `node-gyp rebuild` and build`./build/Release/capture.node`. The `capture.node` file is needed if you want to distribute the package in binary form (`libasound.so.2` is still needed).

## Usage

```javascript
const AlsaCapture = require("alsa-capture");

// default values for the options object
const captureInstance = new AlsaCapture({
    channels: 2,
    debug: false,
    device: "default",
    format: "S16_LE",
    periodSize: 32,
    periodTime: undefined,
    rate: 44100,
});

// data is an Uint8Array
// Buffer size = numChannels * formatByteSize * periodSize
// Example: 2 Bytes (AlsaFormat.S16_LE) * 2 (numChannels) * 32 (periodSize) = 128 Bytes
captureInstance.on("audio", (data) => {
    console.log(data);
});

// if the requested rate is not available for the capture device
// ALSA will select the nearest available
captureInstance.on("rateDeviating", (actualRate) => {
    console.warn(`Sound card rate deviates from requested rate; Actual rate: ${actualRate}`);
});

// if the requested period size is not available for the capture device
// ALSA will select the nearest available
captureInstance.on("periodSizeDeviating", (actualPeriodSize) => {
    console.warn(`Sound card period size deviates from requested period size; Actual period size: ${actualPeriodSize}`);
});

captureInstance.on("periodTime", (periodTime) => {
    console.log(`Actual period time ${periodTime}`);
});

// if an error occurs, error will contain the message
captureInstance.on("error", (error) => {
    console.error(error);
});

// if overruns occur frequently try increasing the period_size
captureInstance.on("overrun", () => {
    console.warn("NodeAlsaCapture overrun");
});

captureInstance.on("shortRead", (framesRead) => {
    console.warn(`NodeAlsaCapture shortRead: Only ${framesRead} read`);
});

captureInstance.on("readError", (error) => {
    console.warn(`NodeAlsaCapture readError: ${error}`);
});

setTimeout(() => {
    // close capture device
    captureInstance.close();
}, 10000);

// event is sent after capture devices is closed completely
captureInstance.on("close", () => {
    console.log("capture closed");
});
```

## API

### `new AlsaCapture({opts}?): AlsaCaptureInstance`

Creates a new ALSA capture instance which extends [eventemitter3](https://github.com/primus/eventemitter3) and emits several kinds of events including the PCM audio data. The optional `opts` object sets the ALSA hardware parameters.

| option     | type    | description                                                         | default      |
| ---------- | ------- | ------------------------------------------------------------------- | ------------ |
| channels   | number  | select number of channels to capture                                | 2            |
| debug      | boolean | prints debug data to stderr                                         | false        |
| device     | string  | ALSA device ID                                                      | default      |
| format     | string  | Sample format (see Supported Sample formats)                        | S16_LE       |
| periodSize | number  | A period is the number of frames in between each hardware interrupt | 32           |
| periodTime | number  | Set period time near _n_ us.                                        | (no default) |
| rate       | number  | Sample rate (400 <= rate <= 196000)                                 | 44100        |

Note: `snd_pcm_hw_params_set_period_time_near` will only be called if the `opts` object has the `periodTime` property.

Note: [snd_pcm_hw_params_set_access](https://www.alsa-project.org/alsa-doc/alsa-lib/group___p_c_m___h_w___params.html#ga4c8f1c632931923531ca68ee048a8de8) is set to [SND_PCM_ACCESS_RW_INTERLEAVED](https://www.alsa-project.org/alsa-doc/alsa-lib/group___p_c_m.html#ga661221ba5e8f1d6eaf4ab8e2da57cc1a).

### `close()`

Stops the ALSA capture thread. Afterwards the `close` event will be emitted.

### Events

#### `.on("audio", (data: Uint8Array) => {})`

Returns the PCM data in an `Uint8Array`.

The buffer size is derived from the number of channels, the sample format and the period size:

`bufferSize = numChannels * formatByteSize * periodSize`

#### `.on("close", () => {})`

Capture instance closed.

#### `.on("rateDeviating", (actualRate: Number) => {})`

If the requested sample rate is not available for the capture device ALSA will select the nearest available

#### `.on("periodSizeDeviating", (actualPeriodSize: Number) => {})`

If the requested period size is not available for the capture device ALSA will select the nearest available

#### `.on("periodTime", (periodTime: Number) => {})`

The actual period time

#### `.on("error", (error: String) => {})`

Exceptions in Instance creation. See `error` messages.

#### `.on("overrun", () => {})`

An ALSA Buffer overrun occurred.

#### `.on("shortRead", (framesRead: Number) => {})`

ALSA could not capture enough frames, only `framesRead` frames.

#### `.on("readError", (readError: String) => {})`

Read error message. See [`snd_strerror`](https://github.com/michaelwu/alsa-lib/blob/master/src/error.c#L51).

## Selecting devices

`arecord -l` lists all soundcards and digital audio devices:

```
card 0: PCH [HDA Intel PCH], device 0: CX20590 Analog [CX20590 Analog]
  Subdevices: 1/1
  Subdevice #0: subdevice #0
card 1: CODEC [USB Audio CODEC], device 0: USB Audio [USB Audio]
  Subdevices: 1/1
  Subdevice #0: subdevice #0
```

To select **card 1** use `hw:1,0` or if you want to enable the ALSA plugin layer (ALSA will automatically convert the data to a format supported by your sound card [<sup>1</sup>](https://www-uxsup.csx.cam.ac.uk/pub/doc/suse/suse9.0/userguide-9.0/ch18.html)) use `plughw:1,0`.

`arecord -L` lists all list device names

```
default
    Playback/recording through the PulseAudio sound server
null
    Discard all samples (playback) or generate zero samples (capture)
pulse
    PulseAudio Sound Server
sysdefault:CARD=PCH
    HDA Intel PCH, CX20590 Analog
    Default Audio Device
front:CARD=PCH,DEV=0
    HDA Intel PCH, CX20590 Analog
    Front speakers
hw:CARD=PCH,DEV=0
    HDA Intel PCH, CX20590 Analog
    Direct hardware device without any conversions
plughw:CARD=PCH,DEV=0
    HDA Intel PCH, CX20590 Analog
hw:CARD=CODEC,DEV=0
    USB Audio CODEC, USB Audio
    Direct hardware device without any conversions
plughw:CARD=CODEC,DEV=0
    USB Audio CODEC, USB Audio
    Hardware device with all software conversions
```

So you could also use `hw:CARD=CODEC,DEV=0` to write to this external USB audio card or use `pulse` and use pulse audio settings.

## Supported Sample formats

-   S8
-   U8
-   S16_LE
-   S16_BE
-   U16_LE
-   U16_BE
-   S24_LE
-   S24_BE
-   U24_LE
-   U24_BE
-   S32_LE
-   S32_BE
-   U32_LE
-   U32_BE
-   FLOAT_LE
-   FLOAT_BE
-   FLOAT64_LE
-   FLOAT64_BE
-   IEC958_SUBFRAME_LE
-   IEC958_SUBFRAME_BE
-   MU_LAW
-   A_LAW
-   IMA_ADPCM
-   MPEG
-   GSM
-   SPECIAL
-   S24_3LE
-   S24_3BE
-   U24_3LE
-   U24_3BE
-   S20_3LE
-   S20_3BE
-   U20_3LE
-   U20_3BE
-   S18_3LE
-   S18_3BE
-   U18_3LE
-   U18_3BE
-   G723_24
-   G723_24_1B
-   G723_40
-   G723_40_1B
-   DSD_U8
-   DSD_U16_LE
-   DSD_U32_LE
-   DSD_U16_BE

## License

MIT

Copyright (c) 2020, 2021, 2022 Bernd Kaiser

## Acknowledgements

-   The ALSA c code examples
-   The [streaming worker](https://github.com/freezer333/streaming-worker) was originally written by [Scott Frees](https://pages.ramapo.edu/~sfrees/).
