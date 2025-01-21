#include <iostream>
#include <thread>
#include <mutex>
#include "StompProtocol.h"
#include "ConnectionHandler.h"
#include "keyboardInput.h"

#include <filesystem>  // Required for path handling

std::mutex mutex; // Ensures thread safety when modifying shared objects

// Listens for incoming STOMP messages from the server, used for the thread in charge of communication
void listenToServer(StompProtocol &protocol, ConnectionHandler &connectionHandler) {
    std::string response;
    while (connectionHandler.getLine(response)) { // Continuously reads messages from server
        std::lock_guard<std::mutex> lock(mutex); // Ensures thread safety
        protocol.parseFrame(response); // Processes the received STOMP frame
    }
}

int main(int argc, char *argv[]) {
    ConnectionHandler* connectionHandler = nullptr; // Pointer to manage connection
    StompProtocol* protocol = nullptr; // Pointer to manage STOMP protocol
    std::thread serverListener; // Thread for handling server messages

    std::string userInput;
    while (true) {
        userInput = KeyboardInput::readLine();
        std::vector<std::string> tokens;
        KeyboardInput::split_str(userInput, ' ', tokens); // Use split_str to parse user input

        if (tokens.empty()) continue;

		// Extract first word in the input to know the type of command
        std::string command = tokens[0];

        if (command == "login") {
            // Make sure user is not already logged in
            if (protocol && protocol->isConnected()) {
                std::cerr << "user already logged in" << std::endl;
                continue;
            }
            // Make sure the command has the correct number of arguments
            if (tokens.size() != 3) {
                std::cerr << "login command needs 3 args: {host:port} {username} {password}" << std::endl;
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
			// Convert port string to int
            int serverPort = std::stoi(hostPort.substr(colonPosition + 1));

			// Extract username and password
            std::string username = tokens[2];
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

            // Start listener thread for server responses
            serverListener = std::thread(listenToServer, std::ref(*protocol), std::ref(*connectionHandler));
        }

        else if (command == "join") {
            // Check if the user is logged in (connected to the STOMP server)
            if (!protocol || !protocol->isConnected()) {
                std::cerr << "Please login first" << std::endl;
                continue;
            }

            // Check if the correct number of arguments is provided
            if (tokens.size() != 2) {
                std::cerr << "join command needs 1 args: {channel_name}" << std::endl;
                continue;
            }

            // Generate unique IDs for subscription and receipt
            int subscriptionId = protocol->getNextId();
            int receiptId = protocol->getNextReceiptId();

            // Create the SUBSCRIBE frame headers
            std::map<std::string, std::string> headers = {
                {"destination", "/" + tokens[1]},
                {"id", std::to_string(subscriptionId)},
                {"receipt", std::to_string(receiptId)}
            };

            // Store the receipt mapping
            protocol->storeReceipt(receiptId, "Joined channel " + tokens[1]);

            // Send the SUBSCRIBE frame to the server
            protocol->send("SUBSCRIBE", headers, "");
        }

        else if (command == "exit") {
            // Check if the user is logged in
            if (!protocol || !protocol->isConnected()) {
                std::cerr << "Please login first" << std::endl;
                continue;
            }

            // Check argument count
            if (tokens.size() != 2) {
                std::cerr << "Usage: exit {channel_name}" << std::endl;
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
            // Check if the user is logged in
            if (!protocol || !protocol->isConnected()) {
                std::cerr << "Please login first" << std::endl;
                continue;
            }

            // Check if the correct number of arguments is provided
            if (tokens.size() != 2) {
                std::cerr << "report command needs 1 args: {file}" << std::endl;
                continue;
            }

            // Parse the events from the provided file
            names_and_events parsedEvents = parseEventsFile(tokens[1]);

            for (const Event &event : parsedEvents.events) {
                // Prepare the headers for the SEND frame
                std::map<std::string, std::string> headers = {
                    {"destination", "/" + parsedEvents.channel_name} // Send to the correct channel
                };

                // Construct the body in the correct format
                std::string body = "user:" + event.getEventOwnerUser() + "\n" +
                                "city:" + event.get_city() + "\n" +
                                "event name:" + event.get_name() + "\n" +
                                "date time:" + std::to_string(event.get_date_time()) + "\n" +
                                "general information:\n";

                for (const auto &[key, value] : event.get_general_information()) {
                    body += key + ":" + value + "\n";  // Ensure proper formatting
                }

                body += "description:\n" + event.get_description() + "\n";

                // Send the formatted SEND frame to the server
                protocol->send("SEND", headers, body);

                // Print when finished
                std::cout << "reported" << std::endl;
            }
        }

        else if (command == "summary") {
            // Check if the user is logged in
            if (!protocol || !protocol->isConnected()) {
                std::cerr << "Please login first" << std::endl;
                continue;
            }

            // Check argument count
            if (tokens.size() != 4) {
                std::cerr << "summary command needs 3 args: {channel_name} {user} {file}" << std::endl;
                continue;
            }

            // Determine the full path to the bin folder in client/
            std::string binPath = (std::filesystem::current_path().parent_path() / "bin" / tokens[3]).string();

            // Call the summarize function with the correct file path
            protocol->summarizeEmergencyChannel(tokens[1], tokens[2], binPath);
        }

        else if (command == "logout") {
            if (!loggedIn) {
                std::cerr << "Not logged in" << std::endl;
                continue;
            }
            std::map<std::string, std::string> headers = {{"receipt-id", "disconnect"}};
            {
                std::lock_guard<std::mutex> lock(mutex);
                protocol->send("DISCONNECT", headers, "");
            }
            loggedIn = false;
            serverListener.join(); // Ensure the listener thread finishes before exiting
            delete connectionHandler;
            delete protocol;
            connectionHandler = nullptr;
            protocol = nullptr;
            break; // Exit the loop and terminate the program
        }

        else {
            std::cerr << "Unknown command: " << command << std::endl;
        }
    }

    return 0;
}
