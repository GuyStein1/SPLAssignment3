#include "StompProtocol.h"
#include <sstream>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <ctime>
#include <algorithm>

// Constructor initializes STOMP protocol with connection handler.
StompProtocol::StompProtocol(ConnectionHandler &handler) : 
connectionHandler(handler), connected(false), stopCommunication(false), errorOccured(false) {}

int StompProtocol::getNextId() {
    return idCounter++;  // Generate a unique ID for subscriptions
}

int StompProtocol::getNextReceiptId() {
    return receiptCounter++;  // Generate a unique receipt ID for this client
}

int StompProtocol::getSubscriptionId(const std::string& channel) {
    if (subscriptionIds.find(channel) != subscriptionIds.end()) {
        return subscriptionIds[channel];
    }
    return -1;  // Return -1 if not found
}

void StompProtocol::storeSubscriptionId(const std::string& channel, int subscriptionId) {
    subscriptionIds[channel] = subscriptionId;
}

// Stores the request type associated with a receipt ID.
void StompProtocol::storeReceipt(int receiptId, const std::string& requestType) {
    receiptMap[receiptId] = requestType;
}

// Sends a CONNECT frame to initiate connection.
void StompProtocol::connect() {
    std::map<std::string, std::string> headers = {{"accept-version", "1.2"}, {"host", "stomp.server"}};
    send("CONNECT", headers, "");
}

// Checks if the client is connected to the server.
bool StompProtocol::isConnected() {
    std::lock_guard<std::mutex> lock(connectionMutex);
    return connected;
}

// Sets the connection status.
void StompProtocol::setConnected(bool connected) {
    std::lock_guard<std::mutex> lock(connectionMutex);
    this->connected = connected;
}

// Check if an error occurred
bool StompProtocol::hasErrorOccurred() { 
    std::lock_guard<std::mutex> lock(errorMutex);
    return errorOccured; 
} 

void StompProtocol::signalStopCommunication() { stopCommunication = true; } // Signal communication thread to stop

bool StompProtocol::shouldStopCommunication() const { return stopCommunication; } // Check stop flag

// Sends a STOMP frame with given command, headers, and body.
void StompProtocol::send(const std::string& command, const std::map<std::string, std::string>& headers, const std::string& body) {
    if (!connected && command != "CONNECT") {
        std::cerr << "Cannot send frame: Not connected to server!" << std::endl;
        return;
    }

    std::stringstream frame;
    frame << command << "\n";

    // Append headers to the frame.
    for (const auto& [key, value] : headers) {
        frame << key << ":" << value << "\n";
    }

    if (!body.empty()) {
        frame << "\n" << body << "\0"; // Separate headers from body and add STOMP null terminator.
    } else {
        frame << "\n\0"; // Add STOMP null terminator.
    }

    std::string frameStr = frame.str();

    // std::cout << "Sending frame: " << frameStr << std::endl; // Add logging

    // Send the frame to the server using the connection handler.
    connectionHandler.sendFrameAscii(frameStr, '\0'); // Use sendFrameAscii to add the null character
}

// Parses and processes an incoming STOMP frame from the server.
void StompProtocol::parseFrame(const std::string& message) {
    std::istringstream stream(message);
    std::string line, command;
    std::map<std::string, std::string> headers;
    std::string body;

    std::getline(stream, command); // Extracts the command (first line).

    // Extract headers until an empty line is encountered.
    while (std::getline(stream, line) && !line.empty()) {
        size_t delimiter = line.find(":");
        if (delimiter != std::string::npos) {
            headers[line.substr(0, delimiter)] = line.substr(delimiter + 1);
        }
    }

    std::getline(stream, body, '\0'); // Extracts body content (if any).

    // Determine which handler to call based on the command type.
    if (command == "CONNECTED") {
        handleConnected();
    } else if (command == "MESSAGE") {
        handleMessage(headers, body);
    } else if (command == "ERROR") {
        handleError(headers, body);
    } else if (command == "RECEIPT") {
        handleReceipt(headers);
    }
}

// Handles CONNECTED frame, confirming successful login.
void StompProtocol::handleConnected() {
    setConnected(true);
    std::cout << "Login successful" << std::endl;
}

