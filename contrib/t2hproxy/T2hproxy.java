/*
 *  TFTP to HTTP proxy in Java
 *
 *  Copyright Ken Yap 2003
 *  Released under GPL2
 */
import java.io.IOException;
import java.io.InputStream;
import java.io.FileInputStream;
import java.io.BufferedInputStream;
import java.io.UnsupportedEncodingException;
import java.lang.String;
import java.lang.StringBuffer;
import java.lang.Thread;
import java.lang.NumberFormatException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.SocketException;
import java.net.SocketTimeoutException;
import java.nio.Buffer;
import java.nio.ByteBuffer;
import java.nio.BufferUnderflowException;
import java.util.HashMap;
import java.util.Properties;

import org.apache.commons.httpclient.Credentials;
import org.apache.commons.httpclient.Header;
import org.apache.commons.httpclient.HostConfiguration;
import org.apache.commons.httpclient.HttpClient;
import org.apache.commons.httpclient.HttpException;
import org.apache.commons.httpclient.HttpMethod;
import org.apache.commons.httpclient.UsernamePasswordCredentials;
import org.apache.commons.httpclient.methods.GetMethod;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;

/**
 *  Description of the Class
 *
 *@author     ken
 *@created    24 September 2003
 */
public class T2hproxy implements Runnable {
	/**
	 *  Description of the Field
	 */
	public final static String NAME = T2hproxy.class.getName();
	/**
	 *  Description of the Field
	 */
	public final static String VERSION = "0.1";
	/**
	 *  Description of the Field
	 */
	public final static int MTU = 1500;
	/**
	 *  Description of the Field
	 */
	public final static short TFTP_RRQ = 1;
	/**
	 *  Description of the Field
	 */
	public final static short TFTP_DATA = 3;
	/**
	 *  Description of the Field
	 */
	public final static short TFTP_ACK = 4;
	/**
	 *  Description of the Field
	 */
	public final static short TFTP_ERROR = 5;
	/**
	 *  Description of the Field
	 */
	public final static short TFTP_OACK = 6;
	/**
	 *  Description of the Field
	 */
	public final static short ERR_NOFILE = 1;
	/**
	 *  Description of the Field
	 */
	public final static short ERR_ILLOP = 4;
	/**
	 *  Description of the Field
	 */
	public final static int MAX_RETRIES = 5;
	/**
	 *  TFTP timeout in milliseconds
	 */
	public final static int TFTP_ACK_TIMEOUT = 2000;
	/**
	 *  Description of the Field
	 */
	public final static int DEFAULT_PROXY_PORT = 3128;

	private static Log log = LogFactory.getLog(T2hproxy.class);
	/**
	 *  The members below must be per thread and must not share any storage with
	 *  the main thread
	 */
	private DatagramSocket responsesocket;
	private DatagramPacket response;
	private InetAddress iaddr;
	private int port;
	private byte[] req;
	private String prefix;
	private String proxy = null;
	private int timeout;
	private HashMap options = new HashMap();
	private int blocksize = 512;
	private HttpClient client = new HttpClient();
	private HttpMethod method;
	private BufferedInputStream bstream = null;
	private String message;


	/**
	 *  Constructor for the T2hproxy object
	 *
	 *@param  i   Description of the Parameter
	 *@param  p   Description of the Parameter
	 *@param  b   Description of the Parameter
	 *@param  pf  Description of the Parameter
	 *@param  pr  Description of the Parameter
	 *@param  t   Timeout for HTTP GET
	 */
	public T2hproxy(InetAddress i, int p, byte[] b, String pf, String pr, int t) {
		iaddr = i;
		port = p;
		// make a copy of the request buffer
		req = new byte[b.length];
		System.arraycopy(b, 0, req, 0, b.length);
		prefix = pf;
		// proxy can be null
		proxy = pr;
		timeout = t;
	}


	/**
	 *  Extract an asciz string from bufer
	 *
	 *@param  buffer  Description of the Parameter
	 *@return         The asciz value
	 */
	private String getAsciz(ByteBuffer buffer) {
		StringBuffer s = new StringBuffer();
		try {
			byte b;
			while ((b = buffer.get()) != 0) {
				s.append((char) b);
			}
		} catch (BufferUnderflowException e) {
		} finally {
			return (s.toString());
		}
	}


	/**
	 *  Convert a string of digits to a number, invalid => 0
	 *
	 *@param  s  Description of the Parameter
	 *@return    Description of the Return Value
	 */
	private int atoi(String s) {
		if (s == null) {
			return (0);
		}
		int value = 0;
		try {
			value = (new Integer(s)).intValue();
		} catch (NumberFormatException e) {
		}
		return (value);
	}


