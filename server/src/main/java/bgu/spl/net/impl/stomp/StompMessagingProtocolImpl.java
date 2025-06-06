package bgu.spl.net.impl.stomp;

import bgu.spl.net.api.StompMessagingProtocol;
import bgu.spl.net.srv.Connections;

import java.util.Map;
import java.util.HashMap;

public class StompMessagingProtocolImpl implements StompMessagingProtocol<StompFrame> {
    private int connectionId;
    private Connections<StompFrame> connections;
    private boolean shouldTerminate = false;
    private boolean connected = false; // Tracks if the client has successfully connected

    // Each client manages its own subscriptions: (subscriptionId -> topic)
    private final Map<Integer, String> clientSubscriptions = new HashMap<>();

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
            case "SEND":
                handleSend(message);
                break;
            case "SUBSCRIBE":
                handleSubscribe(message);
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
     * - Ensures required headers (`accept-version`, `login`, `passcode`, `host`)
     * are present.
     * - Authenticates the user or creates a new one if it doesn't exist.
     * - Sends a RECEIPT if the client requested it.
     * - Sends an ERROR frame if authentication fails or required headers are
     * missing.
     */
    private void handleConnect(StompFrame message) {
        if (connected) {
            sendError("User already logged in", message.getHeader("receipt"), message);
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

         // Check if user is already active
        if (connections.isUserActive(login)) {
            sendError("User already logged in", receiptId, message);
            return;
        }

        // Validate the user's credentials
        if (!connections.authenticateUser(connectionId, login, passcode)) {
            sendError("Invalid password for user: " + login, message.getHeader("receipt"), message);
            return;
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
     * - Removes the client from all subscriptions in `topicSubscriptions`.
     * - Sends a RECEIPT if requested.
     */
    private void handleDisconnect(StompFrame message) {
        if (!connected) {
            sendError("User is already disconnected.", message.getHeader("receipt"), message);
            return;
        }

        shouldTerminate = true;

        // Remove the client from all their subscriptions
        for (String topic : clientSubscriptions.values()) {
            connections.removeSubscription(topic, connectionId);
        }

        // Clear the client’s personal subscription tracking
        clientSubscriptions.clear();

        sendReceiptIfRequested(message.getHeader("receipt"));

        connections.disconnect(connectionId);
    }

    /**
     * Handles a SUBSCRIBE request.
     * - Ensures the client is connected before subscribing.
     * - Stores the subscription using `subscriptionId` per client.
     * - Adds the client to the global `topicSubscriptions` list.
     * - Sends an ERROR if required headers are missing.
     * - Sends a RECEIPT if requested.
     */
    private void handleSubscribe(StompFrame message) {
        if (!connected) {
            sendError("SUBSCRIBE received before CONNECT", message.getHeader("receipt"), message);
            return;
        }

        // Extract required headers
        String topic = message.getHeader("destination");
        String subscriptionIdStr = message.getHeader("id");

        if (topic == null || subscriptionIdStr == null) {
            sendError("Missing required headers in SUBSCRIBE. Required: destination, id", message.getHeader("receipt"),
                    message);
            return;
        }

        int subscriptionId;
        try {
            subscriptionId = Integer.parseInt(subscriptionIdStr);
        } catch (NumberFormatException e) {
            sendError("Invalid subscription ID format: " + subscriptionIdStr, message.getHeader("receipt"), message);
            return;
        }

        // Store the subscription for this client
        clientSubscriptions.put(subscriptionId, topic);

        // Add client to the global topic subscription map
        connections.addSubscription(topic, connectionId, subscriptionId);

        // Send RECEIPT if requested
        sendReceiptIfRequested(message.getHeader("receipt"));
    }

    /**
     * Handles an UNSUBSCRIBE request.
     * - Ensures the client is connected before unsubscribing.
     * - Removes the client's subscription using `subscriptionId`.
     * - Removes the client from the global `topicSubscriptions` list.
     * - Sends an ERROR if required headers are missing or the subscription does not
     * exist.
     * - Sends a RECEIPT if requested.
     */
    private void handleUnsubscribe(StompFrame message) {
        if (!connected) {
            sendError("UNSUBSCRIBE received before CONNECT", message.getHeader("receipt"), message);
            return;
        }

        // Extract required header
        String subscriptionIdStr = message.getHeader("id");
        if (subscriptionIdStr == null) {
            sendError("Missing `id` header in UNSUBSCRIBE", message.getHeader("receipt"), message);
            return;
        }

        int subscriptionId;
        try {
            subscriptionId = Integer.parseInt(subscriptionIdStr);
        } catch (NumberFormatException e) {
            sendError("Invalid subscription ID format: " + subscriptionIdStr, message.getHeader("receipt"), message);
            return;
        }

        // Retrieve the topic the client is subscribed to under this `subscriptionId`
        String topic = clientSubscriptions.remove(subscriptionId);

        if (topic == null) {
            sendError("Attempted unsubscribing from a channel not subscribed to.", message.getHeader("receipt"), message);
            return;
        }

        // Remove the client from the topicSubscriptions map
        connections.removeSubscription(topic, connectionId);

        sendReceiptIfRequested(message.getHeader("receipt"));
    }

    /**
     * Handles a SEND request.
     * - Ensures the sender is subscribed to the topic before allowing them to send
     * a message.
     * - Sends a MESSAGE to all subscribers of the topic.
     * - Each MESSAGE must include:
     * 1. The correct `subscriptionId` for the receiving client.
     * 2. A unique `message-id` generated by the server.
     * - Sends an ERROR if the sender is not subscribed.
     */

    private void handleSend(StompFrame message) {
        if (!connected) {
            sendError("SEND received before CONNECT", message.getHeader("receipt"), message);
            return;
        }

        // Extract required headers
        String topic = message.getHeader("destination");
        if (topic == null) {
            sendError("Missing destination header in SEND", message.getHeader("receipt"), message);
            return;
        }

        // Check if the sender is subscribed to the topic
        if (!clientSubscriptions.containsValue(topic)) {
            sendError("You must be subscribed to topic " + topic + " to send messages.",
                    message.getHeader("receipt"),
                    message);
            return;
        }

        int messageId = connections.getNextMessageId(); // Generate unique message ID

        // Send MESSAGE frame to all subscribers, including their unique subscriptionId
        Map<Integer, Integer> subscribers = connections.getSubscribers(topic);
        for (Map.Entry<Integer, Integer> entry : subscribers.entrySet()) {
            
            int subscriberConnectionId = entry.getKey();
            int subscriptionId = entry.getValue(); // Get the correct subscriptionId for this subscriber

            // Construct the MESSAGE frame with the subscription ID and unique message ID
            Map<String, String> headers = new HashMap<>();
            headers.put("destination", topic);
            headers.put("subscription", String.valueOf(subscriptionId)); // Include subscription ID
            headers.put("message-id", String.valueOf(messageId)); // Include unique message ID

            StompFrame messageFrame = new StompFrame("MESSAGE", headers, message.getBody());
            connections.send(subscriberConnectionId, messageFrame);
        }
    }

    /**
     * Sends an ERROR frame to the client when an invalid request is detected.
     * - Includes the original STOMP frame.
     * - Ensures that if a "receipt" was included in the request, it's also included
     * in the ERROR response.
     * - Terminates the connection after sending the ERROR.
     */
    private void sendError(String errorMessage, String receiptId, StompFrame originalMessage) {
        // Construct the ERROR frame body with the malformed frame and explanation
        String body = "The message:\n"
                + "-----\n"
                + originalMessage.toString() + "\n"
                + "-----\n"
                + "Reason: " + errorMessage;

        // Construct headers
        Map<String, String> headers = new HashMap<>();
        headers.put("message", " " + errorMessage);

        // Include the receipt ID if one was provided
        if (receiptId != null) {
            headers.put("receipt-id", receiptId);
        }

        // Create the ERROR frame
        StompFrame errorFrame = new StompFrame("ERROR", headers, body);

        // Send the ERROR frame
        connections.send(connectionId, errorFrame);

        // Mark connection for termination
        shouldTerminate = true;
    }

    /**
     * Sends a RECEIPT frame if the original message included a "receipt" header.
     * - Ensures proper acknowledgment of received frames when requested.
     */
    private void sendReceiptIfRequested(String receiptId) {
        if (receiptId != null) {
            // Construct receipt header
            Map<String, String> receiptHeaders = new HashMap<>();
            receiptHeaders.put("receipt-id", receiptId);

            // Create and send the RECEIPT frame
            StompFrame receipt = new StompFrame("RECEIPT", receiptHeaders, "");
            connections.send(connectionId, receipt);
        }
    }

    /**
     * Indicates whether the connection should be terminated.
     * - Returns true if an ERROR frame was sent or if the client requested to
     * disconnect.
     */
    @Override
    public boolean shouldTerminate() {
        return shouldTerminate;
    }

}