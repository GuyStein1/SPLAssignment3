package bgu.spl.net.srv;

import java.util.concurrent.ConcurrentHashMap;

/**
 * Manages active connections for all clients.
 * - Allows sending messages to individual clients.
 * - Tracks active connections using `connectionId -> ConnectionHandler<T>`.
 */
public class ConnectionsImpl<T> implements Connections<T> {

    // Stores active client connections (connectionId -> ConnectionHandler<T>)
    private final ConcurrentHashMap<Integer, ConnectionHandler<T>> activeConnections = new ConcurrentHashMap<>();

    @Override
    public boolean send(int connectionId, T msg) {
        ConnectionHandler<T> handler = activeConnections.get(connectionId);
        if (handler != null) {
            handler.send(msg);
            return true; // Successfully sent
        }
        return false; // Connection does not exist
    }

    @Override
    public void disconnect(int connectionId) {
        activeConnections.remove(connectionId);
    }

    // Registers a new client connection.
    public void register(int connectionId, ConnectionHandler<T> handler) {
        activeConnections.put(connectionId, handler);
    }
}
