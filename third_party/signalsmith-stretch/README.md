Signalsmith Stretch 1.3.2, commit 88061fd4eda56a2353c5d65c74aa3f4ccf76f9da, MIT.
https://github.com/Signalsmith-Audio/signalsmith-stretch

signalsmith-linear (stft.h, fft.h only), commit 7bb1c8f36794d6980080ce817c91f8da8fe56a66, MIT.
https://github.com/Signalsmith-Audio/linear

Both are header-only and unmodified. The stretch header includes the FFT headers as `signalsmith-linear/stft.h`, which is why
they sit in a folder of that name.
