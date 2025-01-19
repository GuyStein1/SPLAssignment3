package bgu.spl.net.srv;

import java.io.IOException;

public interface Connections<T> {

    boolean send(int connectionId, T msg);

    // void send(String channel, T msg);

    void disconnect(int connectionId);

    // Added method to register client connections
    void register(int connectionId, ConnectionHandler<T> handler);
}
