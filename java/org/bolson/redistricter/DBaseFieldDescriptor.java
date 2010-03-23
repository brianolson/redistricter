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
	
	public int getInt(byte[] data, int offset, int length) {
		assert(length >= (this.startpos + this.length));
		assert(type == NUMERIC);
		return Integer.parseInt(new String(data, offset + this.startpos, this.length));
	}
	
	public String getString(byte[] data, int offset, int length) {
		assert(length >= (this.startpos + this.length));
		assert(type == CHARACTER);
		return new String(data, offset + this.startpos, this.length);
	}
	
	public byte[] getBytes(byte[] data, int offset, int length) {
		assert(length >= (this.startpos + this.length));
		assert(type == CHARACTER);
		byte[] out = new byte[this.length];
		System.arraycopy(data, offset + this.startpos, out, 0, this.length);
		return out;
	}

	public String toString() {
		return "(" + name + " type=" + ((char)type) + " length=" + length + " count=" + count + ")";
	}
}