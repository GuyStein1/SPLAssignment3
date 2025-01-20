package bgu.spl.net.srv;

import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Manages active connections for all clients.
 * - Allows sending messages to individual clients.
 * - Tracks active connections using `connectionId -> ConnectionHandler<T>`.
 */
public class ConnectionsImpl<T> implements Connections<T> {

    // Stores active client connections (connectionId -> ConnectionHandler<T>)
    private final ConcurrentHashMap<Integer, ConnectionHandler<T>> activeConnections = new ConcurrentHashMap<>();

    // Shared topic subscriptions: (topic -> {connectionId -> subscriptionId})
    private static final Map<String, Map<Integer, Integer>> topicSubscriptions = new ConcurrentHashMap<>();

    // Stores valid user credentials (for authentication)
    private static final Map<String, String> users = new ConcurrentHashMap<>();

    private static final AtomicInteger messageCounter = new AtomicInteger(0); // Unique message ID counter

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

        // Remove client from all topic subscriptions
        synchronized (topicSubscriptions) {
            for (Map<Integer, Integer> subscribers : topicSubscriptions.values()) {
                subscribers.remove(connectionId);
            }
        }
    }

    // Registers a new client connection.
    public void register(int connectionId, ConnectionHandler<T> handler) {
        activeConnections.put(connectionId, handler);
    }

    // Registers a new user or verifies credentials.
    public boolean authenticateUser(String username, String password) {
        synchronized (users) {
            if (users.containsKey(username)) {
                return users.get(username).equals(password); // Validate password
            } else {
                users.put(username, password); // Register new user
                return true;
            }
        }
    }

    // Generates a unique message ID.
    public int getNextMessageId() {
        return messageCounter.incrementAndGet();
    }

    // Adds a client subscription.
    public void addSubscription(String topic, int connectionId, int subscriptionId) {
        topicSubscriptions.putIfAbsent(topic, new ConcurrentHashMap<>());
        topicSubscriptions.get(topic).put(connectionId, subscriptionId);

        // Prints for debugging
        System.out.println("subscribe excecuted");
        System.out.println(topicSubscriptions.toString());
    }

    // Removes a client subscription.
    public void removeSubscription(String topic, int connectionId) {
        if (topicSubscriptions.containsKey(topic)) {
            topicSubscriptions.get(topic).remove(connectionId);
            if (topicSubscriptions.get(topic).isEmpty()) {
                topicSubscriptions.remove(topic);
            }
        }
    }

    // Retrieves all subscribers of a topic.
    public Map<Integer, Integer> getSubscribers(String topic) {
        return topicSubscriptions.getOrDefault(topic, new ConcurrentHashMap<>());
    }
}