	/**
	 *  Wait for ack packet with timeout
	 *
	 *@return    Return block number acked
	 */
	private int waitForAck() {
		DatagramPacket ack = new DatagramPacket(new byte[MTU], MTU);
		try {
			do {
				responsesocket.setSoTimeout(TFTP_ACK_TIMEOUT);
				responsesocket.receive(ack);
			} while (!ack.getAddress().equals(iaddr) || ack.getPort() != port);
		} catch (SocketTimeoutException e) {
			return (-1);
		} catch (Exception e) {
			log.info(e.toString(), e);
		}
		ByteBuffer buffer = ByteBuffer.wrap(ack.getData(), ack.getOffset(), ack.getLength() - ack.getOffset());
		short op;
		if ((op = buffer.getShort()) == TFTP_ACK) {
			return ((int) buffer.getShort());
		} else if (op == TFTP_ERROR) {
			return (-2);
		}
		return (-3);
	}


	/**
	 *  Description of the Method
	 *
	 *@param  error    Description of the Parameter
	 *@param  message  Description of the Parameter
	 */
	private void sendError(short error, String message) {
		ByteBuffer buffer = ByteBuffer.wrap(response.getData());
		buffer.putShort(TFTP_ERROR).putShort(error).put(message.getBytes());
		response.setLength(buffer.position());
		try {
			responsesocket.send(response);
		} catch (Exception e) {
			log.info(e.toString(), e);
		}
	}


	/**
	 *  Description of the Method
	 *
	 *@return    Description of the Return Value
	 */
	private boolean sendOackRecvAck() {
		ByteBuffer buffer = ByteBuffer.wrap(response.getData());
		buffer.putShort(TFTP_OACK).put("blksize".getBytes()).put((byte) 0).put(String.valueOf(blocksize).getBytes()).put((byte) 0);
		response.setLength(buffer.position());
		int retry;
		for (retry = 0; retry < MAX_RETRIES; retry++) {
			try {
				responsesocket.send(response);
			} catch (Exception e) {
				log.info(e.toString(), e);
			}
			if (waitForAck() == 0) {
				log.debug("Ack received");
				break;
			}
		}
		return (retry < MAX_RETRIES);
	}


	/**
	 *  Description of the Method
	 *
	 *@param  block      Description of the Parameter
	 *@return            Description of the Return Value
	 */
	private boolean sendDataBlock(int block) {
		int retry;
		for (retry = 0; retry < MAX_RETRIES; retry++) {
			try {
				responsesocket.send(response);
			} catch (Exception e) {
				log.info(e.toString(), e);
			}
			int ablock;
			if ((ablock = waitForAck()) == block) {
				log.debug("Ack received for " + ablock);
				break;
			} else if (ablock == -1) {
				log.info("Timeout waiting for ack");
			} else if (ablock == -2) {
				return (false);
			} else {
				log.info("Unknown opcode from ack");
			}
		}
		return (retry < MAX_RETRIES);
	}


	/**
	 *  Description of the Method
	 *
	 *@param  buffer  Description of the Parameter
	 *@return         Description of the Return Value
	 */
	private boolean handleOptions(ByteBuffer buffer) {
		for (; ; ) {
			String option = getAsciz(buffer);
			String value = getAsciz(buffer);
			if (option.equals("") || value.equals("")) {
				break;
			}
			log.info(option + " " + value);
			options.put(option, value);
		}
		blocksize = atoi((String) options.get("blksize"));
		if (blocksize < 512) {
			blocksize = 512;
		}
		if (blocksize > 1432) {
			blocksize = 1432;
		}
		return (sendOackRecvAck());
	}


	/**
	 *  Description of the Method
	 *
	 *@param  url  Description of the Parameter
	 */
	private void makeStream(String url) {
		// establish a connection within timeout milliseconds
		client.setConnectionTimeout(timeout);
		if (proxy != null) {
			String[] hostport = proxy.split(":");
			int port = DEFAULT_PROXY_PORT;
			if (hostport.length > 1) {
				port = atoi(hostport[1]);
				if (port == 0) {
					port = DEFAULT_PROXY_PORT;
				}
			}
			log.info("Proxy is " + hostport[0] + ":" + port);
			client.getHostConfiguration().setProxy(hostport[0], port);
		}
		// create a method object
		method = new GetMethod(url);
		method.setFollowRedirects(true);
		method.setStrictMode(false);
		try {
			int status;
			if ((status = client.executeMethod(method)) != 200) {
				log.info(message = method.getStatusText());
				return;
			}
			bstream = new BufferedInputStream(method.getResponseBodyAsStream());
		} catch (HttpException he) {
			message = he.getMessage();
		} catch (IOException ioe) {
			message = "Unable to get " + url;
		}
	}


