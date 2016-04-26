package cb;

import org.omg.CosNaming.*;
import org.omg.CORBA.*;
import org.omg.PortableServer.*;

/**
 * Simple client application that invokes remote methods using CORBA.
 * 
 * Created: Mon Sep 3 19:28:34 2001
 *
 * @author Nicolas Noffke
 * @version 20160328.1
 */
public class Client extends CallBackPOA {
	/**
	 * constructor
	 */
	public Client() {
	}

	/**
	 * Method to receive messages from the server using the client side remote
	 * object. Message will be printed out.
	 *
	 * @param message
	 *            the received message
	 */
	public void call_back(String message) {
		System.out.println("Client callback object received hello message >" + message + '<');
	}

	/**
	 * Main function that starts the client application. Calls the remote method
	 * one_time and registers 2 periodically invoked callbacks.
	 *
	 * @param args
	 *            command line arguments
	 * @throws Exception
	 *             if the server or the naming service is not available
	 */
	public static void main(String[] args) throws Exception {
		// ORB mit den Kommandazeilenargumenten initialisieren
		org.omg.CORBA.ORB orb = org.omg.CORBA.ORB.init(args, null);

		// Erhalten des RootContext des angegebenen Namingservices
		org.omg.CORBA.Object o = orb.resolve_initial_references("NameService");

		// Verwenden von NamingContextExt
		NamingContextExt rootContext = NamingContextExtHelper.narrow(o);

		// Angeben des Pfades zum Echo Objekt
		NameComponent[] name = new NameComponent[2];
		name[0] = new NameComponent("test", "my_context");
		name[1] = new NameComponent("Echo", "Object");

		// aufloesen der Objektreferenz
		Server server = ServerHelper.narrow(rootContext.resolve(name));

		// RootPoa speichern
		POA poa = POAHelper.narrow(orb.resolve_initial_references("RootPOA"));

		// aktivieren des Object Adapters
		poa.the_POAManager().activate();

		// Client Objekt uber den RootPOA zur Verfuegung stellen
		CallBack cb = CallBackHelper.narrow(poa.servant_to_reference(new Client()));

		System.out.println("Executing one-time callback.");

		// invoking remote method
		server.one_time(cb, "Hello! This is a test message.");

		System.out.println("Register periodically callback that is called every 2 seconds.");

		// invoke remote method that calls the callback every 2 seconds.
		server.register(cb, "I'm the 2 seconds callback.", (short) 2);

		System.out.println("Register periodically callback that is called every 3 seconds.");
		server.register(cb, "This is another message (3 seconds).", (short) 3);

		System.out.println("Press [ENTER] to shut down the server.");
		// waiting for keyboard interruption...
		while (System.in.read() != '\n')
			;
		// invoke the remote method to shut down the server
		server.shutdown();
	}
}