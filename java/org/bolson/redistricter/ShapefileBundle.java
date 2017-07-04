package org.bolson.redistricter;

import java.io.BufferedReader;
import java.io.DataInputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.Writer;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.logging.Level;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

//import org.bolson.redistricter.ShapefileBundle.BufferedImageRasterizer;


// TODO: draw water differently
// TODO: use one ShapefileBundle as a master scale template for others to be rendered as overlays
// TODO: take this apart into many smaller files.

/**
 * Things I need from the shapefile bundle:
 * Block adjacency
 * Rasterization
 * Geometry measurement {min,max}{lat,lon}
 *
 * This class implements both.
 */
public class ShapefileBundle {
	// TODO: requires Java 1.6
	//static java.util.logging.Logger log = java.util.logging.Logger.getLogger(java.util.logging.Logger.GLOBAL_LOGGER_NAME);
	static java.util.logging.Logger log = java.util.logging.Logger.getLogger("org.bolson.redistricter");
	
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
	
	static interface SetLink {
		/**
		 * Record a link between two blocks.
		 * @param a blockid
		 * @param b blockid
		 * @return true if link is new, false if not new or don't know.
		 */
		public boolean setLink(byte[] a, byte[] b);
	}
	static class MapSetLinkWrapper implements SetLink {
		public Map<byte[], Set<byte[]> > out;
		MapSetLinkWrapper(Map<byte[], Set<byte[]> > it) {
			out = it;
		}
		public boolean setLink(byte[] a, byte[] b) {
			int c = cmp(a, b);
			byte[] lower;
			byte[] upper;
			if (c < 0) {
				lower = a;
				upper = b;
			} else if (c > 0) {
				lower = b;
				upper = a;
			} else {
				return false;
			}
			Set<byte[]> rhset;
			if (!out.containsKey(lower)) {
				rhset = new HashSet<byte[]>();
				out.put(lower, rhset);
			} else {
				rhset = out.get(lower);
			}
			return rhset.add(upper);
		}
	}
	
	public static ArrayList<String> readLines(String path) throws IOException {
		FileReader fr = new FileReader(path);
		BufferedReader br = new BufferedReader(fr);
		String line = br.readLine();
		ArrayList<String> out = new ArrayList<String>();
		while (line != null) {
			out.add(line);
			line = br.readLine();
		}
		return out;
	}
	
	ZipFile bundle = null;
	Shapefile shp = null;
	DBase dbf = null;
	private Proj projection;
	
	public static final long blockidToUbid(byte[] blockid) {
		if (blockid.length <= 19) {
			try {
				return DBaseFieldDescriptor.byteArrayToLong(blockid);
			} catch (NumberFormatException e) {
				return -1;
			}
		}
		return -1;
	}
	
	public static final String blockidToString(byte[] blockid) {
		int start = 0;
		int end = -1;
		while (Character.isWhitespace(blockid[start])) {
			start++;
		}
		end = blockid.length;
		while (end > start) {
			end--;
			if (!Character.isWhitespace(blockid[end])) {
				if (end != blockid.length) {
					end++;
				}
				break;
			}
		}
		return new String(blockid, start, end);
	}

	public interface PolygonDrawMode {
		void draw(Polygon p, RasterizationContext ctx);
	}
	public static class PolygonDrawEdges implements PolygonDrawMode {
		public void draw(Polygon p, RasterizationContext ctx) {
			p.drawEdges(ctx);
		}
		public static final PolygonDrawEdges singleton = new PolygonDrawEdges();
		/** Only use the singleton */
		private PolygonDrawEdges() {}
	}
	public static class PolygonFillRasterize implements PolygonDrawMode {
		public void draw(Polygon p, RasterizationContext ctx) {
			p.rasterize(ctx);
		}
		public static final PolygonFillRasterize singleton = new PolygonFillRasterize();
		/** Only use the singleton */
		private PolygonFillRasterize() {}
	}

	/**
	 * Open shapefile bundle from foo.zip
	 * Leaves things ready to read headers, but body not processed.
	 * @param filename
	 * @throws IOException
	 */
	public void open(String filename) throws IOException {
		int lastSlash = filename.lastIndexOf('/');
		assert(filename.endsWith(".zip"));
		String nameroot = filename.substring(lastSlash+1, filename.length() - 4);
		
		bundle = new ZipFile(filename);
		openFinish(nameroot);
	}

	/**
	 * Open shapefile bundle from foo.zip
	 * Leaves things ready to read headers, but body not processed.
	 * @param filename
	 * @throws IOException
	 */
	public void open(Path filename) throws IOException {
		String nameroot = filename.getFileName().toString();
		assert(nameroot.endsWith(".zip"));
		nameroot = nameroot.substring(0, nameroot.length() - 4);
		bundle = new ZipFile(filename.toFile());
		openFinish(nameroot);
	}

