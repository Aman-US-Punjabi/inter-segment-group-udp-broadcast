import java.io.*;
import java.net.*;

public class BroadcastClient {
  public static void main(String[] args) throws IOException {
      if ( args.length == 2 ) {
	  String[] ipPair = args[0].split( ":" );
	  if ( ipPair.length == 2 ) {
	      String broadcast_group = ipPair[0];
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
		  if ( broadcast_group.equals( "0" ) ) {
		      // default
		      broadcast_group = "239.255.255.255";
		  }
		  InetAddress addr = InetAddress.getByName( broadcast_group );
		  byte[]      buf  = args[1].getBytes( );
		  DatagramPacket p = 
		      new DatagramPacket( buf, buf.length, addr, port );

		  BroadcastSocket socket = new BroadcastSocket( );
		  socket.send( p );
		  socket.close( );
		  System.exit( 0 );
	      }
	  }
      }
      System.err.println( "usage: java BroadcastClient groupIp:port message" );
  }
}
