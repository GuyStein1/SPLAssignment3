package bgu.spl.net.srv;

import bgu.spl.net.api.MessageEncoderDecoder;
import bgu.spl.net.api.MessagingProtocol;
import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.IOException;
import java.net.Socket;

public class BlockingConnectionHandler<T> implements Runnable, ConnectionHandler<T> {

    private final MessagingProtocol<T> protocol;
    private final MessageEncoderDecoder<T> encdec;
    private final Socket sock;
    private BufferedInputStream in;
    private BufferedOutputStream out;
    private volatile boolean connected = true;

    // Added fields
    private final Connections<T> connections;
    private final int connectionId;

    // Constructor for the BlockingConnectionHandler
    public BlockingConnectionHandler(Socket sock, MessageEncoderDecoder<T> reader, MessagingProtocol<T> protocol,
            Connections<T> connections, int connectionId) {
        this.sock = sock;
        this.encdec = reader;
        this.protocol = protocol;

        // Initialize added fields
        this.connections = connections;
        this.connectionId = connectionId;

        // Register this handler in ConnectionsImpl to allow message sending
        connections.register(connectionId, this);
    }

    @Override
    public void run() {
        try (Socket sock = this.sock) { // just for automatic closing
            int read;

            in = new BufferedInputStream(sock.getInputStream());
            out = new BufferedOutputStream(sock.getOutputStream());

            while (!protocol.shouldTerminate() && connected && (read = in.read()) >= 0) {
                T nextMessage = encdec.decodeNextByte((byte) read);
                if (nextMessage != null) {
                    // Process message (sending is handled inside `process()`)
                    protocol.process(nextMessage);
                }
            }

        } catch (IOException ex) {
            ex.printStackTrace();
        }

    }

    // Closes the connection and removes the client from `ConnectionsImpl`.
    @Override
    public void close() {
        connected = false;
        connections.disconnect(connectionId); // Remove client from active connections
        try {
            sock.close();
        } catch (IOException ignored) {}
    }

    // Sends a message to the client
    @Override
    public void send(T msg) {
        if (!connected) return; // Do not send messages if the client is disconnected
        try {
            byte[] encodedMsg = encdec.encode(msg);
            out.write(encodedMsg);
            out.flush();
        } catch (IOException e) {
            close(); // Close connection on failure
        }
    }
}
