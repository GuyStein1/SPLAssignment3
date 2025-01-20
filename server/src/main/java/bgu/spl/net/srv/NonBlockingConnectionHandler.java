package bgu.spl.net.srv;

import bgu.spl.net.api.MessageEncoderDecoder;
import bgu.spl.net.api.MessagingProtocol;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.SelectionKey;
import java.nio.channels.SocketChannel;
import java.util.Queue;
import java.util.concurrent.ConcurrentLinkedQueue;

public class NonBlockingConnectionHandler<T> implements ConnectionHandler<T> {

    private static final int BUFFER_ALLOCATION_SIZE = 1 << 13; // 8k
    private static final ConcurrentLinkedQueue<ByteBuffer> BUFFER_POOL = new ConcurrentLinkedQueue<>();

    private final MessagingProtocol<T> protocol;
    private final MessageEncoderDecoder<T> encdec;
    private final Queue<ByteBuffer> writeQueue = new ConcurrentLinkedQueue<>();
    private final SocketChannel chan;
    private final Reactor<T> reactor;

    // Added fields
    private final Connections<T> connections;
    private final int connectionId;

    // Adjusted constructor to initialize Connections and connectionId
    public NonBlockingConnectionHandler(
            MessageEncoderDecoder<T> reader,
            MessagingProtocol<T> protocol,
            SocketChannel chan,
            Reactor<T> reactor,
            Connections<T> connections,
            int connectionId) {
        this.chan = chan;
        this.encdec = reader;
        this.protocol = protocol;
        this.reactor = reactor;

        // Initialize new fields
        this.connections = connections;
        this.connectionId = connectionId;

        // Register this handler in ConnectionsImpl to allow message sending
        connections.register(connectionId, this);

        // Ensure the protocol is initialized with the correct connection info
        protocol.start(connectionId, connections);
    }

    public Runnable continueRead() {
        ByteBuffer buf = leaseBuffer();

        boolean success = false;
        try {
            success = chan.read(buf) != -1;
        } catch (IOException ex) {
            ex.printStackTrace();
        }

        if (success) {
            buf.flip();
            return () -> {
                try {
                    while (buf.hasRemaining()) {
                        T nextMessage = encdec.decodeNextByte(buf.get());
                        if (nextMessage != null) {
                            // Previously, the response was added directly to `writeQueue` here.
                            // This caused writing to be mixed with reading logic.
                            // Instead, we now call `protocol.process(nextMessage)`, and if a response
                            // needs to be sent, it will be handled properly via `send(msg)`. 
                            protocol.process(nextMessage);
                        }
                    }
                } finally {
                    releaseBuffer(buf);
                }
            };
        } else {
            releaseBuffer(buf);
            close();
            return null;
        }

    }

    public void close() {
        try {
            chan.close();
            // Remove client from Connections 
            connections.disconnect(connectionId);
        } catch (IOException ex) {
            ex.printStackTrace();
        }
    }

    public boolean isClosed() {
        return !chan.isOpen();
    }

    public void continueWrite() {
        while (!writeQueue.isEmpty()) {
            try {
                ByteBuffer top = writeQueue.peek();
                chan.write(top);
                if (top.hasRemaining()) {
                    return;
                } else {
                    writeQueue.remove();
                }
            } catch (IOException ex) {
                ex.printStackTrace();
                close();
            }
        }

        if (writeQueue.isEmpty()) {
            if (protocol.shouldTerminate())
                close();
            else
                reactor.updateInterestedOps(chan, SelectionKey.OP_READ);
        }
    }

    private static ByteBuffer leaseBuffer() {
        ByteBuffer buff = BUFFER_POOL.poll();
        if (buff == null) {
            return ByteBuffer.allocateDirect(BUFFER_ALLOCATION_SIZE);
        }

        buff.clear();
        return buff;
    }

    private static void releaseBuffer(ByteBuffer buff) {
        BUFFER_POOL.add(buff);
    }

    /*
     *   Sends a message to the client.
     * - Encodes the message and queues it for writing.
     * - Registers the channel for writing in the Reactor.
     */
    @Override
    public void send(T msg) {
        if (isClosed()) return; // Do not send if the connection is closed
        try {
            byte[] encodedMsg = encdec.encode(msg);
            writeQueue.add(ByteBuffer.wrap(encodedMsg));
            // Previously, writing logic was inside `continueRead()`, which mixed reading and writing.
            // By moving message writing to `send()`, we ensure that outgoing messages are queued
            // separately, making the implementation cleaner and avoiding potential race conditions.
            reactor.updateInterestedOps(chan, SelectionKey.OP_READ | SelectionKey.OP_WRITE);
        } catch (Exception e) {
            e.printStackTrace();
            close(); // Close connection on failure
        }
    }
}
