package org.bolson.redistricter;

import java.io.DataInputStream;
import java.io.EOFException;
import java.io.FileOutputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.Writer;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

// Things I need from the shapefile bundle:
// Block adjacency
// Rasterization
public class ShapefileBundle {
	
	static int swap(int x) {
		return
		((x & 0x000000ff) << 24) |
		((x & 0x0000ff00) << 8) |
		((x & 0x00ff0000) >> 8) |
		((x >> 24) & 0x000000ff);
	}
	static long swap(long x) {
		return
		((x & 0x00000000000000ffL) << 56) |
		((x & 0x000000000000ff00L) << 40) |
		((x & 0x0000000000ff0000L) << 24) |
		((x & 0x00000000ff000000L) << 8) |
		((x & 0x000000ff00000000L) >> 8) |
		((x & 0x0000ff0000000000L) >> 24) |
		((x & 0x00ff000000000000L) >> 40) |
		((x >> 56) & 0x00000000000000ff);
	}
	static int bytesToInt(byte[] data, int pos) {
		return ((data[pos+3] & 0x000000ff)) |
		((data[pos+2] << 8) & 0x0000ff00) |
		((data[pos+1] << 16) & 0x00ff0000) |
		(data[pos] << 24);
	}
	static int bytesToUShortLE(byte[] data, int pos) {
		return 
		((((int)data[pos+1]) & 0x000000FF) << 8) |
		(((int)data[pos]) & 0x000000FF);
	}
	static int bytesToIntLE(byte[] data, int pos) {
		return ((data[pos] & 0x000000ff)) |
		((data[pos+1] << 8) & 0x0000ff00) |
		((data[pos+2] << 16) & 0x00ff0000) |
		(data[pos+3] << 24);
	}
	static long bytesToLongLE(byte[] data, int pos) {
		return 
		((((long)data[pos+7]) & 0x00000000000000FFL) << 56) |
		((((long)data[pos+6]) & 0x00000000000000FFL) << 48) |
		((((long)data[pos+5]) & 0x00000000000000FFL) << 40) |
		((((long)data[pos+4]) & 0x00000000000000FFL) << 32) |
		((((long)data[pos+3]) & 0x00000000000000FFL) << 24) |
		((((long)data[pos+2]) & 0x00000000000000FFL) << 16) |
		((((long)data[pos+1]) & 0x00000000000000FFL) << 8) |
		(((long)data[pos]) & 0x00000000000000FFL);
	}
	static String bytesToHexString(byte[] data, int pos, int len) {
		StringBuffer out = new StringBuffer();
		for (int i = 0; i < len; ++i) {
			String hexstr = Integer.toString(data[pos + i] & 0x000000ff, 16);
			if (hexstr.length() == 1) {
				out.append('0');
			}
			out.append(hexstr);
		}
		return out.toString();
	}
	static double bytesToDoubleLE(byte[] data, int pos) {
		long l = bytesToLongLE(data, pos);
		return Double.longBitsToDouble(l);
	}
	
	static class ShapefileHeader {
		/*
		 * 	'fileCode': ShapefileHeaderField(0,4,'>i'),
	'fileLength': ShapefileHeaderField(24,28,'>i'),
	'version': ShapefileHeaderField(28,32,'<i'),
	'shapeType': ShapefileHeaderField(32,36,'<i'),
	'xmin': ShapefileHeaderField(36,44,'<d'),
	'ymin': ShapefileHeaderField(44,52,'<d'),
	'xmax': ShapefileHeaderField(52,60,'<d'),
	'ymax': ShapefileHeaderField(60,68,'<d'),
	'zmin': ShapefileHeaderField(68,76,'<d'),
	'zmax': ShapefileHeaderField(76,84,'<d'),
	'mmin': ShapefileHeaderField(84,92,'<d'),
	'mmax': ShapefileHeaderField(92,100,'<d'),

		 */
		int fileCode;
		int fileLength;
		int version;
		int shapeType;
		double xmin, ymin, xmax, ymax, zmin, zmax, mmin, mmax;
		public void read(DataInputStream in) throws IOException {
			byte[] data = new byte[100];
			in.readFully(data);
			fileCode = bytesToInt(data, 0);
			fileLength = bytesToInt(data, 24);
			version = bytesToIntLE(data, 28);
			shapeType = bytesToIntLE(data, 32);
			xmin = bytesToDoubleLE(data, 36);
			ymin = bytesToDoubleLE(data, 44);
			xmax = bytesToDoubleLE(data, 52);
			ymax = bytesToDoubleLE(data, 60);
			zmin = bytesToDoubleLE(data, 68);
			zmax = bytesToDoubleLE(data, 76);
			mmin = bytesToDoubleLE(data, 84);
			mmax = bytesToDoubleLE(data, 92);
		}
		
