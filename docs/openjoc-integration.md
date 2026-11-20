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

The C API crate builds as rlib, staticlib and cdylib, and its default openjoc-ffmpeg dependency does not enable the optional external-FFmpeg feature. The native workflow now stages the four ABI libraries in Android jniLibs layout and uploads them as the openjoc-android-jniLibs artifact. The consuming app must package that artifact under media3ext/src/main/jniLibs before OpenJocNative.isAvailable() can return true. The JNI bridge itself is compiled as part of media3ext; packaging and device loading still need verification.

The low-level JNI bridge now exposes stream create/send/receive/drain/flush/destroy operations and the ABI 1.3 decode-free classifier (create/classify/finish/reset/destroy). PCM is copied from the C ABI's borrowed frame storage into a caller-owned direct buffer. The dynamic loader validates the complete required symbol set, including classifier functions, on both first and subsequent availability checks.

The classifier is deliberately separate from the stream decoder. A Media3 adapter must retain every original compressed input chunk, with its timestamp and flags, while classification is UNKNOWN. Once the first complete access unit is positively classified, replay the retained chunks in original order to either OpenJOC or the ordinary E-AC-3 decoder. The classifier's own bounded probe storage is not a replacement for this replay queue: it does not return the bytes it inspected. `OpenJocAdmissionBuffer` now provides a bounded, timestamp/flags-preserving chunk-copy primitive (3,538,944-byte cap, derived from the pinned parser's 16-AU staging bound), with unit tests for ordering, ByteBuffer positions and overflow rejection. It is not yet wired into a Media3 decoder. If classification is invalid/unsupported or remains UNKNOWN at EOS, do not silently discard the retained input. The eventual decoder must define a deterministic error/fallback policy. This JNI layer is not yet a Media3 Decoder/Renderer and does not implement that queue or automatic non-JOC fallback.

## Spatial settings status

The C ABI exposes speaker, stereo and binaural modes, SOFA data, SADIE D1/D2 HRTF selection and experimental listener-orientation operations. These are not yet exposed by NextLib. Each setting must be surfaced only after the corresponding mode is implemented and validated on Android hardware. ADM export is out of the initial Media3 playback scope; OpenJOC documents it as reconstructed/scope-limited rather than original authored ADM preservation.

## Required verification before release

- Rust C ABI tests and Android cross-build for all shipped ABIs.
- Instrumented Android tests for JOC admission, non-JOC fallback, malformed input, encrypted input rejection, format changes, seek/flush, EOS drain and renderer release.
- Real-device playback checks for stereo and multichannel output, with AudioSink format negotiation.
- Verify that no compressed bytes are lost during a non-JOC fallback and no stale PCM is emitted after seek.
