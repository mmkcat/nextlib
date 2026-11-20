package io.github.anilbeesetti.nextlib.media3ext.ffdecoder;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.nio.ByteBuffer;
import java.util.List;
import org.junit.Test;

public final class OpenJocAdmissionBufferTest {

  @Test
  public void appendCopiesRemainingBytesAndPreservesInputOrderMetadata() {
    OpenJocAdmissionBuffer buffer = new OpenJocAdmissionBuffer();
    ByteBuffer first = ByteBuffer.wrap(new byte[] {9, 1, 2, 8});
    first.position(1);
    first.limit(3);

    assertTrue(buffer.append(first, 12_000L, 3));
    assertEquals(1, first.position());
    assertEquals(2, buffer.sizeBytes());

    assertTrue(buffer.append(ByteBuffer.wrap(new byte[] {3, 4}), 24_000L, 5));
    List<OpenJocAdmissionBuffer.Chunk> chunks = buffer.snapshot();
    assertEquals(2, chunks.size());
    assertArrayEquals(new byte[] {1, 2}, chunks.get(0).data);
    assertEquals(12_000L, chunks.get(0).timeUs);
    assertEquals(3, chunks.get(0).flags);
    assertArrayEquals(new byte[] {3, 4}, chunks.get(1).data);
    assertEquals(24_000L, chunks.get(1).timeUs);
    assertEquals(5, chunks.get(1).flags);
  }

  @Test
  public void appendRejectsInputThatWouldExceedBoundWithoutPartialMutation() {
    OpenJocAdmissionBuffer buffer = new OpenJocAdmissionBuffer();
    assertTrue(buffer.append(ByteBuffer.allocate(OpenJocAdmissionBuffer.MAX_BYTES - 1), 0, 0));
    assertFalse(buffer.append(ByteBuffer.wrap(new byte[] {1, 2}), 1, 0));
    assertEquals(OpenJocAdmissionBuffer.MAX_BYTES - 1, buffer.sizeBytes());
    assertEquals(1, buffer.snapshot().size());
  }

  @Test
  public void clearReleasesAllRetainedChunksAndResetsByteCount() {
    OpenJocAdmissionBuffer buffer = new OpenJocAdmissionBuffer();
    assertTrue(buffer.append(ByteBuffer.wrap(new byte[] {1, 2, 3}), 0, 0));
    buffer.clear();
    assertTrue(buffer.isEmpty());
    assertEquals(0, buffer.sizeBytes());
    assertTrue(buffer.snapshot().isEmpty());
  }
}
