import java.io.*;
import java.net.*;

public class MulticastClient {
  public static void main(String[] args) throws IOException {
      if ( args.length == 2 ) {
	  String[] ipPair = args[0].split( ":" );
	  if ( ipPair.length == 2 ) {
	      String multicast_group = ipPair[0];
	      int port = -1;
	      try {
		  port = Integer.parseInt( ipPair[1] );
	      } catch ( NumberFormatException e ) {
	      }
	      if ( port >= 0 ) {
		  if ( port == 0 ) {
		      // default
		      port = 50763;
		  }
		  if ( multicast_group.equals( "0" ) ) {
		      // default
		      multicast_group = "239.255.255.255";
		  }
		  InetAddress addr = InetAddress.getByName( multicast_group );
		  byte[]      buf  = args[1].getBytes( );
		  DatagramPacket p = 
		      new DatagramPacket( buf, buf.length, addr, port );

		  MulticastSocket socket = new MulticastSocket( );
		  socket.send( p );
		  socket.close( );
		  System.exit( 0 );
	      }
	  }
      }
      System.err.println( "usage: java MulticastClient groupIp:port message" );
  }
}
