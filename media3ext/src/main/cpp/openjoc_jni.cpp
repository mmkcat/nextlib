#include <jni.h>
#include <android/log.h>
#include <dlfcn.h>
#include <cstring>
#include <cstdint>
#include <mutex>
#include "openjoc/include/openjoc.h"

#define JOC_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "OpenJocNative", __VA_ARGS__)

namespace {
struct OpenJocApi {
    std::mutex mutex;
    void *library = nullptr;
    decltype(&openjoc_decoder_config_init_v1_7) config_init = nullptr;
    decltype(&openjoc_stream_decoder_create) create = nullptr;
    decltype(&openjoc_stream_decoder_send_chunk) send_chunk = nullptr;
    decltype(&openjoc_stream_decoder_receive_frame) receive_frame = nullptr;
    decltype(&openjoc_stream_decoder_drain) drain = nullptr;
    decltype(&openjoc_stream_decoder_flush) flush = nullptr;
    decltype(&openjoc_stream_decoder_destroy) destroy = nullptr;
    decltype(&openjoc_pcm_frame_init) frame_init = nullptr;
    decltype(&openjoc_classifier_create) classifier_create = nullptr;
    decltype(&openjoc_classifier_send_chunk) classifier_send_chunk = nullptr;
    decltype(&openjoc_classifier_finish) classifier_finish = nullptr;
    decltype(&openjoc_classifier_reset) classifier_reset = nullptr;
    decltype(&openjoc_classifier_destroy) classifier_destroy = nullptr;

