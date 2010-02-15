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
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.zip.GZIPOutputStream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

import com.google.protobuf.ByteString;

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
	
	static class RasterizationContext {
		public double minx;
		public double miny;
		public double maxx;
		public double maxy;
		public double pixelHeight;
		public double pixelWidth;
		public int xpx;
		public int ypx;
		public double[] xIntersectScratch = new double[100];
		public int xIntersects = 0;
		public int[] pixels = new int[200];
		public int pxPos = 0;
		
		public RasterizationContext(ShapefileBundle shp, int px,
				int py) {
			xpx = px;
			ypx = py;
			minx = shp.shp.header.xmin;
			miny = shp.shp.header.ymin;
			maxx = shp.shp.header.xmax;
			maxy = shp.shp.header.ymax;
			pixelHeight = (maxy - miny) / ypx;
			pixelWidth = (maxx - minx) / xpx;
		}
		public void growPixels() {
			int[] npx = new int[pixels.length * 2];
			System.arraycopy(pixels, 0, npx, 0, pixels.length);
			pixels = npx;
		}
		public void addPixel(int x, int y) {
			if (pxPos == pixels.length) {
				growPixels();
			}
			pixels[pxPos] = x;
			pixels[pxPos+1] = y;
			pxPos += 2;
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
		
		/*
		 _____________ maxlat
		 |   |   |   |
		 -------------
		 |   |   |   |
		 ------------- minlat
	  minlon       maxlon
	 
	 every pixel should be in exactly one triangle, based on the center of the pixel.
	 The outer edges of the pixel image will be at the min/max points.
	 */
		
		/* for some y, what is the next pixel center below that? */
		static final int pcenterBelow(double somey, double maxy, double pixelHeight) {
			return (int)Math.floor( ((maxy - somey) / pixelHeight) + 0.5 );
		}
		/* for some x, what is the next pixel center to the right? */
		static final int pcenterRight(double somex, double minx, double pixelWidth) {
			return (int)Math.floor( ((somex - minx) / pixelWidth) + 0.5 );
		}
		static final double pcenterY( int py, double maxy, double pixelHeight ) {
			return maxy - ((py + 0.5) * pixelHeight);
		}
		static final double pcenterX( int px, double minx, double pixelWidth ) {
			return minx + ((px + 0.5) * pixelWidth);
		}
		
		static final void intersect( double x1, double y1, double x2, double y2, double y, RasterizationContext ctx) {
			if ( y1 < y2 ) {
				if ( (y < y1) || (y > y2) ) {
					return;
				}
		    } else if ( y1 > y2 ) {
		    	if ( (y > y1) || (y < y2) ) {
		    		return;
		    	}
		    } else {
		    	if ( y != y1 ) {
		    		return;
		    	}
		    }
		    double x = (x1-x2) * ((y-y2) / (y1-y2)) + x2;
		    if ( x1 < x2 ) {
		    	if ( (x < x1) || (x > x2) ) {
		    		return;
		    	}
		    } else if ( x1 > x2 ) {
		    	if ( (x > x1) || (x < x2) ) {
		    		return;
		    	}
		    } else {
		    	if ( x != x1 ) {
		    		return;
		    	}
		    }
		    int i = ctx.xIntersects - 1;
		    while (i >= 0) {
		    	if (x < ctx.xIntersectScratch[i]) {
		    		ctx.xIntersectScratch[i+1] = ctx.xIntersectScratch[i];
		    	}
		    	--i;
		    }
		    ++i;
		    ctx.xIntersectScratch[i] = x;
		    ctx.xIntersects++;
		}
		
		/**
		 * Calculate which pixels (pixel centers) this polygon covers.
		 * RasterizationContext.pxPos should probably be 0 before entering this function,
		 * unless you want to run pixels from multiple polygons together.
		 * @param ctx geometry comes in here, scratch space for x intercepts, pixels out
		 * @return list of x,y pairs of pixels that this polygon rasterizes to
		 */
		public void rasterize(RasterizationContext ctx) {
			// double imMinx, double imMaxy, double pixelHeight, double pixelWidth, int ypx, int xpx
			// Pixel 0,0 is top left at minx,maxy
			int py = pcenterBelow(ymax, ctx.maxy, ctx.pixelHeight);
			if (py < 0) {
				py = 0;
			}
			double y = pcenterY(py, ctx.maxy, ctx.pixelHeight);

			if (ctx.xIntersectScratch.length < (points.length / 2)) {
				// on the crazy outside limit, we intersect with all of the line segments, or something.
				ctx.xIntersectScratch = new double[points.length / 2];
			}

			// for each scan row that fits within this polygon and the greater context...
			while ((y >= ymin) && (py < ctx.ypx)) {
				ctx.xIntersects = 0;
				// Intersect each loop of lines at current scan line.
				for (int parti = 0; parti < parts.length; ++parti) {
					int partend;
					if (parti + 1 < parts.length) {
						partend = parts[parti+1] - 1;
					} else {
						partend = (points.length / 2) - 1;
					}
					for (int pointi = parts[parti]; pointi < partend; ++pointi) {
						intersect(points[pointi*2], points[pointi*2 + 1], points[pointi*2 + 2], points[pointi*2 + 3], y, ctx);
					}
				}
				//assert(ctx.xIntersects > 0);
				assert(ctx.xIntersects % 2 == 0);
				
				// For all start-stop pairs, draw pixels from start edge to end edge
				for (int xi = 0; xi < ctx.xIntersects; xi += 2) {
					int px = pcenterRight(ctx.xIntersectScratch[xi], ctx.minx, ctx.pixelWidth);
					if (px < 0) {
						px = 0;
					}
					double x = pcenterX(px, ctx.minx, ctx.pixelWidth);
					// Draw pixels from start edge to end edge
					while ((x < ctx.xIntersectScratch[xi+1]) && (px < ctx.xpx)) {
						ctx.addPixel(px, py);
						px++;
						x = pcenterX(px, ctx.minx, ctx.pixelWidth);
					}
				}
				
				py++;
				y = pcenterY(py, ctx.maxy, ctx.pixelHeight);
			}
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
	
	public static final long blockidToUbid(byte[] blockid) {
		if (blockid.length == 15) {
			long out = (int)(blockid[2]) - (int)('0');
			for (int i = 3; i < blockid.length; ++i) {
				out = out * 10;
				out += (int)(blockid[i]) - (int)('0');
			}
			return out;
		}
		return -1;
	}
	
	public Redata.MapRasterization makeRasterization(int px, int py) {
		// TODO: could multi-thread this.
		RasterizationContext ctx = new RasterizationContext(this, px, py);
		Redata.MapRasterization.Builder rastb = Redata.MapRasterization.newBuilder();
		rastb.setSizex(px);
		rastb.setSizey(py);
		for (Polygon p : polys) {
			ctx.pxPos = 0;
			p.rasterize(ctx);
			Redata.MapRasterization.Block.Builder bb = Redata.MapRasterization.Block.newBuilder();
			bb.setUbid(blockidToUbid(p.blockid));
			//bb.setBlockid(ByteString.copyFrom(p.blockid));
			for (int i = 0; i < ctx.pxPos; ++i) {
				bb.addXy(ctx.pixels[i]);
			}
			rastb.addBlock(bb);
		}
		return rastb.build();
	}
	public void writeRasterization(OutputStream os, int px, int py) throws IOException {
		Redata.MapRasterization mr = makeRasterization(px, py);
		mr.writeTo(os);
	}
	
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
	public static class BlockIdComparator implements Comparator<byte[]> {
		@Override
		public int compare(byte[] o1, byte[] o2) {
			return ShapefileBundle.cmp(o1, o2);
		}
	}
	
	public static void main(String[] argv) throws IOException {
		boolean tree = true;
		int px = -1;
		int py = -1;
		int boundx = 1920;
		int boundy = 1080;
		String inname = null;
		String linksOut = null;
		String rastOut = null;
		boolean doLinks = false;
		boolean doRast = false;
		
		
		for (int i = 0; i < argv.length; ++i) {
			if (argv[i].endsWith(".zip")) {
				if (inname != null) {
					System.err.println("only one input allowed, already had: " + inname);
					System.exit(1);
					return;
				}
				inname = argv[i];
			} else if (argv[i].equals("--links")) {
				i++;
				linksOut = argv[i];
			} else if (argv[i].equals("--rast")) {
				i++;
				rastOut = argv[i];
			} else {
				System.err.println("bogus arg: " + argv[i]);
				System.exit(1);
				return;
			}
		}
		
		ShapefileBundle x = new ShapefileBundle();
		long start = System.currentTimeMillis();
		x.read(inname);
		long end = System.currentTimeMillis();
		System.out.println("read " + x.records() + " in " + ((end - start) / 1000.0) + " seconds");
		start = end;
		if (linksOut != null) {
			//int linkCount = x.printLinks(new FileWriter("/tmp/foo.links"));
			//int linkCount = x.printLinks(new FileOutputStream("/tmp/foo.links"));
			Map<byte[], Set<byte[]> > links;
			if (tree) {
				links = new TreeMap<byte[], Set<byte[]> >(new BlockIdComparator());
			} else {
				links = new HashMap<byte[], Set<byte[]> >();
			}
			int linksMapped = x.mapLinks(links);
			end = System.currentTimeMillis();
			System.out.println("calculated " + linksMapped + " links in " + ((end - start) / 1000.0) + " seconds, links.size()=" + links.size());
			start = end;
			OutputStream out = new FileOutputStream(linksOut);
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
			end = System.currentTimeMillis();
			System.out.println("wrote " + linkCount + " links in " + ((end - start) / 1000.0) + " seconds");
			start = end;
		}
		if (rastOut != null) {
			if (px == -1 || py == -1) {
				double width = x.shp.header.xmax - x.shp.header.xmin;
				double height = x.shp.header.ymax - x.shp.header.ymin;
				double w2 = width * Math.cos(Math.abs((x.shp.header.ymax + x.shp.header.ymin) / 2.0) * Math.PI / 180.0);
				double ratio = height / w2;
				double boundRatio = (boundy * 1.0) / boundx;
				if (ratio > boundRatio) {
					// state is too tall
					py = boundy;
					px = (int)(py / ratio);
				} else {
					px = boundx;
					py = (int)(ratio * px);
				}
			}
			FileOutputStream fos = new FileOutputStream(rastOut);
			GZIPOutputStream gos = new GZIPOutputStream(fos);
			x.writeRasterization(gos, px, py);
			gos.flush();
			fos.flush();
			gos.close();
			fos.close();
			end = System.currentTimeMillis();
			System.out.println("rasterized and written in " + ((end - start) / 1000.0) + " seconds");
			start = end;
		}
	}
}