		public String toString() {
			return "(Shapefile code=" + fileCode + " length=" + fileLength + " version=" + version +
			" shape=" + shapeType +
			" x=[" + xmin + "," + xmax + "] y=[" + ymin + "," + ymax +
			"] z=[" + zmin + "," + zmax + "] m=[" + mmin + "," + mmax + "])";
		}
	}
	static class Shapefile {
		DataInputStream in;
		public ShapefileHeader header = null;
		byte[] recordHeader = new byte[8];
		byte[] recordBuffer = null;
		public int recordCount = 0;
		
		public void setInputStream(InputStream in) {
			if (in instanceof DataInputStream) {
				this.in = (DataInputStream)in;
			} else {
				this.in = new DataInputStream(in);
			}
		}
		public Polygon next() throws IOException {
			if (header == null) {
				header = new ShapefileHeader();
				header.read(in);
				System.out.println(header);
			}
			int recordContentLength;
			try {
				in.skipBytes(4);//int recordNumber = in.readInt();
				recordContentLength = in.readInt() * 2;
			} catch (EOFException e) {
				return null;
			}
			if (recordBuffer == null || recordBuffer.length < recordContentLength) {
				recordBuffer = new byte[recordContentLength];
			}
			in.readFully(recordBuffer, 0, recordContentLength);
			int type = bytesToIntLE(recordBuffer, 0);
			assert(type == 5);
			recordCount++;
			return new Polygon(recordBuffer, 0, recordContentLength);
		}
	}
	
	static class PolygonBucket {
		public double minx, maxx, miny, maxy;
		public ArrayList<Polygon> polys = new ArrayList<Polygon>();
		PolygonBucket(double minx, double maxx, double miny, double maxy) {
			this.minx = minx;
			this.maxx = maxx;
			this.miny = miny;
			this.maxy = maxy;
		}
		public boolean contains(double x, double y) {
			return ((x >= minx) && (x <= maxx) && (y >= miny) && (y <= maxy));
		}
		public boolean add(Polygon p) {
			polys.add(p);
			/*
			if (contains(p.xmin, p.ymin) || contains(p.xmin, p.ymax) || contains(p.xmax,p.ymin) || contains(p.xmax, p.ymax)) {
				polys.add(p);
				return true;
			}
			return false;
			*/
			return true;
		}
		public int writeLinks(Writer out, Polygon b) throws IOException {
			int count = 0;
			for (Polygon a : polys) {
				if (a == b) {
					continue;
				}
				if (a.hasTwoPointsInCommon(b)) {
					ShapefileBundle.writeLink(out, a, b);
					count++;
				}
			}
			return count;
		}
		public int writeLinks(OutputStream out, Polygon b) throws IOException {
			int count = 0;
			for (Polygon a : polys) {
				if (a == b) {
					continue;
				}
				if (a.hasTwoPointsInCommon(b)) {
					ShapefileBundle.writeLink(out, a, b);
					count++;
				}
			}
			return count;
		}
		public int mapLinks(Map<byte[], Set<byte[]> > out, Polygon b) {
			int count = 0;
			for (Polygon a : polys) {
				if (a == b) {
					continue;
				}
				if (a.hasTwoPointsInCommon(b)) {
					int c = cmp(a.blockid, b.blockid);
					Polygon lower, upper;
					if (c < 0) {
						lower = a;
						upper = b;
					} else if (c > 0) {
						lower = b;
						upper = a;
					} else {
						assert(false);
						continue;
					}
					Set<byte[]> rhset;
					if (!out.containsKey(lower.blockid)) {
						rhset = new HashSet<byte[]>();
						out.put(lower.blockid, rhset);
					} else {
						rhset = out.get(lower.blockid);
					}
					if (rhset.add(upper.blockid)) {
						count++;
					}
				}
			}
			return count;
		}
	}
	static class PolygonBucketArray {
		int width;
		int height;
		double minx, miny, maxx, maxy;
		double dx, dy;
		PolygonBucket[] they;
		PolygonBucketArray(double minx, double miny, double maxx, double maxy, int width, int height) {
			this.minx = minx;
			this.miny = miny;
			this.maxx = maxx;
			this.maxy = maxy;
			this.width = width;
			this.height = height;
			assert(maxy > miny);
			assert(maxx > minx);
			init();
		}
		
