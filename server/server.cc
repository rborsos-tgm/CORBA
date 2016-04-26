#include <echo.hh>

#ifdef HAVE_STD
#  include <iostream>
using namespace std;
#else
#  include <iostream.h>
#endif

static CORBA::Boolean bindObjectToName(CORBA::ORB_ptr, CORBA::Object_ptr);

static CORBA::ORB_ptr orb;
static int            dying = 0;
static int            num_active_servers = 0;
static omni_mutex     mu;
static omni_condition sigobj(&mu);

//////////////////////////////////////////////////////////////////////

/**
	A thread object used to server clients registered
	using the cb::Server::register() method.
*/
class server_thread : public omni_thread {
public:
	inline server_thread(cb::CallBack_ptr client, const char* mesg, int period)
		: pd_client(cb::CallBack::_duplicate(client)),
		pd_mesg(mesg), pd_period(period) {}

	virtual void run(void* arg);

private:
	cb::CallBack_var  pd_client;
	CORBA::String_var pd_mesg;
	int               pd_period;
};

/**
 Periodically calls the call_back.
*/
void
server_thread::run(void* arg)
{
	try {
		while (!dying) {
			omni_thread::sleep(pd_period);
			pd_client->call_back(pd_mesg);
		}
	}
	catch (...) {
		cout << "cb_server: Lost a client!" << endl;
	}

	cout << "cb_server: Worker thread is exiting." << endl;

	mu.lock();
	int do_signal = --num_active_servers == 0;
	mu.unlock();
	if (do_signal)  sigobj.signal();
}

//////////////////////////////////////////////////////////////////////

/**
	Implementation of cb::Server.
*/
class server_i : public POA_cb::Server
{
public:
	inline server_i() {}
	virtual ~server_i();


	virtual void one_time(cb::CallBack_ptr cb, const char* mesg);

	// NB. Because 'register' is a C++ keyword, we have to
	// use the prefix _cxx_ here.
	virtual void _cxx_register(cb::CallBack_ptr cb, const char* mesg,
		CORBA::UShort period_secs);

	virtual void shutdown();
};

/**
	destructor
*/
server_i::~server_i()
{
	cout << "cb_server: The server object has been destroyed." << endl;
}

/**
	Calls the callback a single time.

	@param cb Callback reference
	@param mesg the received message
*/
void
server_i::one_time(cb::CallBack_ptr cb, const char* mesg)
{
	if (CORBA::is_nil(cb)) {
		cerr << "Received a nil callback.\n";
		return;
	}

	cout << "cb_server: Doing a single call-back: " << mesg << endl;

	cb->call_back(mesg);
}

/**
	Registers the callbacks reference and starts a new server_thread that periodically calls the callback.

	@param cb the callbacks reference
	@param mesg the received message
	@param period_secs amount of seconds between the calls
*/
void
server_i::_cxx_register(cb::CallBack_ptr cb, const char* mesg,
	CORBA::UShort period_secs)
{
	if (CORBA::is_nil(cb)) {
		cerr << "Received a nil callback.\n";
		return;
	}

	cout << "cb_server: Starting a new worker thread" << endl;

	mu.lock();
	num_active_servers++;
	mu.unlock();

	server_thread* server = new server_thread(cb, mesg, period_secs);
	server->start();
}

/**
	Shuts down the server
*/
void
server_i::shutdown()
{
	omni_mutex_lock sync(mu);

	if (!dying) {
		cout << "cb_server: I am being shutdown!" << endl;

		// Tell the servers to exit, and wait for them to do so.
		dying = 1;

		while (num_active_servers > 0)  sigobj.wait();

		// Shutdown the ORB (but do not wait for completion).  This also
		// causes the main thread to unblock from CORBA::ORB::run().
		orb->shutdown(0);
	}
}

//////////////////////////////////////////////////////////////////////