// Handles MESSAGE frames, extracting and storing received event information.
void StompProtocol::handleMessage(const std::map<std::string, std::string>& headers, const std::string& body) {
    std::string destination = headers.at("destination"); // Extracts topic destination.

    // std::cout << "New message received in " << destination << ":\n" << body << std::endl;

    Event newEvent(body); // Parses the body as an Event object.
    eventSummary[destination].push_back(newEvent); // Stores the event.
}

// Handles ERROR frames by displaying error details.
void StompProtocol::handleError(const std::map<std::string, std::string>& headers, const std::string& body) {
    std::cerr << "ERROR received from server:\n";
    for (const auto& [key, value] : headers) {
        std::cerr << key << ": " << value << std::endl;
    }
    std::cerr << body << std::endl;

    // Signal communication thread to stop
    signalStopCommunication();

    // Mutex scope starts here
    std::lock_guard<std::mutex> lock(errorMutex);
    errorOccured = true;
    // Mutex scope ends here
}

// Handles RECEIPT frames by confirming successful message delivery.
void StompProtocol::handleReceipt(const std::map<std::string, std::string>& headers) {
    if (headers.find("receipt-id") != headers.end()) {
        int receiptId = std::stoi(headers.at("receipt-id"));

        // Check if we stored this receipt ID
        if (receiptMap.find(receiptId) != receiptMap.end()) {
            std::string requestType = receiptMap[receiptId];
              if (requestType == "Logout") {

                std::cout << "Logged out" << std::endl;

                // Signal communication thread to stop
                signalStopCommunication();

            } else {
                std::cout << requestType << std::endl;
            }

            // Remove from map since it's processed
            receiptMap.erase(receiptId);
        } else {
            std::cout << "Received an unknown RECEIPT ID: " << receiptId << std::endl;
        }
    } else {
        std::cout << "Received a RECEIPT frame, but no receipt ID was provided." << std::endl;
    }
}
// Converts an epoch timestamp into a formatted date-time string.
std::string StompProtocol::epochToDate(int epochTime) const {
    std::time_t time = static_cast<std::time_t>(epochTime);
    std::tm *tm = std::localtime(&time);
    std::ostringstream oss;
    oss << std::put_time(tm, "%d/%m/%y %H:%M");
    return oss.str();
}


// Method to generate summary output 
void StompProtocol::summarizeEmergencyChannel(const std::string& channel, const std::string& user, const std::string& filePath) {
    std::vector<Event> relevantEvents;

    // Check if the channel exists and filter events by user
    if (eventSummary.find(channel) != eventSummary.end()) {
        for (const Event& event : eventSummary[channel]) {
            if (event.getEventOwnerUser() == user) {
                relevantEvents.push_back(event);
            }
        }
    }

    // Open file for writing (overwrite mode)
    std::ofstream outFile(filePath);
    if (!outFile) {
        std::cerr << "Error: Could not open file " << filePath << " for writing." << std::endl;
        return;
    }

    // Print header
    outFile << "Channel " << channel << "\n";
    outFile << "Stats:\n";
    outFile << "Total: " << relevantEvents.size() << "\n";

    // Print event reports header
    outFile << "\nEvent Reports:\n";

    if (!relevantEvents.empty()) {
        // Sort events by date_time, then by name lexicographically
        std::sort(relevantEvents.begin(), relevantEvents.end(), [](const Event& a, const Event& b) {
            // Sort lexicographically if time is the same
            if (a.get_date_time() == b.get_date_time()) {
                return a.get_name() < b.get_name();
            }
            // Sort by time if time is different
            return a.get_date_time() < b.get_date_time();
        });

        // Set description to be 27 chars max
        for (size_t i = 0; i < relevantEvents.size(); i++) {
            const Event& event = relevantEvents[i];
            std::string shortDescription = event.get_description().substr(0, 27);
            if (event.get_description().length() > 30) {
                shortDescription += "...";
            }

            outFile << "Report_" << (i + 1) << ":\n";
            outFile << "city: " << event.get_city() << "\n";
            outFile << "date time: " << epochToDate(event.get_date_time()) << "\n";
            outFile << "event name: " << event.get_name() << "\n";
            outFile << "summary: " << shortDescription << "\n";
        }
    }

    std::cout << "Summary successfully written to " << filePath << std::endl;
}