		public PolygonBucketArray(Shapefile shp, int width, int height) {
			this.minx = shp.header.xmin;
			this.miny = shp.header.ymin;
			this.maxx = shp.header.xmax;
			this.maxy = shp.header.ymax;
			this.width = width;
			this.height = height;
			init();
		}
		
		protected void init() {
			they = new PolygonBucket[width * height];
			dx = (maxx - minx) / width;
			dy = (maxy - miny) / height;
			for (int x = 0; x < width; x++) {
				double subminx = minx + dx * x;
				double submaxx = maxx - dx * (width - (x + 1));
				for (int y = 0; y < height; y++) {
					double subminy = miny + dy * y;
					double submaxy = maxy - dy * (height - (y + 1));
					they[(x*height) + y] = new PolygonBucket(subminx, submaxx, subminy, submaxy);
				}
			}
		}
		
		int bucketX(double x) {
			assert(x >= minx);
			assert(x <= maxx);
			int ix;
			if (x == maxx) {
				ix = width - 1;
			} else {
				ix = (int)Math.floor((x - minx) / dx);
			}
			assert(ix >= 0);
			assert(ix < width);
			return ix;
		}
		
		int bucketY(double y) {
			assert(y >= miny);
			assert(y <= maxy);
			int iy;
			if (y == maxy) {
				iy = height - 1;
			} else {
				iy = (int)Math.floor((y - miny) / dy);
			}
			assert(iy >= 0);
			assert(iy < height);
			return iy;
		}

		PolygonBucket getBucket(double x, double y) {
			assert(x >= minx);
			assert(x <= maxx);
			assert(y >= miny);
			assert(y <= maxy);
			int ix;
			if (x == maxx) {
				ix = width - 1;
			} else {
				ix = (int)Math.floor((x - minx) / dx);
			}
			int iy;
			if (y == maxy) {
				iy = height - 1;
			} else {
				iy = (int)Math.floor((y - miny) / dy);
			}
			assert(ix >= 0);
			assert(ix < width);
			assert(iy >= 0);
			assert(iy < height);
			return they[(ix * height) + iy];
		}
		
		int writeLinks(Writer out, Polygon p) throws IOException {
			int count = 0;
			int minix = bucketX(p.xmin);
			int maxix = bucketX(p.xmax);
			int miniy = bucketY(p.ymin);
			int maxiy = bucketY(p.ymax);
			for (int ix = minix; ix <= maxix; ++ix) {
				for (int iy = miniy; iy <= maxiy; ++iy) {
					count += they[(ix * height) + iy].writeLinks(out, p);
				}
			}
			return count;
		}
		
		int writeLinks(OutputStream out, Polygon p) throws IOException {
			int count = 0;
			int minix = bucketX(p.xmin);
			int maxix = bucketX(p.xmax);
			int miniy = bucketY(p.ymin);
			int maxiy = bucketY(p.ymax);
			for (int ix = minix; ix <= maxix; ++ix) {
				for (int iy = miniy; iy <= maxiy; ++iy) {
					count += they[(ix * height) + iy].writeLinks(out, p);
				}
			}
			return count;
		}
		
		public int mapLinks(Map<byte[], Set<byte[]> > out, Polygon p) {
			int minix = bucketX(p.xmin);
			int maxix = bucketX(p.xmax);
			int miniy = bucketY(p.ymin);
			int maxiy = bucketY(p.ymax);
			int count = 0;
			for (int ix = minix; ix <= maxix; ++ix) {
				for (int iy = miniy; iy <= maxiy; ++iy) {
					count += they[(ix * height) + iy].mapLinks(out, p);
				}
			}
			return count;
		}
		
		void add(Polygon p) {
			int minix = bucketX(p.xmin);
			int maxix = bucketX(p.xmax);
			int miniy = bucketY(p.ymin);
			int maxiy = bucketY(p.ymax);
			for (int ix = minix; ix <= maxix; ++ix) {
				for (int iy = miniy; iy <= maxiy; ++iy) {
					they[(ix * height) + iy].add(p);
				}
			}
		}
	}

