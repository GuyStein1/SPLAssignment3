package bgu.spl.net.impl.stomp;

import bgu.spl.net.srv.Server;

/**
 * - Reads command-line arguments to determine whether to run in TPC or Reactor mode.
 * - Creates and starts the appropriate server type.
 */
public class StompServer {

    public static void main(String[] args) {
        if (args.length != 2) {
            System.out.println("Please provide a port and server type (tpc/reactor): <port> <server-type>");
            return;
        }

        int port = Integer.parseInt(args[0]);
        String serverType = args[1];

        Server<StompFrame> server;

        if (serverType.equalsIgnoreCase("tpc")) {
            // Thread-Per-Client using `Server.threadPerClient()`
            server = Server.<StompFrame>threadPerClient(
                port,
                () -> new StompMessagingProtocolImpl(),
                () -> new StompMessageEncoderDecoder()
            );
        } else if (serverType.equalsIgnoreCase("reactor")) {
            // Reactor using `Server.reactor()`
            server = Server.<StompFrame>reactor(
                Runtime.getRuntime().availableProcessors(), 
                port,
                () -> new StompMessagingProtocolImpl(),
                () -> new StompMessageEncoderDecoder()
            );
        } else {
            System.out.println("Invalid server type. Use 'tpc' or 'reactor'.");
            return;
        }

        // Start the selected server
        server.serve();
    }
}
