#include <iostream>
#include <thread>
#include <mutex>
#include "StompProtocol.h"
#include "ConnectionHandler.h"
#include "keyboardInput.h"

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
    bool loggedIn = false;
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
            if (loggedIn) {
                std::cerr << "The client is already logged in, log out before trying again" << std::endl;
                continue;
            }
            if (tokens.size() != 3) {
                std::cerr << "Usage: login {host:port} {username} {password}" << std::endl;
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
            int serverPort = std::stoi(hostPort.substr(colonPosition + 1));
            std::string username = tokens[2];
            std::string password = tokens[3];

            // Create connectionHandler and protocol
            connectionHandler = new ConnectionHandler(serverHost, serverPort);
            protocol = new StompProtocol(*connectionHandler);

            if (!connectionHandler->connect()) {
                std::cerr << "Could not connect to server" << std::endl;
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
            loggedIn = true;

            // Start listener thread for server responses
            serverListener = std::thread(listenToServer, std::ref(*protocol), std::ref(*connectionHandler));
        }

        else if (command == "join") {
            if (!loggedIn || tokens.size() < 2) {
                std::cerr << "Usage: join {channel_name}" << std::endl;
                continue;
            }
            std::map<std::string, std::string> headers = {
                {"destination", "/" + tokens[1]},
                {"id", tokens[1]},
                {"receipt-id", "sub-" + tokens[1]}
            };
            std::lock_guard<std::mutex> lock(mutex);
            protocol->send("SUBSCRIBE", headers, "");
        }

        else if (command == "exit") {
            if (!loggedIn || tokens.size() < 2) {
                std::cerr << "Usage: exit {channel_name}" << std::endl;
                continue;
            }
            std::map<std::string, std::string> headers = {
                {"id", tokens[1]},
                {"receipt-id", "unsub-" + tokens[1]}
            };
            std::lock_guard<std::mutex> lock(mutex);
            protocol->send("UNSUBSCRIBE", headers, "");
        }

        else if (command == "report") {
            if (!loggedIn || tokens.size() < 2) {
                std::cerr << "Usage: report {file}" << std::endl;
                continue;
            }

            names_and_events parsedEvents = parseEventsFile(tokens[1]);
            for (const Event &event : parsedEvents.events) {
                std::map<std::string, std::string> headers = {{"destination", "/" + parsedEvents.channel_name}};
                std::string body = "user:" + event.getEventOwnerUser() + "\n" +
                                   "city:" + event.get_city() + "\n" +
                                   "event name:" + event.get_name() + "\n" +
                                   "date time:" + std::to_string(event.get_date_time()) + "\n" +
                                   "general information:\n";
                for (const auto &[key, value] : event.get_general_information()) {
                    body += key + ":" + value + "\n";
                }
                body += "description:\n" + event.get_description();
                std::lock_guard<std::mutex> lock(mutex);
                protocol->send("SEND", headers, body);
            }
        }

        else if (command == "summary") {
            if (!loggedIn || tokens.size() < 4) {
                std::cerr << "Usage: summary {channel_name} {user} {file}" << std::endl;
                continue;
            }
            std::lock_guard<std::mutex> lock(mutex);
            protocol->summarizeEmergencyChannel(tokens[1], tokens[2], tokens[3]);
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
