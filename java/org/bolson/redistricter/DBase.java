/**
 * Just enough implementation of the DBase3 format to read Census shapefile bundles.
 * @author Brian Olson
 */
package org.bolson.redistricter;

import java.io.DataInputStream;
import java.io.EOFException;
import java.io.IOException;
import java.util.ArrayList;

public class DBase {
	byte version;
	int year;
	byte month;
	byte day;
	int numRecords;
	int numHeaderBytes;
	int numRecordBytes;
	byte incomplete;
	byte encrypted;
	byte mdx;
	byte language;
	String driverName = null;
	
	DataInputStream in;
	byte[] scratch = new byte[48];
	public ArrayList<DBaseFieldDescriptor> fields = new ArrayList<DBaseFieldDescriptor>();
	/**
	 * Each record is fixed length.
	 */
	int recordLength;
	
	int readCount = 0;
	
	/**
	 * Immediately reads header data.
	 * @param x stream to read from
	 * @throws IOException 
	 */
	public void setInputStream(DataInputStream x) throws IOException {
		in = x;
		in.readFully(scratch, 0, 32);
		version = scratch[0];
		year = scratch[1] + 1900;
		month = scratch[2];
		day = scratch[3];
		numRecords = ShapefileBundle.bytesToIntLE(scratch, 4);
		numHeaderBytes = ShapefileBundle.bytesToUShortLE(scratch, 8);
		numRecordBytes = ShapefileBundle.bytesToUShortLE(scratch, 10);
		incomplete = scratch[14];
		encrypted = scratch[15];
		mdx = scratch[28];
		language = scratch[29];
		int readPartTwoLen = 31;
		if ((version & 0x07) == 4) {
			in.readFully(scratch, 0, 32);
			driverName = new String(scratch, 0, 32);
			in.skipBytes(4);
			readPartTwoLen = 47;
		} else {
			assert((version & 0x07) == 3);
		}
		scratch[0] = in.readByte();
		int startpos = 0;
		while (scratch[0] != (byte)0x0d) {
			in.readFully(scratch, 1, readPartTwoLen);
			DBaseFieldDescriptor nh = new DBaseFieldDescriptor(scratch, 0, 1+readPartTwoLen);
			nh.startpos = startpos;
			startpos += nh.length;
			fields.add(nh);
			scratch[0] = in.readByte();
		}
		recordLength = startpos;
		scratch = new byte[recordLength];
	}
	byte[] next() throws IOException {
		byte code;
		try {
			code = in.readByte();
			if (code == (byte)0x1a) {
				return null;
			}
		} catch (EOFException e) {
			return null;
		}
		in.readFully(scratch, 0, recordLength);
		readCount++;
		return scratch;
	}
	
	public String toString() {
		StringBuffer sb = new StringBuffer("(DBF ");
		sb.append(Integer.toHexString(version));
		sb.append(" ");
		sb.append(year);
		sb.append("-");
		sb.append(month);
		sb.append("-");
		sb.append(day);
		sb.append(" numRecords=");
		sb.append(numRecords);
		sb.append(" numHeaderBytes=");
		sb.append(numHeaderBytes);
		sb.append(" numRecordBytes=");
		sb.append(numRecordBytes);
		sb.append(" fields:{");
		boolean first = true;
		for (DBaseFieldDescriptor f : fields) {
			if (first) {
				first = false;
			} else {
				sb.append(", ");
			}
			sb.append(f);
		}
		sb.append("})");
		return sb.toString();
	}
	public DBaseFieldDescriptor getField(String name) {
		for (DBaseFieldDescriptor field : fields) {
			if (field.name.equals(name)) {
				return field;
			}
		}
		return null;
	}
}
