const AlsaCapture = require("./index");

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
    process.exit(1);
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
    console.log("capture closed")
});
