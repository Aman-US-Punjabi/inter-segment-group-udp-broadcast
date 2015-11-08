import java.io.*;
import java.net.*;

public class TcpUdpClient {
    public static void main( String[] args ) throws IOException {
	if ( args.length == 4 ) {
	    byte[] buf = args[3].getBytes( );

	    int port = 0;
	    try {
		port = Integer.parseInt( args[2] );
	    } catch ( Exception e ) {
	    }
	    if ( port > 0 ) {
		if ( args[0].equals( "udp" ) ) {
		    udp( args[1], port, buf );
		    System.exit( 0 );
		}
		else if ( args[0].equals( "tcp" ) ) {
		    tcp( args[1], port, buf );
		    System.exit( 0 );
		}
	    }
	}
	System.err.println("Ussage: java Client udp|tcp server port message");
    }

    private static void udp( String server, int port, byte[] buf ) 
	throws IOException{
	InetAddress addr = InetAddress.getByName( server );
	DatagramPacket p = new DatagramPacket( buf, buf.length, addr, port );
	DatagramSocket socket = new DatagramSocket( );
	socket.send( p );
	System.out.println( "udp sent to " + server + ":" + port );

    }

    private static void tcp( String server, int port, byte[] buf ) 
	throws IOException {
	Socket socket = new Socket( server, port );
	OutputStream output = socket.getOutputStream( );
	output.write( buf );
	System.out.println( "tcp connected to " + server + ":" + port );
    }
}
