#include "../include/StompProtocol.h"
#include <sstream>
#include <iostream>

// Constructor (C++ automatically initializes hash maps)
StompProtocol::StompProtocol() : connected(false) {}

// Processes a received STOMP frame from the server and handles messages.
void StompProtocol::processServerMessage(const std::string& message, ConnectionHandler& connectionHandler) {
    std::istringstream stream(message);
    std::string line;
    std::getline(stream, line); // Read the STOMP command

    if (line == "CONNECTED") {
        connected = true; // Mark client as connected
        std::cout << "Successfully connected to STOMP server." << std::endl;
    } 
    else if (line == "MESSAGE") {
        std::string topic, body;

        // Extract headers
        while (std::getline(stream, line) && !line.empty()) {
            if (line.find("destination:") == 0) {
                topic = line.substr(11); // Extracts topic name, "destination:" is 11 chars long
            }
        }

        // Read the message body
        std::getline(stream, body);

        // Store the message in the summary database
        eventSummary[topic].push_back(body);

        std::cout << "New message from " << topic << ": " << body << std::endl;
    } 
    else if (line == "RECEIPT") {
        std::cout << "Server acknowledged request." << std::endl;
    } 
    else if (line == "ERROR") {
        std::cerr << "Server error received: " << message << std::endl;
        connectionHandler.close();
    }
}


/**
 * Constructs a CONNECT frame for user authentication.
 */
std::string StompProtocol::createConnectFrame(const std::string& username, const std::string& password) {
    std::ostringstream frame;
    frame << "CONNECT\n";
    frame << "accept-version:1.2\n";
    frame << "host:stomp.server\n";
    frame << "login:" << username << "\n";
    frame << "passcode:" << password << "\n\n";
    frame << '\0'; // STOMP message must end with a null terminator
    return frame.str();
}

/**
 * Constructs a SUBSCRIBE frame to join a topic.
 */
std::string StompProtocol::createSubscribeFrame(const std::string& topic, int subscriptionId) {
    std::ostringstream frame;
    frame << "SUBSCRIBE\n";
    frame << "destination:" << topic << "\n";
    frame << "id:" << subscriptionId << "\n\n";
    frame << '\0';
    return frame.str();
}

/**
 * Constructs a SEND frame to publish a message to a topic.
 */
std::string StompProtocol::createSendFrame(const std::string& destination, const std::string& body) {
    std::ostringstream frame;
    frame << "SEND\n";
    frame << "destination:" << destination << "\n\n";
    frame << body << "\n";
    frame << '\0';
    return frame.str();
}

/**
 * Constructs a DISCONNECT frame to close the connection.
 */
std::string StompProtocol::createDisconnectFrame(int receiptId) {
    std::ostringstream frame;
    frame << "DISCONNECT\n";
    frame << "receipt:" << receiptId << "\n\n";
    frame << '\0';
    return frame.str();
}

/**
 * Adds a topic to the client's subscription list.
 */
void StompProtocol::addSubscription(const std::string& topic, int subscriptionId) {
    subscriptions[topic] = subscriptionId;
}

/**
 * Removes a topic from the client's subscription list.
 */
void StompProtocol::removeSubscription(const std::string& topic) {
    subscriptions.erase(topic);
}

/**
 * Prints a summary of received messages grouped by topic.
 */
void StompProtocol::printSummary() {
    for (const auto& entry : eventSummary) {
        std::cout << "Topic: " << entry.first << std::endl;
        for (const std::string& message : entry.second) {
            std::cout << "  - " << message << std::endl;
        }
    }
}
