#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "event.h"
#include "ConnectionHandler.h"

class StompProtocol
{
public:
    StompProtocol(ConnectionHandler &handler); // Initializes the STOMP protocol handler.

    void connect(); // Sends a CONNECT frame to the server.

    void send(const std::string &command, const std::map<std::string, std::string> &headers, const std::string &body); // Sends a STOMP frame.

    void parseFrame(const std::string &message); // Parses a received STOMP frame.

    void summarizeEmergencyChannel(const std::string &channel, const std::string &user, const std::string &filePath); // Summarizes stored events and saves to file.

    std::string epochToDate(int epochTime) const; // Converts epoch time to a formatted date string.

    bool isConnected() const; // Checks if the client is connected.

    int getNextId();        // Generates a unique subscription ID
    int getNextReceiptId(); // Generates a unique receipt ID

    void storeReceipt(int receiptId, const std::string& requestType); // Stores the mapping between receipt ID and request type

private:
    ConnectionHandler &connectionHandler;                   // Handles communication with the server.
    bool connected;                                         // Indicates if the client is connected.

    std::unordered_map<std::string, std::vector<Event>> eventSummary; // Stores received events.

    // Used to match RECEIPT frames to their corresponding requests, and know which request by the client the receipt is for.
    std::unordered_map<int, std::string> receiptMap; // Maps receipt ID → request type

    int idCounter = 0;       // Tracks unique subscription IDs per client
    int receiptCounter = 0;  // Tracks unique receipt IDs per client

    void handleConnected();                                                                         // Handles a CONNECTED frame.
    void handleMessage(const std::map<std::string, std::string> &headers, const std::string &body); // Handles MESSAGE frames.
    void handleError(const std::map<std::string, std::string> &headers, const std::string &body);   // Handles ERROR frames.
    void handleReceipt(const std::map<std::string, std::string> &headers);                          // Handles RECEIPT frames.
};
