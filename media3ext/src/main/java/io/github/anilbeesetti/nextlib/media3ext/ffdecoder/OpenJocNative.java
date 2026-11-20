package io.github.anilbeesetti.nextlib.media3ext.ffdecoder;

import java.nio.ByteBuffer;

/**
 * Low-level bridge to the OpenJOC stream C API.
 *
 * The OpenJOC shared library is packaged separately for each Android ABI. This class deliberately
 * exposes stream operations rather than assuming that one Media3 input buffer is one access unit.
 */
final class OpenJocNative {
  static final int STATUS_OK = 0;
  static final int STATUS_NEED_MORE_INPUT = 1;
  static final int STATUS_FRAME_AVAILABLE = 2;
  static final int STATUS_END_OF_STREAM = 3;
  static final int STATUS_NOT_JOC = 11;

  static {
    System.loadLibrary("media3ext");
    try {
      System.loadLibrary("openjoc_capi");
    } catch (UnsatisfiedLinkError ignored) {
      // Keep the player usable on builds where the optional OpenJOC ABI artifact was not packaged.
    }
  }

  private OpenJocNative() {}

  static boolean isAvailable() {
    return nativeIsAvailable();
  }

  static long create(int renderMode) {
    return nativeCreate(renderMode);
  }

  static int sendChunk(long decoder, ByteBuffer input, int length, boolean discontinuity) {
    ByteBuffer chunk = input.slice();
    if (length < 0 || length > chunk.remaining()) {
      return 6; // OPENJOC_STATUS_INVALID_ARGUMENT
    }
    return nativeSendChunk(decoder, chunk, length, discontinuity);
  }

  /**
   * Copies one received frame into output. info must have room for [sampleRate, channels, bytes].
   * The C API frame pointer is borrowed, so native code copies it before returning.
   */
  static int receiveFrame(long decoder, ByteBuffer output, int[] info) {
    return nativeReceiveFrame(decoder, output.slice(), info);
  }

  static int drain(long decoder) {
    return nativeDrain(decoder);
  }

  static int flush(long decoder) {
    return nativeFlush(decoder);
  }

  static void destroy(long decoder) {
    nativeDestroy(decoder);
  }

  private static native boolean nativeIsAvailable();
  private static native long nativeCreate(int renderMode);
  private static native int nativeSendChunk(
      long decoder, ByteBuffer input, int length, boolean discontinuity);
  private static native int nativeReceiveFrame(long decoder, ByteBuffer output, int[] info);
  private static native int nativeDrain(long decoder);
  private static native int nativeFlush(long decoder);
  private static native void nativeDestroy(long decoder);
  private static native long nativeCreateClassifier();
  private static native int nativeClassifyChunk(
      long classifier, ByteBuffer input, int length, int[] classification);
  private static native int nativeFinishClassification(long classifier, int[] classification);
  private static native int nativeResetClassifier(long classifier);
  private static native void nativeDestroyClassifier(long classifier);
}
