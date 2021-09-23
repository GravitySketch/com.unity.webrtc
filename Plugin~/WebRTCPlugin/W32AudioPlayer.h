#pragma once

#include <cstdint>

struct IAudioClient2;
struct IAudioRenderClient;

class W32AudioPlayer {
public:
    W32AudioPlayer();
    ~W32AudioPlayer();

    bool Init(
        const size_t channels = 2,
        const int bits_per_sample = 16,
        const int sample_rate = 44100);

    void PlayAudioFrame(
        const void *audioData,
        const int bitsPerSample,
        const int sampleRate,
        const size_t numberOfChannels,
        const size_t numberOfFrames);

    bool IsInitialized() const;

private:
    IAudioClient2 *_audioClient = nullptr;
    IAudioRenderClient *_audioRenderClient = nullptr;
    uint32_t _bufferSizeInFrames = 0;
    int _bitsPerSample = 0;
    int _sampleRate = 0;
    size_t _numberOfChannels = 0;
};