    bool load() {
        std::lock_guard<std::mutex> guard(mutex);
        if (library) return create && send_chunk && receive_frame && drain && flush && destroy;
        library = dlopen("libopenjoc_capi.so", RTLD_NOW | RTLD_LOCAL);
        if (!library) return false;
#define LOAD(member, symbol) member = reinterpret_cast<decltype(member)>(dlsym(library, symbol))
        LOAD(config_init, "openjoc_decoder_config_init_v1_7");
        LOAD(create, "openjoc_stream_decoder_create");
        LOAD(send_chunk, "openjoc_stream_decoder_send_chunk");
        LOAD(receive_frame, "openjoc_stream_decoder_receive_frame");
        LOAD(drain, "openjoc_stream_decoder_drain");
        LOAD(flush, "openjoc_stream_decoder_flush");
        LOAD(destroy, "openjoc_stream_decoder_destroy");
        LOAD(frame_init, "openjoc_pcm_frame_init");
        LOAD(classifier_create, "openjoc_classifier_create");
        LOAD(classifier_send_chunk, "openjoc_classifier_send_chunk");
        LOAD(classifier_finish, "openjoc_classifier_finish");
        LOAD(classifier_reset, "openjoc_classifier_reset");
        LOAD(classifier_destroy, "openjoc_classifier_destroy");
#undef LOAD
        return config_init && create && send_chunk && receive_frame && drain &&
               flush && destroy && frame_init && classifier_create &&
               classifier_send_chunk && classifier_finish && classifier_reset && classifier_destroy;
    }
};

OpenJocApi api;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_github_anilbeesetti_nextlib_media3ext_ffdecoder_OpenJocNative_nativeIsAvailable(
        JNIEnv *, jclass) {
    return api.load() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jlong JNICALL
Java_io_github_anilbeesetti_nextlib_media3ext_ffdecoder_OpenJocNative_nativeCreate(
        JNIEnv *, jclass, jint renderMode) {
    if (!api.load()) return 0;
    openjoc_decoder_config config{};
    if (api.config_init(&config) != OPENJOC_STATUS_OK) return 0;
    config.render_mode = static_cast<uint32_t>(renderMode);
    openjoc_stream_decoder *decoder = nullptr;
    const openjoc_status status = api.create(&config, &decoder);
    if (status != OPENJOC_STATUS_OK || !decoder) {
        JOC_LOGE("OpenJOC stream decoder creation failed: %d", status);
        return 0;
    }
    return reinterpret_cast<jlong>(decoder);
}

extern "C" JNIEXPORT jint JNICALL
Java_io_github_anilbeesetti_nextlib_media3ext_ffdecoder_OpenJocNative_nativeSendChunk(
        JNIEnv *env, jclass, jlong handle, jobject input, jint length, jboolean discontinuity) {
    if (!api.load() || handle == 0 || input == nullptr || length < 0) {
        return OPENJOC_STATUS_INVALID_ARGUMENT;
    }
    auto *data = static_cast<const uint8_t *>(env->GetDirectBufferAddress(input));
    const jlong capacity = env->GetDirectBufferCapacity(input);
    if (!data || capacity < length) return OPENJOC_STATUS_INVALID_ARGUMENT;
    const uint32_t flags = discontinuity ? OPENJOC_PACKET_FLAG_DISCONTINUITY : 0;
    return api.send_chunk(reinterpret_cast<openjoc_stream_decoder *>(handle),
                          data, static_cast<size_t>(length), OPENJOC_NO_PTS, flags);
}

extern "C" JNIEXPORT jint JNICALL
Java_io_github_anilbeesetti_nextlib_media3ext_ffdecoder_OpenJocNative_nativeReceiveFrame(
        JNIEnv *env, jclass, jlong handle, jobject output, jintArray info) {
    if (!api.load() || handle == 0 || output == nullptr || info == nullptr ||
        env->GetArrayLength(info) < 3) {
        return OPENJOC_STATUS_INVALID_ARGUMENT;
    }
    auto *destination = static_cast<uint8_t *>(env->GetDirectBufferAddress(output));
    const jlong capacity = env->GetDirectBufferCapacity(output);
    if (!destination || capacity < 0) return OPENJOC_STATUS_INVALID_ARGUMENT;

    openjoc_pcm_frame frame{};
    if (api.frame_init(&frame) != OPENJOC_STATUS_OK) return OPENJOC_STATUS_INVALID_ARGUMENT;
    const openjoc_status status =
        api.receive_frame(reinterpret_cast<openjoc_stream_decoder *>(handle), &frame);
    if (status != OPENJOC_STATUS_FRAME_AVAILABLE) return status;
    if (frame.data == nullptr || frame.data_len % sizeof(float) != 0) {
        return OPENJOC_STATUS_DECODE_ERROR;
    }
    if (frame.data_len > static_cast<size_t>(capacity)) return OPENJOC_STATUS_OUT_OF_MEMORY;

    std::memcpy(destination, frame.data, frame.data_len);
    jint values[3] = {
        static_cast<jint>(frame.sample_rate),
        static_cast<jint>(frame.channel_count),
        static_cast<jint>(frame.data_len)
    };
    env->SetIntArrayRegion(info, 0, 3, values);
    return OPENJOC_STATUS_FRAME_AVAILABLE;
}

extern "C" JNIEXPORT jint JNICALL
Java_io_github_anilbeesetti_nextlib_media3ext_ffdecoder_OpenJocNative_nativeDrain(
        JNIEnv *, jclass, jlong handle) {
    if (!api.load() || handle == 0) return OPENJOC_STATUS_INVALID_ARGUMENT;
    return api.drain(reinterpret_cast<openjoc_stream_decoder *>(handle));
}

extern "C" JNIEXPORT jint JNICALL
Java_io_github_anilbeesetti_nextlib_media3ext_ffdecoder_OpenJocNative_nativeFlush(
        JNIEnv *, jclass, jlong handle) {
    if (!api.load() || handle == 0) return OPENJOC_STATUS_INVALID_ARGUMENT;
    return api.flush(reinterpret_cast<openjoc_stream_decoder *>(handle));
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_anilbeesetti_nextlib_media3ext_ffdecoder_OpenJocNative_nativeDestroy(
        JNIEnv *, jclass, jlong handle) {
    if (api.load() && handle != 0) {
        api.destroy(reinterpret_cast<openjoc_stream_decoder *>(handle));
    }
}

extern "C" JNIEXPORT jlong JNICALL
Java_io_github_anilbeesetti_nextlib_media3ext_ffdecoder_OpenJocNative_nativeCreateClassifier(
        JNIEnv *, jclass) {
    if (!api.load()) return 0;
    openjoc_classifier *classifier = nullptr;
    const openjoc_status status = api.classifier_create(&classifier);
    if (status != OPENJOC_STATUS_OK || !classifier) {
        JOC_LOGE("OpenJOC classifier creation failed: %d", status);
        return 0;
    }
    return reinterpret_cast<jlong>(classifier);
}

extern "C" JNIEXPORT jint JNICALL
Java_io_github_anilbeesetti_nextlib_media3ext_ffdecoder_OpenJocNative_nativeClassifyChunk(
        JNIEnv *env, jclass, jlong handle, jobject input, jint length, jintArray classification) {
    if (!api.load() || handle == 0 || input == nullptr || classification == nullptr ||
        env->GetArrayLength(classification) < 1 || length < 0) {
        return OPENJOC_STATUS_INVALID_ARGUMENT;
    }
    auto *data = static_cast<const uint8_t *>(env->GetDirectBufferAddress(input));
    const jlong capacity = env->GetDirectBufferCapacity(input);
    if (!data || capacity < length) return OPENJOC_STATUS_INVALID_ARGUMENT;
    openjoc_classification result = OPENJOC_CLASSIFICATION_UNKNOWN;
    const openjoc_status status = api.classifier_send_chunk(
        reinterpret_cast<openjoc_classifier *>(handle), data, static_cast<size_t>(length), &result);
    jint value = static_cast<jint>(result);
    env->SetIntArrayRegion(classification, 0, 1, &value);
    return status;
}

extern "C" JNIEXPORT jint JNICALL
Java_io_github_anilbeesetti_nextlib_media3ext_ffdecoder_OpenJocNative_nativeFinishClassification(
        JNIEnv *env, jclass, jlong handle, jintArray classification) {
    if (!api.load() || handle == 0 || classification == nullptr ||
        env->GetArrayLength(classification) < 1) return OPENJOC_STATUS_INVALID_ARGUMENT;
    openjoc_classification result = OPENJOC_CLASSIFICATION_UNKNOWN;
    const openjoc_status status = api.classifier_finish(
        reinterpret_cast<openjoc_classifier *>(handle), &result);
    jint value = static_cast<jint>(result);
    env->SetIntArrayRegion(classification, 0, 1, &value);
    return status;
}

extern "C" JNIEXPORT jint JNICALL
Java_io_github_anilbeesetti_nextlib_media3ext_ffdecoder_OpenJocNative_nativeResetClassifier(
        JNIEnv *, jclass, jlong handle) {
    if (!api.load() || handle == 0) return OPENJOC_STATUS_INVALID_ARGUMENT;
    return api.classifier_reset(reinterpret_cast<openjoc_classifier *>(handle));
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_anilbeesetti_nextlib_media3ext_ffdecoder_OpenJocNative_nativeDestroyClassifier(
        JNIEnv *, jclass, jlong handle) {
    if (api.load() && handle != 0) {
        api.classifier_destroy(reinterpret_cast<openjoc_classifier *>(handle));
    }
}
