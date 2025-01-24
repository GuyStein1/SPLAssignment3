#include <iostream>
#include <thread>
#include <mutex>
#include "StompProtocol.h"
#include "ConnectionHandler.h"
#include "keyboardInput.h"

std::mutex mutex; // Ensures thread safety when modifying shared objects


// Listens for incoming STOMP messages from the server, used for the thread in charge of communication
void communicate(StompProtocol*& protocol, ConnectionHandler*& connectionHandler) {
    std::string response;
    while (!protocol->shouldStopCommunication()) {

        response.clear();  // Ensure response is empty before reading a new frame

        if (!connectionHandler->getFrameAscii(response, '\0')) {
            std::cerr << "Server connection lost." << std::endl;
            protocol->signalStopCommunication();
            break;
        }

        protocol->parseFrame(response);
    }

    // Exiting loop means logged out or error occured.

    // Store if error occured before deleteing protocol.
    bool error = protocol->hasErrorOccurred();

    // Close the socket
    connectionHandler->close();

    // Clean up resources
    delete connectionHandler;
    connectionHandler = nullptr;  // Prevent dangling pointer

    delete protocol;
    protocol = nullptr;  // Prevent dangling pointer

    // Terminate program if error occured
    if (error){
        std::terminate(); // Exit the program
    }
}