	static class Polygon {
		public double xmin, xmax, ymin, ymax;
		public int[] parts;
		public double[] points;
		//public String blockid;
		public byte[] blockid;
		
		public Polygon(byte[] data, int offset, int length) {
			init(data, offset, length);
		}
		public void init(byte[] data, int offset, int length) {
			int pos = 0;
			int type = bytesToIntLE(data, pos); pos += 4;
			assert(type == 5);
			xmin = bytesToDoubleLE(data, pos); pos += 8;
			ymin = bytesToDoubleLE(data, pos); pos += 8;
			xmax = bytesToDoubleLE(data, pos); pos += 8;
			ymax = bytesToDoubleLE(data, pos); pos += 8;
			int numParts = bytesToIntLE(data, pos); pos += 4;
			int numPoints = bytesToIntLE(data, pos); pos += 4;
			parts = new int[numParts];
			points = new double[numPoints*2];
			for (int i = 0; i < numParts; ++i) {
				parts[i] = bytesToIntLE(data, pos);
				pos += 4;
			}
			for (int i = 0; i < numPoints * 2; ++i) {
				points[i] = bytesToDoubleLE(data, pos);
				pos += 8;
			}
			assert(pos == length);
		}
		
		public boolean hasTwoPointsInCommon(Polygon b) {
			boolean haveOne = false;
			for (int i = 0; i < points.length; i += 2) {
				for (int j = 0; j < b.points.length; j += 2) {
					if ((points[i] == b.points[j]) && (points[i+1] == b.points[j+1])) {
						if (haveOne) {
							return true;
						}
						haveOne = true;
					}
				}
			}
			return false;
		}
		
		public int[] rasterize(double xZero, double yZero, double xPerPixel, double yPerPixel) {
			// TODO: rasterize
			return null;
		}
		
