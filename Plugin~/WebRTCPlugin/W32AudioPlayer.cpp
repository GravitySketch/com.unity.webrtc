#include "pch.h"
#include "W32AudioPlayer.h"

#ifndef WIN32
#  error "Win32 audio player can only be used on windows."
#endif

#include <combaseapi.h>
#include <errhandlingapi.h>
#include <audioclient.h>
#include <mmdeviceapi.h>

W32AudioPlayer::W32AudioPlayer() {
}

bool W32AudioPlayer::IsInitialized() const {
  return _numberOfChannels != 0;
}

bool W32AudioPlayer::Init(
  const size_t channels,
  const int bitsPerSample,
  const int samplesPerSecond) {
  _numberOfChannels = channels;
  _bitsPerSample = bitsPerSample;
  _sampleRate = samplesPerSecond;

  IMMDeviceEnumerator *deviceEnumerator;
  if (S_OK != CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                               CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                               reinterpret_cast<LPVOID *>(&deviceEnumerator))) {
    // TODO error message
    return false;
  }

  IMMDevice *audioDevice;
  if (S_OK != deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole,
                                                        &audioDevice)) {
    // TODO error message
    return false;
  }
  deviceEnumerator->Release();

  if (S_OK != audioDevice->Activate(__uuidof(IAudioClient2), CLSCTX_ALL,
                                    nullptr,
                                    reinterpret_cast<LPVOID *>(&_audioClient))) {
    // TODO error message
    return false;
  }
  audioDevice->Release();

  WAVEFORMATEX* defaultMixFormat = nullptr;
  _audioClient->GetMixFormat(&defaultMixFormat);
  if (nullptr == defaultMixFormat) {
    // TODO error message
    return false;
  }

  WAVEFORMATEX mixFormat = *defaultMixFormat;
  mixFormat.wFormatTag = WAVE_FORMAT_PCM;
  mixFormat.nChannels = channels;
  mixFormat.nSamplesPerSec = samplesPerSecond; //defaultMixFormat->nSamplesPerSec;
  mixFormat.wBitsPerSample = bitsPerSample;
  mixFormat.nBlockAlign = (mixFormat.nChannels * mixFormat.wBitsPerSample) / 8;
  mixFormat.nAvgBytesPerSec = mixFormat.nSamplesPerSec * mixFormat.nBlockAlign;

  const int64_t REFTIMES_PER_SEC = 10000000; // hundred nanoseconds
  const REFERENCE_TIME requestedSoundBufferDuration = REFTIMES_PER_SEC * 2;
  const DWORD initStreamFlags = AUDCLNT_STREAMFLAGS_RATEADJUST |
                                AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                                AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
  if (S_OK != _audioClient->Initialize(
                  AUDCLNT_SHAREMODE_SHARED,
                  initStreamFlags,
                  requestedSoundBufferDuration,
                  0,
                  &mixFormat,
                  nullptr)) {
    // TODO error message
    return false;
  }

  if (S_OK != _audioClient->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<LPVOID*>(&_audioRenderClient))) {
    // TODO error message
    return false;
  }

  if (S_OK != _audioClient->GetBufferSize(&_bufferSizeInFrames)) {
    // TODO error message
    return false;
  }

  if (S_OK != _audioClient->Start()) {
    // TODO error message
    return false;
  }
}

W32AudioPlayer::~W32AudioPlayer() {
    if (_audioClient != nullptr) {
        _audioClient->Stop();
        _audioClient->Release();
    }
    if (_audioRenderClient != nullptr) {
        _audioRenderClient->Release();
    }
}

// We get this from an AudioSinkInterface (probably a RemoteAudioSource) called
// via ChannelReceive::GetAudioFrameWithInfo which calls us _synchronously_.
//
// An audio frame goes through the webrtc pipeline as follows:
//   => AudioIngress::GetAudioFrameWithInfo
//   => AudioReceiveStream::GetAudioFrameWithInfo
//   => ChannelReceive::GetAudioFrameWithInfo
//
//   AudioMixerImpl::GetAudioFromSources(
//   =>
//
//      RtpTransport::DemuxPacket
//   => RtpDemuxer::OnRtpPacket
//   => ChannelReceive::OnRtpPacket
//   => ChannelReceive::ReceivePacket
void W32AudioPlayer::PlayAudioFrame(
    const void* audioData,
    const int bitsPerSample,
    const int sampleRate,
    const size_t numberOfChannels,
    const size_t numberOfFrames) {
  if (numberOfChannels != _numberOfChannels) {
    // FIXME emit error and fail or something?
  }
  if (bitsPerSample != _bitsPerSample) {
    // FIXME emit error and fail or something?
  }
  if (sampleRate != _sampleRate) {
    // FIXME emit error and fail or something?
  }

  UINT32 bufferPadding;
  if (S_OK != _audioClient->GetCurrentPadding(&bufferPadding)) {
    // FIXME do something?
  }

  size_t sampleCount = numberOfChannels * numberOfFrames;
  if (bufferPadding > sampleCount) {
    // FIXME emit an error about dropping some data.
    const auto numberOfSamplesToDrop = sampleCount - bufferPadding;
    audioData = reinterpret_cast<const void*>(reinterpret_cast<const char*>(audioData) + numberOfSamplesToDrop);
    sampleCount = bufferPadding;
  }

  unsigned char *renderBuffer;
  if (S_OK != _audioRenderClient->GetBuffer(sampleCount, &renderBuffer)) {
    // FIXME emit an error and do something
  }

  memcpy(renderBuffer, audioData, sampleCount);

  if (S_OK != _audioRenderClient->ReleaseBuffer(sampleCount, 0)) {
    // FIXME emit an error and do something
  }
}
