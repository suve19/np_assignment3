# np_assignment3


Adapt the Makefile to suit your solution, i.e. C or C++ compiler.
Requirement is that make and make clean operates.

Server binary must be called cserverd.
Client binary must be called cchat.

main_curses.c
	Contains examples on how to use regexec and ncurses.

test_client.pl
	Is a tool to assist in the client testing.
	Requires a proper server, use reference server.

	Usage: test_client.pl serverIP serverPort logfile.

	Operations:
	1. Start 'logger' client (nc serverIP serverPort),
	to log output from server and clients.
	
	2. Start test_client.
	It launches N clients towards the <serverIP>:<serverPort>.
	These send messages peridodically, from Ipsum Loreum text.
	Reads replies, then sends again (after a timeout).

	3. Verify that 'logger' sees the messages properly from
	the test_client.

	4. Start your client. Send some messages. See that it works.

	Note; in my 'test' I have access to the server as well, and
	I see the messages, as well as a hex dump of them. So, its
	EASY to see if you are sending a full message (padded with
	zeros), cf. 'Hello!\n\x00\x00\x00\x00......\x00'.
	
	Important part, client should be able to read and send
	messages according to standard. React properly, if the
	server sends an ERROR. 



test_server.pl
	Basic test of your server.
	At the moment it tests NICK name handling and multiple
	clients (configurable).

	Usage: test_server.pl serverIP serverPort logfile.

	Operations:
	1. Start your server, cf. ./cserverd 127.0.0.1:5000

	2. Start the test_server.pl 127.0.0.1 5000 myLog

	The tool writes parts of the result in STDOUT, some in
	the log file (myLog in example).

	The output should in the best of worlds be, something
	like:
	"Im the departing subroutinge, Errors: 0"
	"<nothing printed here>".

	But the log should also be tested.

