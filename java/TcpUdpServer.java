import java.io.*;
import java.net.*;

public class TcpUdpServer {
    public static void main( String[] args ) throws IOException {
	if ( args.length == 2 ) {
	    int port = 0;
	    try {
		port = Integer.parseInt( args[1] );
	    } catch ( Exception e ) {
	    }
	    if ( port > 0 ) {
		if ( args[0].equals( "udp" ) ) {
		    udp( port );
		    System.exit( 0 );
		}
		else if ( args[0].equals( "tcp" ) ) {
		    tcp( port );
		    System.exit( 0 );
		}
	    }
	}
	System.err.println( "Usage: java Server udp|tcp port" );
    }

    private static void udp( int port ) throws IOException{
	DatagramSocket socket = new DatagramSocket( port );
	while ( true ) {
	    byte[] buf = new byte[256];
	    DatagramPacket p = new DatagramPacket( buf, buf.length );
	    socket.receive( p );
	    String s = new String( p.getData( ), 0, p.getLength( ) );
	    System.out.println( p.getAddress( ).getHostName( ) + ": " + s );
	}
    }

    private static void tcp( int port ) throws IOException {
	ServerSocket server = new ServerSocket( port );
	while ( true ) {
	    Socket socket = server.accept( );
	    InputStream input = socket.getInputStream( );
	    byte[] buf = new byte[256];
	    int nRead = input.read( buf );
	    String s = new String( buf, 0, nRead );
	    System.out.println( socket.
				getInetAddress( ).getHostName( ) + 
				": " + s );
	}
    }
}
