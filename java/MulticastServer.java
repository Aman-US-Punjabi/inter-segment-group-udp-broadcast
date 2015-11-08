import java.io.*;
import java.net.*;

public class MulticastServer {
    public static void main(String[] args) throws IOException {
	if ( args.length == 1 ) {
	    String[] ipPair = args[0].split( ":" );
	    if (ipPair.length ==2 ) {
		String multicast_group =ipPair[0];
		int port = -1;
		try {
		    port = Integer.parseInt( ipPair[1] );
		} catch (NumberFormatException e ) {
		}
		if ( port >= 0 ) {
		    if ( port == 0 ) {
			// default
			port = 50763;
		    }
		    if ( multicast_group.equals( "0" ) ) {
			// default
			multicast_group= "239.255.255.255";
		    }
		    InetAddress group 
			= InetAddress.getByName( multicast_group );
		    MulticastSocket socket = new MulticastSocket( port );
		    socket.joinGroup( group );

		    while ( true ) {
			byte[] buf = new byte[256];
			DatagramPacket p 
			    = new DatagramPacket( buf, buf.length );
			socket.receive( p );
			
			int offset = 
			    ( buf[0]==-32 && buf[1]==-31 && buf[2]==-30 &&
			      buf[3] > 0 && buf[3] <= 10 ) ?
			    ( buf[3] + 1 ) * 4 : 0;
			
			String s = new String( buf, offset, 
					       p.getLength( ) - offset );
			System.out.println( p.getAddress( ).getHostName( ) + 
					    ": " + s );
		    }
		}
	    }
	}
	System.err.println( "usage: java MulticastServer groupIp:port " );
    }
}