	private void openFinish(String nameroot) throws IOException {
		ZipEntry shpEntry = bundle.getEntry(nameroot + ".shp");
		assert(shpEntry != null);
		InputStream shpIs = bundle.getInputStream(shpEntry);
		InputStream dbfIs = bundle.getInputStream(bundle.getEntry(nameroot + ".dbf"));

		shp = new Shapefile();
		shp.setInputStream(shpIs);
		shp.setProjection(projection);

		dbf = new DBase();
		dbf.setInputStream(new DataInputStream(dbfIs));
	}
	
	public static class CompositeDBaseField extends DBaseFieldDescriptor {
		protected ArrayList<DBaseFieldDescriptor> subFields = new ArrayList<DBaseFieldDescriptor>(); 

		public CompositeDBaseField() {
			super();
			length = 0;
			type = CHARACTER;
		}
		
		public void add(DBaseFieldDescriptor field) {
			subFields.add(field);
			length += field.length;
			if (name == null) {
				name = field.name;
			} else {
				name = name + "+" + field.name;
			}
		}
		
		public int getBytes(byte[] data, int offset, int length, byte[] out, int outOffset) {
			// TODO: assert(change in outOffset == this.length)
			for (DBaseFieldDescriptor field : subFields) {
				outOffset += field.getBytes(data, offset, length, out, outOffset);
			}
			return outOffset;
		}
	}
	
	/**
	 * Read a Shapefile and DBase pair.
	 * Loads all the polygons from the Shapefile and identifiers from the 
	 * corresponding records in the DBase file.
	 * @param shp
	 * @param dbf
	 * @return 
	 * @throws IOException
	 */
	public int read(Iterable<PolygonProcessor> pps) throws IOException {
		boolean y2kmode = false;
		// GEOID10 part of tabblock10
		DBaseFieldDescriptor blockIdField = dbf.getField("GEOID10");
		if (blockIdField == null) {
			// BLKIDFP tabblock00
			blockIdField = dbf.getField("BLKIDFP");
		}
		if (blockIdField == null) {
			// BLKIDFP00 tabblock00
			blockIdField = dbf.getField("BLKIDFP00");
		}
		if (blockIdField == null) {
			// NAMELSAD part of places
			blockIdField = dbf.getField("NAMELSAD");
		}
		if (blockIdField == null && !y2kmode) {
			// maybe synthesize blockId from parts of faces file
			DBaseFieldDescriptor state = dbf.getField("STATEFP10");
			DBaseFieldDescriptor county = dbf.getField("COUNTYFP10");
			DBaseFieldDescriptor tract = dbf.getField("TRACTCE10");
			//BLKGRPCE10
			DBaseFieldDescriptor block = dbf.getField("BLOCKCE10");
			//DBaseFieldDescriptor suffix = dbf.getField("SUFFIX1CE");
			if ((state != null) && (county != null) && (tract != null) && (block != null)) {
				CompositeDBaseField cfield = new CompositeDBaseField();
				cfield.add(state);
				cfield.add(county);
				cfield.add(tract);
				cfield.add(block);
				//cfield.add(suffix);
				blockIdField = cfield;
			}
		}
		if (blockIdField == null && y2kmode) {
			// maybe synthesize blockId from parts of faces file
			DBaseFieldDescriptor state = dbf.getField("STATEFP00");
			DBaseFieldDescriptor county = dbf.getField("COUNTYFP00");
			DBaseFieldDescriptor tract = dbf.getField("TRACTCE00");
			DBaseFieldDescriptor block = dbf.getField("BLOCKCE00");
			if ((state != null) && (county != null) && (tract != null) && (block != null)) {
				CompositeDBaseField cfield = new CompositeDBaseField();
				cfield.add(state);
				cfield.add(county);
				cfield.add(tract);
				cfield.add(block);
				blockIdField = cfield;
			}
		}
		if (blockIdField == null) {
			log.log(Level.WARNING, "BLKIDFP nor BLKIDFP00 not in DBase file: {0}", dbf);
		}
		DBaseFieldDescriptor isWaterField = dbf.getField("LWFLAG");
		byte[] lwflag = new byte[1];
		DBaseFieldDescriptor waterArea = dbf.getField("AWATER");
		DBaseFieldDescriptor landArea = dbf.getField("ALAND");
		if (waterArea == null) {
			waterArea = dbf.getField("AWATER00");
			landArea = dbf.getField("ALAND00");
		}
		
		if (log.isLoggable(Level.FINE)) {
			log.fine(shp.toString());
			log.fine(dbf.toString());
			log.fine("blockIdField=" + blockIdField);
			log.fine("isWaterField=" + isWaterField);
			log.fine("waterArea=" + waterArea);
			log.fine("landArea=" + landArea);
		}
		
		int count = 0;
		Polygon p = (Polygon)shp.next();

		while (p != null) {
			count++;
			byte[] rowbytes = dbf.next();
			if (blockIdField != null) {
				assert(rowbytes != null);
				p.blockid = blockIdField.getBytes(rowbytes, 0, rowbytes.length);
			} else {
				p.blockid = null;
			}
			if (isWaterField != null) {
				isWaterField.getBytes(rowbytes, 0, rowbytes.length, lwflag, 0);
				if (lwflag[0] != (byte)'L') {
					//log.info("lwflag=" + (char)(lwflag[0]));
					p.isWater = true;
				}
			} else if ((waterArea != null) && (landArea != null)) {
				long wa = waterArea.getLong(rowbytes, 0, rowbytes.length);
				long la = landArea.getLong(rowbytes, 0, rowbytes.length);
				if ((la == 0) && (wa > 0)) {
					p.isWater = true;
				}
			}
			for (PolygonProcessor pp : pps) {
				pp.process(p);
			}
			p = (Polygon)shp.next();
		}
		// TODO: assert that polys and dbf are both empty at the end.
		return count;
	}
	public int records() {
		assert(shp.recordCount == dbf.readCount);
		return shp.recordCount;
	}
	