/**
	Main function in order to start the application

	@param argc number of arguments
	@param argv arguments
*/
int main(int argc, char** argv)
{
	try {
		orb = CORBA::ORB_init(argc, argv);

		{
			// get the rootPoas reference
			CORBA::Object_var obj = orb->resolve_initial_references("RootPOA");
			PortableServer::POA_var poa = PortableServer::POA::_narrow(obj);

			// new server object
			server_i* myserver = new server_i();

			//PortableServer::ObjectId_var myechoid = poa->activate_object(myecho);

			obj = myserver->_this();             // *implicit activation*

			CORBA::String_var x;
			x = orb->object_to_string(obj);
			cout << x << endl;

			if (!bindObjectToName(orb, obj))
				return 1;

			myserver->_remove_ref();
			PortableServer::POAManager_var pman = poa->the_POAManager();
			pman->activate();

			orb->run();
		}

		cout << "cb_server: Returned from orb->run()." << endl;
		orb->destroy();
	}
	catch (CORBA::SystemException& ex) {
		cerr << "Caught CORBA::" << ex._name() << endl;
	}
	catch (CORBA::Exception& ex) {
		cerr << "Caught CORBA::Exception: " << ex._name() << endl;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////

/**
	Registers an object's reference in the naming service
*/
static CORBA::Boolean
bindObjectToName(CORBA::ORB_ptr orb, CORBA::Object_ptr objref)
{
	CosNaming::NamingContext_var rootContext;

	try {
		// Obtain a reference to the root context of the Name service:
		CORBA::Object_var obj;
		obj = orb->resolve_initial_references("NameService");

		// Narrow the reference returned.
		rootContext = CosNaming::NamingContext::_narrow(obj);
		if (CORBA::is_nil(rootContext)) {
			cerr << "Failed to narrow the root naming context." << endl;
			return 0;
		}
	}
	catch (CORBA::NO_RESOURCES&) {
		cerr << "Caught NO_RESOURCES exception. You must configure omniORB "
			<< "with the location" << endl
			<< "of the naming service." << endl;
		return 0;
	}
	catch (CORBA::ORB::InvalidName&) {
		// This should not happen!
		cerr << "Service required is invalid [does not exist]." << endl;
		return 0;
	}

	try {
		// Bind a context called "test" to the root context:

		CosNaming::Name contextName;
		contextName.length(1);
		contextName[0].id = (const char*) "test";       // string copied
		contextName[0].kind = (const char*) "my_context"; // string copied
		// Note on kind: The kind field is used to indicate the type
		// of the object. This is to avoid conventions such as that used
		// by files (name.type -- e.g. test.ps = postscript etc.)

		CosNaming::NamingContext_var testContext;
		try {
			// Bind the context to root.
			testContext = rootContext->bind_new_context(contextName);
		}
		catch (CosNaming::NamingContext::AlreadyBound& ex) {
			// If the context already exists, this exception will be raised.
			// In this case, just resolve the name and assign testContext
			// to the object returned:
			CORBA::Object_var obj;
			obj = rootContext->resolve(contextName);
			testContext = CosNaming::NamingContext::_narrow(obj);
			if (CORBA::is_nil(testContext)) {
				cerr << "Failed to narrow naming context." << endl;
				return 0;
			}
		}

		// Bind objref with name Echo to the testContext:
		CosNaming::Name objectName;
		objectName.length(1);
		objectName[0].id = (const char*) "Echo";   // string copied
		objectName[0].kind = (const char*) "Object"; // string copied

		try {
			testContext->bind(objectName, objref);
		}
		catch (CosNaming::NamingContext::AlreadyBound& ex) {
			testContext->rebind(objectName, objref);
		}
		// Note: Using rebind() will overwrite any Object previously bound
		//       to /test/Echo with obj.
		//       Alternatively, bind() can be used, which will raise a
		//       CosNaming::NamingContext::AlreadyBound exception if the name
		//       supplied is already bound to an object.

		// Amendment: When using OrbixNames, it is necessary to first try bind
		// and then rebind, as rebind on it's own will throw a NotFoundexception if
		// the Name has not already been bound. [This is incorrect behaviour -
		// it should just bind].
	}
	catch (CORBA::TRANSIENT& ex) {
		cerr << "Caught system exception TRANSIENT -- unable to contact the "
			<< "naming service." << endl
			<< "Make sure the naming server is running and that omniORB is "
			<< "configured correctly." << endl;

		return 0;
	}
	catch (CORBA::SystemException& ex) {
		cerr << "Caught a CORBA::" << ex._name()
			<< " while using the naming service." << endl;
		return 0;
	}
	return 1;
}