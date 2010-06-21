/**
 * 
 */
package org.bolson.redistricter;

public class DBaseFieldDescriptor {

	public String name;
	public byte type;
	public byte length;
	public byte count;
	
	/**
	 * Byte offset within a fixed-length record.
	 */
	public int startpos;
	
	public static final byte NUMERIC = (byte)'N';
	public static final byte CHARACTER = (byte)'C';
	
	protected DBaseFieldDescriptor() {
		// nop
	}
	protected DBaseFieldDescriptor(String name, byte type, byte length, byte count) {
		this.name = name;
		this.type = type;
		this.length = length;
		this.count = count;
	}
	
	public DBaseFieldDescriptor(byte[] data, int offset, int length) {
		parseHeader(data, offset, length);
	}
	
	/*
	 * 		if len(rawbytes) == 48:
		(self.name, self.ftype, self.length, self.count,
		 unused_1,
		 self.mdx,
		 unused_2,
		 self.nextAutoincrementValue,
		 unused_3
		 ) = struct.unpack('<32scBBHBHII', rawbytes)
	elif len(rawbytes) == 32:
		(self.name, # 11s
		 self.ftype, # c
		 unused_1, # I
		 self.length, self.count, # BB
		 unused_2
		 ) = struct.unpack('<11scIBB14s', rawbytes)
	self.name = self.name.strip(' \t\r\n\0')

	 */
	public void parseHeader(byte[] data, int offset, int length) {
		if (length == 32) {
			this.name = new String(data, offset, 11);
			this.type = data[offset+11];
			this.length = data[offset + 16];
			this.count = data[offset + 17];
		} else if (length == 48) {
			this.name = new String(data, offset, 32);
			this.type = data[offset+32];
			this.length = data[offset + 33];
			this.count = data[offset + 34];
		} else {
			assert(false);
		}
		this.name = this.name.trim();
	}
	
	public static long byteArrayToLong(byte[] ar) throws NumberFormatException {
		int pos = 0;
		while (Character.isWhitespace(ar[pos])) {
			pos++;
		}
		if ((ar[pos] < (byte)'0') || (ar[pos] > (byte)'9')) {
			throw new NumberFormatException("bad char in 1s place=" + (char)ar[pos]);
		}
		long out = 0;
		int end = ar.length;
		while ((pos < end) && (ar[pos] >= (byte)'0') && (ar[pos] <= (byte)'9')) {
			out *= 10;
			out += ar[pos] - (byte)'0';
			pos++;
		}
		return out;
	}
	public static long byteArrayToLong(byte[] ar, int offset, int length) throws NumberFormatException {
		int pos = offset;
		int end = offset + length;
		while (Character.isWhitespace(ar[pos]) && (pos < end)) {
			pos++;
		}
		if ((ar[pos] < (byte)'0') || (ar[pos] > (byte)'9')) {
			throw new NumberFormatException("bad char in 1s place=" + (char)ar[pos]);
		}
		long out = 0;
		while ((pos < end) && (ar[pos] >= (byte)'0') && (ar[pos] <= (byte)'9')) {
			out *= 10;
			out += ar[pos] - (byte)'0';
			pos++;
		}
		return out;
	}
	
	public static int byteArrayToInt(byte[] ar) throws NumberFormatException {
		int pos = ar.length - 1;
		if ((ar[pos] < (byte)'0') || (ar[pos] > (byte)'9')) {
			throw new NumberFormatException("bad char in 1s place=" + (char)ar[pos]);
		}
		int out = 0;
		while ((pos >= 0) && (ar[pos] >= (byte)'0') && (ar[pos] <= (byte)'9')) {
			out *= 10;
			out += ar[pos] - (byte)'0';
			pos--;
		}
		return out;
	}
	public static int byteArrayToInt(byte[] ar, int offset, int length) throws NumberFormatException {
		int pos = offset + length - 1;
		if ((ar[pos] < (byte)'0') || (ar[pos] > (byte)'9')) {
			throw new NumberFormatException("bad char in 1s place=" + (char)ar[pos]);
		}
		int out = 0;
		while ((pos >= offset) && (ar[pos] >= (byte)'0') && (ar[pos] <= (byte)'9')) {
			out *= 10;
			out += ar[pos] - (byte)'0';
			pos--;
		}
		return out;
	}
	
	public int getInt(byte[] data, int offset, int length) {
		assert(length >= (this.startpos + this.length));
		assert(type == NUMERIC);
		return byteArrayToInt(data, offset + this.startpos, this.length);
	}
	public long getLong(byte[] data, int offset, int length) {
		assert(length >= (this.startpos + this.length));
		assert(type == NUMERIC);
		return byteArrayToLong(data, offset + this.startpos, this.length);
	}
	
	public String getString(byte[] data, int offset, int length) {
		assert(length >= (this.startpos + this.length));
		assert(type == CHARACTER);
		return new String(data, offset + this.startpos, this.length);
	}
	
	public byte[] getBytes(byte[] data, int offset, int length) {
		byte[] out = new byte[this.length];
		getBytes(data, offset, length, out, 0);
		return out;
	}
	/**
	 * 
	 * @param data
	 * @param offset
	 * @param length
	 * @param out
	 * @param outOffset
	 * @return new offset into out where next byte might go
	 */
	public int getBytes(byte[] data, int offset, int length, byte[] out, int outOffset) {
		assert(length >= (this.startpos + this.length));
		assert(type == CHARACTER);
		System.arraycopy(data, offset + this.startpos, out, outOffset, this.length);
		return this.length;
	}

	public String toString() {
		return "(" + name + " type=" + ((char)type) + " length=" + length + " count=" + count + ")";
	}
}