	public static class SynchronizingSetLink implements SetLink {
		SetLink out;
		public SynchronizingSetLink(SetLink sub) {
			out = sub;
		}
		//@Override // javac 1.5 doesn't like this
		public boolean setLink(byte[] a, byte[] b) {
			boolean x;
			synchronized (out) {
				x = out.setLink(a, b);
			}
			return x;
		}
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
		//@Override // javac 1.5 doesn't like this
		public int compare(byte[] o1, byte[] o2) {
			return ShapefileBundle.cmp(o1, o2);
		}
	}

public static final String usage =
"--links outname.links\n" +
"--rast outname.mppb\n" +
"--mask outname.png\n" +
"--boundx <int>\n" +
"--boundy <int>\n" +
"--csvDist path\n" +
"--outlineOut path\n" +
"--rastgeom out path\n" +
"--proj dataset:id" +
"--verbose\n" +
"tl_2009_09_tabblock00.zip\n" +
"\n";
/*
"# options you probably won't need" +
--simple-rast # don't optimize pb
--outline # set outline rasterize mode
--flagfile path

--xpx int
--ypx int
--minx double
--maxx
--miny
--maxy
--threads n
--awidth buckets
--aheight buckets

 */

	public static void loggingInit() {
		// java.util.logging seems to require some stupid setup.
		java.util.logging.Logger plog = log;
		boolean noHandlerSetAll = true;
		while (plog != null) {
			//System.out.println(plog.toString());
			for (java.util.logging.Handler h : plog.getHandlers()) {
				//System.out.println(h);
				h.setLevel(Level.ALL);
				noHandlerSetAll = false;
			}
			plog = plog.getParent();
		}
		if (noHandlerSetAll) {
			java.util.logging.ConsoleHandler ch = new java.util.logging.ConsoleHandler();
			ch.setLevel(Level.ALL);
			log.addHandler(ch);
		}
	}
	
	public void setProjection(Proj projection) {
		this.projection = projection;
		if (this.projection != null) {
			log.info("bundle useing proj " + this.projection);
		}
	}
	
	/**
	 * --xpx int
--ypx int
--minx double
--maxx double
--miny double
--maxy double
--rast path out.mppb
--mask path out.png
--outline # set outline rasterize mode
--simple-rast # don't optimize pb
--flagfile path
--links out path, usually 'geoblocks.links'
--rastgeom out path, 
--threads n
--boundx n e.g. 1920
--boundy n e.g. 1080
--awidth buckets
--aheight buckets
--proj dataset:id, e.g. "NAD83:2001"
--verbose
	 * @param argv
	 * @throws IOException
	 */
	public static void main(String[] argv) throws IOException {
		// TODO: take county and place (and more?) at the same time as tabblock and co-render all the layers
		long totalStart = System.currentTimeMillis();
		loggingInit();
		
		RunContext x = new RunContext();
		x.main(argv);

		long totalEnd = System.currentTimeMillis();
		log.info("total time: " + ((totalEnd - totalStart) / 1000.0) + " seconds");
	}
	
	public String toString() {
		return bundle.getName();
	}
}
