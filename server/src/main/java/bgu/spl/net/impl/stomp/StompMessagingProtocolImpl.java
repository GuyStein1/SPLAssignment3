package bgu.spl.net.impl.stomp;

import bgu.spl.net.api.StompMessagingProtocol;
import bgu.spl.net.srv.Connections;

import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.Set;
import java.util.HashMap;
import java.util.Iterator;

public class StompMessagingProtocolImpl implements StompMessagingProtocol<StompFrame> {
    private int connectionId;
    private Connections<StompFrame> connections;
    private boolean shouldTerminate = false;
    private boolean connected = false; // Tracks if the client has successfully connected

    // Shared subscriptions map: Tracks which clients (connectionId) are subscribed to which topics
    private static final Map<String, Set<Integer>> subscriptions = new ConcurrentHashMap<>();

    // Stores valid user credentials (for authentication)
    private static final Map<String, String> users = new ConcurrentHashMap<>();

    @Override
    public void start(int connectionId, Connections<StompFrame> connections) {
        this.connectionId = connectionId;
        this.connections = connections;
    }

    @Override
    public void process(StompFrame message) {
        // Handle different STOMP commands
        switch (message.getCommand()) {
            case "CONNECT":
                handleConnect(message);
                break;
            case "SUBSCRIBE":
                handleSubscribe(message);
                break;
            case "SEND":
                handleSend(message);
                break;
            case "UNSUBSCRIBE":
                handleUnsubscribe(message);
                break;
            case "DISCONNECT":
                handleDisconnect(message);
                break;
            default:
                // If the command is unknown, send an ERROR frame
                sendError("Invalid command: " + message.getCommand(), message.getHeader("receipt"), message);
        }
    }

    /**
     * Handles a CONNECT request.
     * - Ensures required headers (`accept-version`, `login`, `passcode`, `host`) are present.
     * - Authenticates the user or creates a new one if it doesn't exist.
     * - Sends a RECEIPT if the client requested it.
     * - Sends an ERROR frame if authentication fails or required headers are missing.
     */
    private void handleConnect(StompFrame message) {
        if (connected) {
            sendError("Duplicate CONNECT command received.", message.getHeader("receipt"), message);
            return;
        }

        // Extract required headers
        String version = message.getHeader("accept-version");
        String login = message.getHeader("login");
        String passcode = message.getHeader("passcode");
        String host = message.getHeader("host");
        String receiptId = message.getHeader("receipt"); // Check if a receipt was requested

        // Validate required headers
        if (version == null || login == null || passcode == null || host == null) {
            sendError("Missing required headers in CONNECT. Required: accept-version, login, passcode, host",
                    receiptId, message);
            return;
        }

        // Check if the user already exists
        if (users.containsKey(login)) {
            // If the username exists but the password is incorrect, send an ERROR
            if (!users.get(login).equals(passcode)) {
                sendError("Invalid password for user: " + login, receiptId, message);
                return;
            }
        } else {
            // If the username does not exist, create a new user
            users.put(login, passcode);
        }

        // Mark client as connected
        connected = true;

        // Create headers for CONNECTED response
        Map<String, String> responseHeaders = new HashMap<>();
        responseHeaders.put("version", "1.2");

        // Send a CONNECTED frame to confirm success
        StompFrame response = new StompFrame("CONNECTED", responseHeaders, "");
        connections.send(connectionId, response);

        // Send a RECEIPT if the client requested it
        sendReceiptIfRequested(receiptId);
    }

