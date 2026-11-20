# OpenJOC Android / Media3 integration

## Scope

This document records the integration boundary for the NextLib Android adapter. It is not a claim that OpenJOC playback is already implemented.

OpenJOC is an optional E-AC-3 JOC spatial decoder. It must not replace the existing FFmpeg decoder for ordinary E-AC-3 or unrelated codecs. The current NextLib FfmpegAudioRenderer remains the fallback path.

## Confirmed upstream contract

The adapter targets the versioned C ABI in OpenJOC crates/openjoc-capi/include/openjoc.h:

- ABI version 1.7; use openjoc_decoder_config_init_v1_7 rather than hand-initializing the configuration structure.
- Use openjoc_stream_decoder_create and openjoc_stream_decoder_send_chunk: Media3 input buffers are not guaranteed to align one-to-one with E-AC-3 access units.
- Drain all available output using openjoc_stream_decoder_receive_frame until NEED_MORE_INPUT; only FRAME_AVAILABLE carries PCM.
- Treat NOT_JOC as a signal to fall back to the ordinary E-AC-3 decoder, not as silence or a fatal playback error.
- Call openjoc_stream_decoder_flush on seek/discontinuity and openjoc_stream_decoder_drain at end of stream. Destroy the handle exactly once.
- Output is packed interleaved float32. Read the frame sample rate, channel count, sample count and layout from each returned frame; do not assume stereo or a fixed channel layout.
- A frame's data pointer is borrowed from the decoder and must be copied before the next receive call.

## Media3 adapter requirements

1. Add a dedicated OpenJOC audio renderer ahead of the FFmpeg renderer only for JOC-capable E-AC-3 tracks.
2. Advertise support only when the Android ABI has a loadable OpenJOC native library and the output AudioSink accepts the renderer's actual output format.
3. Keep encrypted/DRM tracks out of this path unless a supported secure-input design is implemented.
4. Keep ordinary E-AC-3 and all other codecs on the existing MediaCodec/FFmpeg paths.
5. Preserve Media3 decoder lifecycle semantics: queued input, output-buffer ownership, reset/flush, EOS drain, and renderer release.
6. If OpenJOC reports NOT_JOC before emitting PCM, hand the stream to the existing E-AC-3 fallback without losing compressed bytes. Do not switch after partial OpenJOC PCM has been emitted.
7. Report the active backend distinctly in decoder analytics; do not label OpenJOC as FFmpeg.

## Native packaging gate

The C API crate builds as rlib, staticlib and cdylib, and its default openjoc-ffmpeg dependency does not enable the optional external-FFmpeg feature. Android packaging still needs an explicit, reproducible Rust cross-build for every supported ABI (currently x86, x86_64, armeabi-v7a and arm64-v8a), plus a defined way to package/load the resulting library. This must be verified with the project's NDK and OpenJOC's Rust MSRV before enabling the renderer by default.

Do not add a fake JNI implementation or an always-visible settings switch before the native artifact, ABI loading, and playback fallback have been tested.

## Spatial settings status

The C ABI exposes speaker, stereo and binaural modes, SOFA data, SADIE D1/D2 HRTF selection and experimental listener-orientation operations. These are not yet exposed by NextLib. Each setting must be surfaced only after the corresponding mode is implemented and validated on Android hardware. ADM export is out of the initial Media3 playback scope; OpenJOC documents it as reconstructed/scope-limited rather than original authored ADM preservation.

## Required verification before release

- Rust C ABI tests and Android cross-build for all shipped ABIs.
- Instrumented Android tests for JOC admission, non-JOC fallback, malformed input, encrypted input rejection, format changes, seek/flush, EOS drain and renderer release.
- Real-device playback checks for stereo and multichannel output, with AudioSink format negotiation.
- Verify that no compressed bytes are lost during a non-JOC fallback and no stale PCM is emitted after seek.