int main(int argc, char *argv[]) {
    ConnectionHandler* connectionHandler = nullptr; // Pointer to manage connection
    StompProtocol* protocol = nullptr; // Pointer to manage STOMP protocol

    std::thread communicator; // Communication thread

    std::string username; // Username for the current session

    std::string userInput;
    while (true) {

        try
        {
            userInput = KeyboardInput::readLine();
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error reading input: " << e.what() << std::endl;
            continue; // Prevent crash, allow user to retry
        }

        std::vector<std::string> tokens;
        KeyboardInput::split_str(userInput, ' ', tokens); // Use split_str to parse user input

		// Extract first word in the input to know the type of command
        std::string command = tokens[0];

        if (tokens.empty()) continue;

        if (command == "login") {

            // Make sure the command has the correct number of arguments
            if (tokens.size() != 4) {
                std::cerr << "login command needs 3 args: {host:port} {username} {password}" << std::endl;
                continue;
            }

            // Make sure user is not already logged in
            if (protocol && protocol->isConnected()) {
                std::cerr << "user already logged in" << std::endl;
                continue;
            }

            // Extract host and port
            std::string hostPort = tokens[1];
            size_t colonPosition = hostPort.find(':');
			// If ':' isnt found it is in the wrong format
            if (colonPosition == std::string::npos) {
                std::cerr << "Invalid host:port format" << std::endl;
                continue;
            }
            std::string serverHost = hostPort.substr(0, colonPosition);
			// Convert port string to short
            short serverPort = std::stoi(hostPort.substr(colonPosition + 1));

			// Extract username and password
            username = tokens[2];
            std::string password = tokens[3];

            // Create connectionHandler and protocol
            connectionHandler = new ConnectionHandler(serverHost, serverPort);
            protocol = new StompProtocol(*connectionHandler);

            // Connect to server
            if (!connectionHandler->connect()) {
                std::cerr << "Could not connect to server: Make sure server is running, ip and host are correct, and that you have internet connection." << std::endl;
                // Cleanup
                delete connectionHandler;
                delete protocol;
                connectionHandler = nullptr;
                protocol = nullptr;
                continue;
            }

            // Send CONNECT frame
            std::map<std::string, std::string> headers = {
                {"accept-version", "1.2"},
                {"host", "stomp.cs.bgu.ac.il"},
                {"login", username},
                {"passcode", password}
            };
            protocol->send("CONNECT", headers, "");

            // Start communication thread with pointer references
            communicator = std::thread(communicate, std::ref(protocol), std::ref(connectionHandler));
        }

        else if (command == "join") {

            // Check if the correct number of arguments is provided
            if (tokens.size() != 2) {
                std::cerr << "join command needs 1 args: {channel_name}" << std::endl;
                continue;
            }
            // Check if the user is logged in (connected to the STOMP server)
            if (!protocol || !protocol->isConnected()) {
                std::cerr << "Please login first" << std::endl;
                continue;
            }

            // Generate unique IDs for subscription and receipt
            int subscriptionId = protocol->getNextId();
            int receiptId = protocol->getNextReceiptId();

            // Create the SUBSCRIBE frame headers
            std::map<std::string, std::string> headers = {
                {"destination", tokens[1]},
                {"id", std::to_string(subscriptionId)},
                {"receipt", std::to_string(receiptId)}
            };

            // Store the receipt mapping
            protocol->storeReceipt(receiptId, "Joined channel " + tokens[1]);

            // Store the subscription ID for this channel
            protocol->storeSubscriptionId(tokens[1], subscriptionId);  

            // Send the SUBSCRIBE frame to the server
            protocol->send("SUBSCRIBE", headers, "");
        }

        else if (command == "exit") {

            // Check argument count
            if (tokens.size() != 2) {
                std::cerr << "exit commmand needs 1 args: {channel_name}" << std::endl;
                continue;
            }

            // Check if the user is logged in
            if (!protocol || !protocol->isConnected()) {
                std::cerr << "Please login first" << std::endl;
                continue;
            }

 

            std::string channel = tokens[1];

            // Generate unique receipt ID
            int receiptId = protocol->getNextReceiptId();

            // Find the subscription ID for this channel (assuming we stored them)
            int subscriptionId = protocol->getSubscriptionId(channel);  // New function to retrieve it

            // Means not subscribed to this channel, can't unsubscribe.
            if (subscriptionId == -1) {
                std::cerr << "you are not subscribed to channel " << channel << std::endl;
                continue;
            }

            // Prepare UNSUBSCRIBE frame
            std::map<std::string, std::string> headers = {
                {"id", std::to_string(subscriptionId)},   // Unique subscription ID
                {"receipt", std::to_string(receiptId)}   // Unique receipt ID for confirmation
            };

            // Store the receipt mapping to track the request
            protocol->storeReceipt(receiptId, "Exited channel " + channel);

            // Send the UNSUBSCRIBE frame
            protocol->send("UNSUBSCRIBE", headers, "");
        }

        else if (command == "report") {

            // Check if the correct number of arguments is provided
            if (tokens.size() != 2) {
                std::cerr << "report command needs 1 args: {file}" << std::endl;
                continue;
            }

            // Check if the user is logged in
            if (!protocol || !protocol->isConnected()) {
                std::cerr << "Please login first" << std::endl;
                continue;
            }

            // Parse the events from the provided file
            names_and_events parsedEvents = parseEventsFile(tokens[1]);

            for (const Event &event : parsedEvents.events) {
                // Prepare the headers for the SEND frame
                std::map<std::string, std::string> headers = {
                    {"destination", parsedEvents.channel_name} // Send to the correct channel
                };

                // Construct the body in the correct format
                std::string body = "user:" + username + "\n" +
                                "city:" + event.get_city() + "\n" +
                                "event name:" + event.get_name() + "\n" +
                                "date time:" + std::to_string(event.get_date_time()) + "\n" +
                                "general information:\n";

                for (std::map<std::string, std::string>::const_iterator it = event.get_general_information().begin(); 
                    it != event.get_general_information().end(); ++it) {
                    const std::string& key = it->first;
                    const std::string& value = it->second;
                    body += " " + key + ":" + value + "\n";  // Ensure proper formatting
                }

                body += "description:\n" + event.get_description() + "\n";

                // Send the formatted SEND frame to the server
                protocol->send("SEND", headers, body);
            }

            // Print when finished
            std::cout << "reported" << std::endl;
        }

        else if (command == "summary") {

            // Check argument count
            if (tokens.size() != 4) {
                std::cerr << "summary command needs 3 args: {channel_name} {user} {file}" << std::endl;
                continue;
            }

            // Check if the user is logged in
            if (!protocol || !protocol->isConnected()) {
                std::cerr << "Please login first" << std::endl;
                continue;
            }

            // Path to bin folder
            std::string binPath = "../bin/" + tokens[3];

            // Call the summarize function with the correct file path
            protocol->summarizeEmergencyChannel(tokens[1], tokens[2], binPath);
        }

        else if (command == "logout") {
            // Check if the user is logged in
            if (!protocol || !protocol->isConnected()) {
                std::cerr << "Not logged in" << std::endl;
                continue;
            }

            // Generate a unique receipt ID
            int receiptId = protocol->getNextReceiptId();

            // Prepare the DISCONNECT frame with the receipt ID
            std::map<std::string, std::string> headers = {
                {"receipt", std::to_string(receiptId)}
            };

            // Store the receipt ID with a "Logout" request type
            protocol->storeReceipt(receiptId, "Logout");

            // Send the DISCONNECT frame to the server
            protocol->send("DISCONNECT", headers, "");

            // Wait for the communication thread to close and clean up resources, 
            // which is done when the server sends a RECEIPT frame for the discconect request (in the protocol)
            communicator.join();
        }

        else {
            std::cerr << "Unknown command: " << command << std::endl;
        }
    }

    return 0;
}
