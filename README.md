# Emergency Service Messaging Platform (SPL Assignment 3, Fall 2024)

## Overview
This project implements an **Emergency Service subscription platform** using the **STOMP (Simple-Text-Oriented Messaging Protocol)**. The system consists of two parts:

- **Server** (Java): A STOMP server handling multiple clients.
- **Client** (C++): A client that interacts with the server to subscribe to emergency channels, send reports, and receive updates.

The project simulates real-time emergency communications, allowing clients to subscribe to topics like `fire`, `police`, `medical`, and report or receive emergency events.

---

## Server (Java)

- **Technologies**: Java, Maven
- **Modes**:
  - **Thread-Per-Client (TPC)**: Each client handled in a separate thread.
  - **Reactor Pattern**: Event-driven non-blocking server.
- **Core Components**:
  - `Connections<T>` interface to manage active clients and channel messaging.
  - `ConnectionHandler<T>` interface to handle client communications.
  - `StompMessagingProtocol<T>` interface for protocol logic.
  - `StompServer` main class to launch the server.
- **Build and Run**:
  ```bash
  mvn compile
  mvn exec:java -Dexec.mainClass="bgu.spl.net.impl.stomp.StompServer" -Dexec.args="<port> <tpc/reactor>"
  ```

---

## Client (C++)

- **Technologies**: C++, Makefile
- **Architecture**:
  - `ConnectionHandler`: Manages TCP communication.
  - `StompProtocol`: Handles STOMP protocol framing and logic.
  - `Event`: Represents emergency events and parses event files.
- **Features**:
  - Connects to the server via TCP and follows the STOMP protocol.
  - Supports multithreading: One thread listens to the keyboard, the other listens to the socket.
  - Commands supported:
    - `login {host:port} {username} {password}`
    - `join {channel_name}`
    - `exit {channel_name}`
    - `report {file}`
    - `summary {channel_name} {user} {file}`
    - `logout`
- **Build and Run**:
  ```bash
  make
  ./bin/StompEMIClient
  ```

---

## Event Files
- Events are provided via JSON files.
- Events are parsed and stored using the `Event` class.
- Reports are sent and received following a predefined format.

Example:
```json
{
    "channel_name": "police",
    "events": [
        {
            "event_name": "Grand Theft Auto",
            "city": "Liberty City",
            "date_time": 1734961200,
            "description": "Pink Lampadati Felon with license plate \"STOL3N1\". White male 1.85 with black baseball hat.",
            "general_information": {
                "active": true,
                "forces_arrival_at_scene": false
            }
        }
    ]
}
```

---

## Notes
- The client handles socket disconnection gracefully.
- Event summaries sort by event time, then by event name.
- Summaries are written to output files with specific formatting and truncation rules.
- Server keeps user authentication data even after disconnects.

---

## Authors
- **Guy Stein**
- **Guy Zilberstein**
