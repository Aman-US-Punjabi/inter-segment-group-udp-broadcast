import java.io.*;
import java.net.*;

public class BroadcastSocket extends MulticastSocket {
    // Constants
    final int alpha = -32;
    final int beta  = -31;
    final int gamma = -30;
    final int ttl_min = 1;
    final int ttl_max = 10;
    final int headerLength = 4;
    final int rawIpLength = 4;

    // Constructors
    public BroadcastSocket( ) throws IOException {
	super( );
    }

    public BroadcastSocket( int port ) throws IOException {
	super( port );
    }

    // The wrapper of send( )
    public void send( DatagramPacket p ) throws IOException {
	// create a header
	byte[] data = new byte[ 8 + p.getLength( ) ];
	data[0] = alpha;
	data[1] = beta;
	data[2] = gamma;
	data[3] = 1;

	// store the raw IP address of the souce node
	InetAddress local = InetAddress.getLocalHost( );
	byte[] rawIp = local.getAddress( );
	if ( rawIp.length != rawIpLength ) {
	    IOException e = 
		new IOException( "Local host's raw IP is not 4 bytes long" );
	    throw e;
	}
	System.arraycopy( rawIp, 0, data, headerLength, rawIpLength );

	// copy the actual payload
	System.arraycopy( p.getData( ), 0, data, 8, p.getLength( ) );

	// now copy back to the original udp packet and broadcast it.
	p.setData( data );
	super.send( p );
    }

    // The wapper of receive( )
    public void receive( DatagramPacket p ) throws IOException {
	// receive a udp packet
     	super.receive( p );

	// extract only user data
	byte[] orig = p.getData( );
    	int offset =
	    ( orig[0]==alpha && orig[1]==beta && orig[2]==gamma &&
	      orig[3] >= ttl_min && orig[3] <= ttl_max ) ?
	    ( orig[3] + 1 ) * 4 : 0;
	byte[] data = new byte[p.getLength( ) - offset];
	System.arraycopy( orig, offset, data, 0, data.length );

	// now copy back to the original udp packet and set its source address
	p.setData( data );

	if ( offset > 0 ) {
	    byte[] rawIp = new byte[rawIpLength];
	    System.arraycopy( orig, headerLength, rawIp, 0, rawIp.length );
	    InetAddress realSource = null;
	    try {
		realSource = InetAddress.getByAddress( rawIp );
	    } catch ( UnknownHostException e ) { 
	    }
	    if ( realSource != null )
		p.setAddress( realSource );
	}
    }

    public void setData( byte[] buf, int offset, int length ) {
    } 
    public byte[] getData( ) {
	return null;
    }

}