    /**
     * Handles a DISCONNECT request.
     * - Ensures the client is connected before allowing disconnection.
     * - Removes the client from all subscriptions.
     * - Sends a RECEIPT if requested.
     */
    private void handleDisconnect(StompFrame message) {
        if (!connected) {
            sendError("User is already disconnected.", message.getHeader("receipt"), message);
            return;
        }

        shouldTerminate = true;

       // Use an Iterator to safely remove elements while iterating (thread-safe)
       synchronized (subscriptions) {
        Iterator<Map.Entry<String, Set<Integer>>> iterator = subscriptions.entrySet().iterator();
        while (iterator.hasNext()) {
            Map.Entry<String, Set<Integer>> entry = iterator.next();
            Set<Integer> subscribers = entry.getValue();
            subscribers.remove(connectionId); // Remove client from the topic
            if (subscribers.isEmpty()) {
                iterator.remove(); // Safely remove topic if empty
            }
        }
    }

        sendReceiptIfRequested(message.getHeader("receipt"));

        connections.disconnect(connectionId);
    }

    /**
     * Handles a SUBSCRIBE request.
     * - Adds the client to the specified topic.
     * - Sends a RECEIPT if requested.
     */
    private void handleSubscribe(StompFrame message) {
        if (!connected) {
            sendError("Subscribe attempt before connecting.", message.getHeader("receipt"), message);
            return;
        }

        String topic = message.getHeader("destination");
        if (topic == null) {
            sendError("Missing destination header in SUBSCRIBE", message.getHeader("receipt"), message);
            return;
        }

        subscriptions.putIfAbsent(topic, ConcurrentHashMap.newKeySet());
        subscriptions.get(topic).add(connectionId);

        sendReceiptIfRequested(message.getHeader("receipt"));
    }

    /**
     * Handles an UNSUBSCRIBE request.
     * - Removes the client from the specified topic.
     * - Sends an ERROR if the client was never subscribed.
     */
    private void handleUnsubscribe(StompFrame message) {
        if (!connected) {
            sendError("UNSUBSCRIBE received before CONNECT", message.getHeader("receipt"), message);
            return;
        }

        String topic = message.getHeader("destination");
        if (topic == null) {
            sendError("Missing destination header in UNSUBSCRIBE", message.getHeader("receipt"), message);
            return;
        }

        Set<Integer> subscribers = subscriptions.get(topic);
        if (subscribers == null || !subscribers.contains(connectionId)) {
            sendError("Client is not subscribed to the topic: " + topic, message.getHeader("receipt"), message);
            return;
        }

        subscribers.remove(connectionId);
        if (subscribers.isEmpty()) {
            subscriptions.remove(topic);
        }

        sendReceiptIfRequested(message.getHeader("receipt"));
    }

    /**
     * Handles a SEND request.
     * - Sends a message to all subscribers of a topic.
     * - Sends an ERROR if the topic doesn't exist.
     */
    private void handleSend(StompFrame message) {
        if (!connected) {
            sendError("SEND received before CONNECT", message.getHeader("receipt"), message);
            return;
        }

        String topic = message.getHeader("destination");
        if (topic == null) {
            sendError("Missing destination header in SEND", message.getHeader("receipt"), message);
            return;
        }

        Set<Integer> subscribers = subscriptions.get(topic);
        if (subscribers == null || subscribers.isEmpty()) {
            sendError("No subscribers exist for topic: " + topic, message.getHeader("receipt"), message);
            return;
        }

        for (int subscriberId : subscribers) {
            connections.send(subscriberId, message);
        }
    }

    private void sendError(String errorMessage, String receiptId, StompFrame originalMessage) {
        String body = "The message:\n-----\n" + originalMessage.toString() + "\n-----\n" + errorMessage;

        Map<String, String> headers = new HashMap<>();
        headers.put("message", "malformed frame received");

        if (receiptId != null) {
            headers.put("receipt-id", receiptId);
        }

        StompFrame errorFrame = new StompFrame("ERROR", headers, body);
        connections.send(connectionId, errorFrame);
        shouldTerminate = true;
    }

    private void sendReceiptIfRequested(String receiptId) {
        if (receiptId != null) {
            Map<String, String> receiptHeaders = new HashMap<>();
            receiptHeaders.put("receipt-id", receiptId);
            StompFrame receipt = new StompFrame("RECEIPT", receiptHeaders, "");
            connections.send(connectionId, receipt);
        }
    }

    @Override
    public boolean shouldTerminate() {
        return shouldTerminate;
    }
}