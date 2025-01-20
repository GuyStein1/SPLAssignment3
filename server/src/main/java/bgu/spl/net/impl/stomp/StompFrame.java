package bgu.spl.net.impl.stomp;

import java.util.HashMap;
import java.util.Map;

/**
 * The StompFrame class represents a single STOMP message (frame) exchanged between the client and server.
 * 
 * - STOMP messages have a structured format: 
 *   1. A command.
 *   2. A set of headers (key-value pairs).
 *   3. A body.
 * - Instead of handling raw Strings for messages, we encapsulate them in a dedicated object, making parsing and
 *   processing easier and reducing the chances of errors.
 */
public class StompFrame {
    private final String command;
    private final Map<String, String> headers;
    private final String body;

    // Constructor 
    public StompFrame(String command, Map<String, String> headers, String body) {
        this.command = command;
        this.headers = headers != null ? headers : new HashMap<>();
        this.body = body;
    }

    // Getters
    public String getCommand() {
        return command;
    }

    public Map<String, String> getHeaders() {
        return headers;
    }

    public String getBody() {
        return body;
    }

    public String getHeader(String key) {
        return headers.get(key);
    }

    // Adds a new header or updates an existing one
    // DO WE NEED THIS????!?!?!?!?!!?!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    public void addHeader(String key, String value) {
        headers.put(key, value);
    }

    // Converts StompFrame to String
    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append(command).append("\n"); // Add command at the top
        
        // Append all headers
        for (Map.Entry<String, String> entry : headers.entrySet()) {
            sb.append(entry.getKey()).append(":").append(entry.getValue()).append("\n");
        }
    
        // Add an empty line to separate headers from body (STOMP format)
        sb.append("\n");
    
        // Append body if it's not empty
        if (!body.isEmpty()) {
            sb.append(body).append("\n");
        }
    
        // Append the STOMP null terminator 
        sb.append('\u0000');
    
        return sb.toString();
    }

    // Parses a STOMP message string into a StompFrame object
    public static StompFrame fromString(String message) {
        // Ensure we remove '\u0000' at the end if present
        if (message.endsWith("\u0000")) {
            message = message.substring(0, message.length() - 1); // Remove the last character
        }
    
        // Split the message into header section and body
        String[] parts = message.split("\n\n", 2);
        String headerPart = parts[0]; // Everything before the double new line
        String body = parts.length > 1 ? parts[1] : ""; // Extract body if it exists
    
        // Split headers and command
        String[] lines = headerPart.split("\n");
        String command = lines[0]; // The first line is always the STOMP command
    
        // Parse headers
        Map<String, String> headers = new HashMap<>();
        for (int i = 1; i < lines.length; i++) {
            String[] keyValue = lines[i].split(":", 2); // Split header key and value
            if (keyValue.length == 2) {
                headers.put(keyValue[0], keyValue[1]);
            }
        }
    
        // Return a new StompFrame object with the extracted components
        return new StompFrame(command, headers, body);
    }
}