		public String toString() {
			StringBuffer out = new StringBuffer("(Polygon ");
			out.append(xmin);
			out.append("<=x<=");
			out.append(xmax);
			out.append(' ');
			out.append(ymin);
			out.append("<=y<=");
			out.append(ymax);
			int part = 0;
			int pos = parts[part];
			part++;
			while (part < parts.length) {
				out.append(" (");
				out.append(points[pos*2]);
				out.append(',');
				out.append(points[pos*2 + 1]);
				pos++;
				while (pos != parts[part]) {
					out.append(' ');
					out.append(points[pos*2]);
					out.append(',');
					out.append(points[pos*2 + 1]);
					pos++;
				}
				out.append(')');
				part++;
			}
			// last/only part
			out.append(" (");
			out.append(points[pos*2]);
			out.append(',');
			out.append(points[pos*2 + 1]);
			pos++;
			while (pos*2 < points.length) {
				out.append(' ');
				out.append(points[pos*2]);
				out.append(',');
				out.append(points[pos*2 + 1]);
				pos++;
			}
			out.append("))");
			return out.toString();
		}
	}
	public static class DBaseFieldDescriptor {

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
			// TODO: optimize away garbage String created here? parse ascii to int directly?
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
	public static class DBase {
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
			numRecords = bytesToIntLE(scratch, 4);
			numHeaderBytes = bytesToUShortLE(scratch, 8);
			numRecordBytes = bytesToUShortLE(scratch, 10);
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
			System.out.println(this);
			scratch[0] = in.readByte();
			int startpos = 0;
			while (scratch[0] != (byte)0x0d) {
				in.readFully(scratch, 1, readPartTwoLen);
				DBaseFieldDescriptor nh = new DBaseFieldDescriptor(scratch, 0, 1+readPartTwoLen);
				nh.startpos = startpos;
				startpos += nh.length;
				System.out.println(nh);
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
			return "(DBF " + Integer.toHexString(version) + " " + year + "-" + month + "-" + day + " numRecords=" + numRecords +
			" numHeaderBytes=" + numHeaderBytes + " numRecordBytes=" + numRecordBytes + ")";
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
	Shapefile shp = null;
	DBase dbf = null;
	ArrayList<Polygon> polys = new ArrayList<Polygon>();
	PolygonBucketArray pba = null;
	
	public void read(String filename) throws IOException {
		int lastSlash = filename.lastIndexOf('/');
		String nameroot = filename.substring(lastSlash+1, filename.length() - 4);
		
		ZipFile f = new ZipFile(filename);
		ZipEntry shpEntry = f.getEntry(nameroot + ".shp");
		assert(shpEntry != null);
		InputStream shpIs = f.getInputStream(shpEntry);
		InputStream dbfIs = f.getInputStream(f.getEntry(nameroot + ".dbf"));
		
		shp = new Shapefile();
		shp.setInputStream(shpIs);
		
		dbf = new DBase();
		dbf.setInputStream(new DataInputStream(dbfIs));
		
		
		Polygon p = shp.next();
		byte[] rowbytes;// = dbf.next();
		DBaseFieldDescriptor blockIdField = dbf.getField("BLKIDFP00");
		assert(blockIdField != null);
		
		pba = new PolygonBucketArray(shp, 20, 20);

		while (p != null) {
			rowbytes = dbf.next();
			assert(rowbytes != null);
			//p.blockid = blockIdField.getString(rowbytes, 0, rowbytes.length);
			p.blockid = blockIdField.getBytes(rowbytes, 0, rowbytes.length);
			polys.add(p);
			pba.add(p);
			//System.out.println(p);
			p = shp.next();
		}
		f.close();
	}
	public int records() {
		assert(shp.recordCount == dbf.readCount);
		return shp.recordCount;
	}
	public int printLinks(Writer out) throws IOException {
		int count = 0;
		for (Polygon p : polys) {
			count += pba.writeLinks(out, p);
		}
		return count;
	}
	public int printLinks(OutputStream out) throws IOException {
		int count = 0;
		for (Polygon p : polys) {
			count += pba.writeLinks(out, p);
		}
		return count;
	}
	public int mapLinks(Map<byte[], Set<byte[]> > out) {
		int count = 0;
		for (Polygon p : polys) {
			count += pba.mapLinks(out, p);
		}
		return count;
	}
	public static void writeLink(Writer out, Polygon a, Polygon b) throws IOException {
		int c = cmp(a.blockid, b.blockid);
		if (c < 0) {
			out.write(new String(a.blockid));
			out.write(',');
			out.write(new String(b.blockid));
		} else if (c > 0) {
			out.write(new String(b.blockid));
			out.write(',');
			out.write(new String(a.blockid));
		} else {
			return;
		}
		out.write('\n');
	}
	public static void writeLink(OutputStream out, Polygon a, Polygon b) throws IOException {
		int c = cmp(a.blockid, b.blockid);
		if (c < 0) {
			out.write(a.blockid);
			out.write(',');
			out.write(b.blockid);
		} else if (c > 0) {
			out.write(b.blockid);
			out.write(',');
			out.write(a.blockid);
		} else {
			return;
		}
		out.write('\n');
	}
	public static int cmp(byte[] a, byte[] b) {
		for (int i = 0; (i < a.length) && (i < b.length); ++i) {
			if (a[i] < b[i]) {
				return -1;
			}
			if (a[i] > b[i]) {
				return 1;
			}
		}
		return 0;
	}
	
	public static void main(String[] argv) throws IOException {
		boolean tree = false;
		for (int i = 0; i < argv.length; ++i) {
			if (argv[i].endsWith(".zip")) {
				ShapefileBundle x = new ShapefileBundle();
				long start = System.currentTimeMillis();
				x.read(argv[i]);
				long end = System.currentTimeMillis();
				System.out.println("read " + x.records() + " in " + ((end - start) / 1000.0) + " seconds");
				//int linkCount = x.printLinks(new FileWriter("/tmp/foo.links"));
				//int linkCount = x.printLinks(new FileOutputStream("/tmp/foo.links"));
				Map<byte[], Set<byte[]> > links;
				if (tree) {
					links = new TreeMap<byte[], Set<byte[]> >();
				} else {
					links = new HashMap<byte[], Set<byte[]> >();
				}
				int linksMapped = x.mapLinks(links);
				long linksCalculated = System.currentTimeMillis();
				System.out.println("calculated " + linksMapped + " links in " + ((linksCalculated - end) / 1000.0) + " seconds, links.size()=" + links.size());
				OutputStream out = new FileOutputStream("/tmp/foo.links");
				int linkCount = 0;
				for (byte[] key : links.keySet()) {
					for (byte[] value : links.get(key)) {
						linkCount++;
						out.write(key);
						out.write(',');
						out.write(value);
						out.write('\n');
					}
				}
				out.close();
				long linksWritten = System.currentTimeMillis();
				System.out.println("wrote " + linkCount + " links in " + ((linksWritten - linksCalculated) / 1000.0) + " seconds");
			}
		}
	}
}
