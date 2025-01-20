#pragma once

#include "../include/ConnectionHandler.h"
#include <string>
#include <unordered_map>
#include <vector>

/**
 * Handles the STOMP protocol for client-side communication.
 * - Constructs STOMP frames for sending.
 * - Processes server responses.
 * - Manages active subscriptions and stores received messages.
 */
class StompProtocol {
private:
    bool connected; // Connection status (initialized to false by default)
    std::unordered_map<std::string, int> subscriptions; // Maps topics to subscription IDs
    std::unordered_map<std::string, std::vector<std::string>> eventSummary; // Stores received messages per topic

public:
    // Constructor
    StompProtocol();

    // Processes incoming STOMP messages from the server
    void processServerMessage(const std::string& message, ConnectionHandler& connectionHandler);

    // Constructs STOMP frames
    std::string createConnectFrame(const std::string& username, const std::string& password);
    std::string createSubscribeFrame(const std::string& topic, int subscriptionId);
    std::string createSendFrame(const std::string& destination, const std::string& body);
    std::string createDisconnectFrame(int receiptId);

    // Subscription management
    void addSubscription(const std::string& topic, int subscriptionId);
    void removeSubscription(const std::string& topic);

    // Prints a summary of received messages
    void printSummary();
};