	/**
	 *  Reads a block of data from URL stream
	 *
	 *@param  stream     Description of the Parameter
	 *@param  data       Description of the Parameter
	 *@param  blocksize  Description of the Parameter
	 *@param  offset     Description of the Parameter
	 *@return            Number of bytes read
	 */
	private int readBlock(BufferedInputStream stream, byte[] data, int offset, int blocksize) {
		int status;
		int nread = 0;
		while (nread < blocksize) {
			try {
				status = stream.read(data, offset + nread, blocksize - nread);
			} catch (Exception e) {
				return (-1);
			}
			if (status < 0) {
				return (nread);
			}
			nread += status;
		}
		return (nread);
	}


	/**
	 *  Description of the Method
	 *
	 *@param  filename  Description of the Parameter
	 */
	private void doRrq(String filename) {
		String url = prefix + filename;
		log.info("GET " + url);
		makeStream(url);
		if (bstream == null) {
			log.info(message);
			sendError(ERR_NOFILE, message);
			return;
		}
		// read directly into send buffer to avoid buffer copying
		byte[] data;
		ByteBuffer buffer = ByteBuffer.wrap(data = response.getData());
		// dummy puts to get start position of data
		buffer.putShort(TFTP_DATA).putShort((short) 0);
		int start = buffer.position();
		int length;
		int block = 1;
		do {
			length = readBlock(bstream, data, start, blocksize);
			block &= 0xffff;
			log.debug("Block " + block + " " + length);
			// fill in the block number
			buffer.position(0);
			buffer.putShort(TFTP_DATA).putShort((short) block);
			response.setLength(start + length);
			if (!sendDataBlock(block)) {
				break;
			}
			buffer.position(start);
			block++;
		} while (length >= blocksize);
		log.info("Closing TFTP session");
		// clean up the connection resources
		method.releaseConnection();
		method.recycle();
	}


	/**
	 *  Main processing method for the T2hproxy object
	 */
	public void run() {
		ByteBuffer buffer = ByteBuffer.wrap(req);
		buffer.getShort();
		String filename = getAsciz(buffer);
		String mode = getAsciz(buffer);
		log.info(filename + " " + mode);
		response = new DatagramPacket(new byte[MTU], MTU, iaddr, port);
		try {
			responsesocket = new DatagramSocket();
		} catch (SocketException e) {
			log.info(e.toString(), e);
			return;
		}
		if (!handleOptions(buffer)) {
			return;
		}
		doRrq(filename);
	}


	/**
	 *  Description of the Method
	 *
	 *@param  s        Description of the Parameter
	 *@param  r        Description of the Parameter
	 *@param  prefix   Description of the Parameter
	 *@param  proxy    Description of the Parameter
	 *@param  timeout  Description of the Parameter
	 */
	public static void handleRequest(DatagramSocket s, DatagramPacket r, String prefix, String proxy, int timeout) {
		log.info("Connection from " + r.getAddress().getCanonicalHostName() + ":" + r.getPort());
		ByteBuffer buffer = ByteBuffer.wrap(r.getData(), r.getOffset(), r.getLength() - r.getOffset());
		if (buffer.getShort() != TFTP_RRQ) {
			DatagramPacket error = new DatagramPacket(new byte[MTU], MTU);
			ByteBuffer rbuf = ByteBuffer.wrap(error.getData());
			rbuf.putShort(TFTP_ERROR).putShort(ERR_ILLOP).put("Illegal operation".getBytes());
			error.setLength(rbuf.position());
			try {
				s.send(error);
			} catch (Exception e) {
				log.info(e.toString(), e);
			}
			return;
		}
		// fork thread
		new Thread(new T2hproxy(r.getAddress(), r.getPort(), r.getData(), prefix, proxy, timeout)).start();
	}


	/**
	 *  The main program for the T2hproxy class
	 *
	 *@param  argv             The command line arguments
	 *@exception  IOException  Description of the Exception
	 */
	public static void main(String[] argv) throws IOException {
		log.info(T2hproxy.NAME + "." + T2hproxy.VERSION);
		int port = Integer.getInteger(T2hproxy.NAME + ".port", 69).intValue();
		String prefix = System.getProperty(T2hproxy.NAME + ".prefix", "http://localhost/");
		String proxy = System.getProperty(T2hproxy.NAME + ".proxy");
		int timeout = Integer.getInteger(T2hproxy.NAME + ".timeout", 5000).intValue();
		String propfile = System.getProperty(T2hproxy.NAME + ".properties");
		if (propfile != null) {
			FileInputStream pf = new FileInputStream(propfile);
			Properties p = new Properties(System.getProperties());
			p.load(pf);
			// set the system properties
			System.setProperties(p);
		}
		DatagramSocket requestsocket;
		try {
			requestsocket = new DatagramSocket(port);
		} catch (SocketException e) {
			log.info(e.toString(), e);
			return;
		}
		DatagramPacket request = new DatagramPacket(new byte[MTU], MTU);
		for (; ; ) {
			try {
				requestsocket.receive(request);
				handleRequest(requestsocket, request, prefix, proxy, timeout);
			} catch (Exception e) {
				log.info(e.toString(), e);
			}
		}
	}
}
