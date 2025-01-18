package bgu.spl.net.impl.stomp;

import bgu.spl.net.api.MessageEncoderDecoder;

import java.nio.charset.StandardCharsets;
import java.util.Arrays;

public class StompMessageEncoderDecoder implements MessageEncoderDecoder<StompFrame> {

    private byte[] bytes = new byte[1 << 10]; // Start with 1 KB buffer
    private int len = 0;

    @Override
    public StompFrame decodeNextByte(byte nextByte) {
        if (nextByte == '\u0000') { // STOMP messages end with a null character
            return popFrame(); // Convert accumulated bytes into a StompFrame
        }

        pushByte(nextByte);
        return null; // Message not complete yet
    }

    @Override
    public byte[] encode(StompFrame message) {
        return message.toString().getBytes(StandardCharsets.UTF_8); // Convert to byte array
    }

    private void pushByte(byte nextByte) {
        if (len >= bytes.length) {
            bytes = Arrays.copyOf(bytes, len * 2); // Expand buffer if full
        }
        bytes[len++] = nextByte;
    }

    private StompFrame popFrame() {
        // Convert accumulated bytes into a string
        String message = new String(bytes, 0, len, StandardCharsets.UTF_8);
        len = 0; // Reset buffer for next message
        return StompFrame.fromString(message); // Parse into StompFrame
    }
}
