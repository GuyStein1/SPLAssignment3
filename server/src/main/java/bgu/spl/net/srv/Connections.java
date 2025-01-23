package bgu.spl.net.srv;

import java.io.IOException;
import java.util.Map;

public interface Connections<T> {

    boolean send(int connectionId, T msg);

    // void send(String channel, T msg);

    void disconnect(int connectionId);

    // Added methods:
    void register(int connectionId, ConnectionHandler<T> handler);

    boolean authenticateUser(int connectionId, String login, String passcode);

    boolean isUserActive(String username);

    void removeSubscription(String topic, int connectionId);

    void addSubscription(String topic, int connectionId, int subscriptionId);

    int getNextMessageId();

    Map<Integer, Integer> getSubscribers(String topic);


}
