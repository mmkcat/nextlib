package io.github.anilbeesetti.nextlib.media3ext.ffdecoder;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Bounded copy of compressed input retained while OpenJOC admission is UNKNOWN.
 *
 * <p>The classifier consumes borrowed chunks but does not return the bytes it inspected.
 * Keeping the original chunks here allows the caller to replay them, in order, to either
 * OpenJOC or the ordinary E-AC-3 decoder once admission is decided.
 */
final class OpenJocAdmissionBuffer {
  // OpenJOC 4f740b25: 4096-byte syncframes * 6 blocks * (I0 + D0..D7) * 16 AUs.
  static final int MAX_BYTES = 3_538_944;

  static final class Chunk {
    final byte[] data;
    final long timeUs;
    final int flags;

    private Chunk(byte[] data, long timeUs, int flags) {
      this.data = data;
      this.timeUs = timeUs;
      this.flags = flags;
    }
  }

  private final ArrayList<Chunk> chunks = new ArrayList<>();
  private int sizeBytes;

  /** Copies the remaining bytes and their Media3 timing/flags into the replay queue. */
  boolean append(ByteBuffer source, long timeUs, int flags) {
    ByteBuffer input = source.duplicate();
    int length = input.remaining();
    if (length == 0 || length > MAX_BYTES - sizeBytes) {
      return false;
    }
    byte[] copy = new byte[length];
    input.get(copy);
    chunks.add(new Chunk(copy, timeUs, flags));
    sizeBytes += length;
    return true;
  }

  /** Returns an immutable snapshot in original input order. */
  List<Chunk> snapshot() {
    return Collections.unmodifiableList(new ArrayList<>(chunks));
  }

  int sizeBytes() {
    return sizeBytes;
  }

  boolean isEmpty() {
    return chunks.isEmpty();
  }

  void clear() {
    chunks.clear();
    sizeBytes = 0;
  }
}
