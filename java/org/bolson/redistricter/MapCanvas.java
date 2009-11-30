package org.bolson.redistricter;

import java.awt.Canvas;
import java.awt.image.BufferedImage;
import java.awt.image.DataBuffer;
import java.awt.image.DataBufferByte;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.zip.DataFormatException;
import java.util.zip.GZIPInputStream;
import java.util.zip.Inflater;
import java.util.zip.InflaterInputStream;

import javax.imageio.ImageWriter;
import javax.imageio.spi.IIORegistry;
import javax.imageio.spi.ImageOutputStreamSpi;
import javax.imageio.spi.ImageWriterSpi;
import javax.imageio.stream.ImageOutputStream;

import org.bolson.redistricter.Redata.MapRasterization;

public class MapCanvas /*extends Canvas*/ {
	int minx;
	int maxx;
	int miny;
	int maxy;
	ArrayList<MBlockRange> ranges = new ArrayList<MBlockRange>();
	MBlockRange all = null;
	
	MapData data;
	
	public String toString() {
		StringBuffer out = new StringBuffer();
		out.append("MapCanvas minx=");
		out.append(minx);
		out.append(", maxx=");
		out.append(maxx);
		out.append(", miny=");
		out.append(miny);
		out.append(", maxy=");
		out.append(maxy);
		out.append("; ranges{");
		for (MBlockRange range : ranges) {
			range.appendToStringBuffer(out);
			out.append(", ");
		}
		out.append("}");
		return out.toString();
	}
	
	/**
	 * More efficient in-memory representation of MapRasterization.Block
	 * @author bolson
	 */
	public static class MBlock {
		public int[] xy;
		public long ubid = -1;
		public int recno = -1;
		
		public void CopyFrom(MapRasterization.Block it) {
			if (it.hasUbid()) {
				ubid = it.getUbid();
			}
			if (it.hasRecno()) {
				recno = it.getRecno();
			}
			xy = new int[it.getXyCount()];
			for (int i = 0; i < xy.length; ++i) {
				xy[i] = it.getXy(i);
			}
		}
	}
	
	public static class MBlockRange {
		public ArrayList<MBlock> they = new ArrayList<MBlock>();
		// values likely to get overridden by first thing added
		public int minx = 9999999;
		public int miny = 9999999;
		public int maxx = -9999999;
		public int maxy = -9999999;
		
		public void add(MapRasterization.Block it) {
			MBlock mb = new MBlock();
			mb.CopyFrom(it);
			add(mb);
		}

		public StringBuffer appendToStringBuffer(StringBuffer out) {
			out.append("(minx=");
			out.append(minx);
			out.append(",maxx=");
			out.append(maxx);
			out.append(",miny=");
			out.append(miny);
			out.append(",maxy=");
			out.append(maxy);
			out.append("; numblocks=");
			out.append(they.size());
			out.append(")");
			return out;
		}

		private void add(MBlock mb) {
			for (int i = 0; i < mb.xy.length; i += 2) {
				if (mb.xy[i  ] < minx) {
					minx = mb.xy[i];
				}
				if (mb.xy[i  ] > maxx) {
					maxx = mb.xy[i];
				}
				if (mb.xy[i+1] < miny) {
					miny = mb.xy[i+1];
				}
				if (mb.xy[i+1] > maxy) {
					maxy = mb.xy[i+1];
				}
			}
			they.add(mb);
		}
		
		/**
		 * Split this range into $count ranges along the X axis into list $out.
		 * @param count Number of sub-ranges to split into.
		 * @param out Destination for new MBlockRange objects.
		 */
		public void splitX(int count, ArrayList<MBlockRange> out) {
			// TODO WRITEME
			out.clear();
			out.add(this);
		}
	}

	/**
	 * Replace map with input.
	 * @param newpix
	 * @return old map pix
	 */
	public void setMap(MapRasterization newpix) {
		all = new MBlockRange();
		for (int i = 0; i < newpix.getBlockCount(); ++i) {
			all.add(newpix.getBlock(i));
		}
		ranges = new ArrayList<MBlockRange>();
		all.splitX(10, ranges);
		// gather stats of subranges
		boolean firstSubrange = true;
		for (MBlockRange subrange : ranges) {
			if (firstSubrange) {
				minx = subrange.minx;
				miny = subrange.miny;
				maxx = subrange.maxx;
				maxy = subrange.maxy;
				firstSubrange = false;
			} else {
				if (subrange.miny > miny) {
					miny = subrange.miny;
				}
				if (subrange.maxy < maxy) {
					maxy = subrange.maxy;
				}
				if (subrange.minx > minx) {
					minx = subrange.minx;
				}
				if (subrange.maxx < maxx) {
					maxx = subrange.maxx;
				}
			}
		}
	}
	
	public static final byte ff = (byte)255;
	public static final byte ha = (byte)128;
	
	/**
	 * array of byte[4] ABGR sets
	 */
	public static final byte[][] colors = {
		{ff, ff, 0, 0},
		{ff, 0, ff, 0},
		{ff, 0, 0, ff},
		{ff, ff, ff, 0},
		{ff, ff, 0, ff},
		{ff, 0, ff, ff},
		{ff, ha, ha, ha},
	};
	
	public void drawToImage(BufferedImage im, byte[] dsz) {
		DataBuffer db = im.getRaster().getDataBuffer();
		int imwidth = im.getWidth();
		int imheight = im.getHeight();
		if (db instanceof DataBufferByte) {
			DataBufferByte dbb = (DataBufferByte)db;
			byte[][] layers = dbb.getBankData();
			assert layers.length == 1;
			byte[] buf = layers[0];
			for (MBlock block : all.they) {
				// ABGR
				int dszi = data.indexForUbid(block.ubid);
				if (dszi < 0) {
					System.err.println("no index for ubid: " + block.ubid);
					continue;
				}
				byte winner = dsz[dszi];
				byte[] color = colors[winner % colors.length];
				for (int i = 0; i < block.xy.length; i += 2) {
					int y = block.xy[i + 1];
					if (y >= imheight) {
						continue;
					}
					int x = block.xy[i];
					if (x >= imwidth) {
						continue;
					}
					int pi = (imwidth * 4 * block.xy[i + 1]) + (4 * block.xy[i]);
					buf[pi + 0] = color[0];
					buf[pi + 1] = color[1];
					buf[pi + 2] = color[2];
					buf[pi + 3] = color[3];
				}
				// TODO
			}
		}
		System.out.println(db.getClass().getName());
		switch (db.getDataType()) {
		case DataBuffer.TYPE_BYTE:
			break;
		//case DataBuffer.TYPE_INT:
		//	break;
		default:
			throw new Error("unhandled DataBuffer type: " + db.getDataType() + ", " + db.getClass().getName());
		}
	}
	
	public static final int dszVersion = 4;
	public static int readUInt32(byte[] data, int offset, boolean bigEndian) {
		int out = 0;
		if (bigEndian) {
			out =
				(              data[offset  ] << 24) |
				(0x00ff0000 & (data[offset+1] << 16)) |
				(0x0000ff00 & (data[offset+2] <<  8)) |
				(0x000000ff & (data[offset+3]      ));
		} else {
			out =
				(              data[offset+3] << 24) |
				(0x00ff0000 & (data[offset+2] << 16)) |
				(0x0000ff00 & (data[offset+1] <<  8)) |
				(0x000000ff & (data[offset  ]      ));
		}
		return out;
	}
	// See Solver.cpp saveZSolution()
	// Format (variable endianness, use fileversion to detect):
	// uint32 fileversion
	// uint32 numPoints
	// uint32 compressed size
	// byte[compressed size] compressed data
	public static byte[] readDSZ(File f) throws IOException, DataFormatException {
		byte[] out = null;
		FileInputStream fis = new FileInputStream(f);
		byte[] header = new byte[12];
		int readlen = fis.read(header, 0, 12);
		if (readlen != 12) {
			throw new IOException("wanted 12 bytes but got " + readlen);
		}
		boolean bigendian = true;
		int version = readUInt32(header, 0, true); 
		if (version == dszVersion) {
			// bigendian = true
		} else if ((version = readUInt32(header, 0, false)) == dszVersion) {
			bigendian = false;
		} else {
			throw new IOException("failed to parse header as known version: " + header + ", " + version);
		}
		int numPoints = readUInt32(header, 4, bigendian);
		int compressedSize = readUInt32(header, 8, bigendian);
		byte[] winnerz = new byte[compressedSize];
		readlen = fis.read(winnerz);
		assert readlen == compressedSize;
		fis.close();
		out = new byte[numPoints];
		Inflater boom = new Inflater();
		boom.setInput(winnerz);
		readlen = boom.inflate(out);
		assert readlen == numPoints;
		return out;
	}
	
	public BufferedImage makeImage(byte[] dsz) {
		BufferedImage out = null;
		try {
			out = new BufferedImage(maxx - minx, maxy - miny, BufferedImage.TYPE_4BYTE_ABGR);
		} catch (OutOfMemoryError e) {
			System.err.println("failed to allocate memory for maxx=" + maxx + ", minx=" + minx + ", maxy=" + maxy + ", miny=" + miny);
			throw e;
		}
		drawToImage(out, dsz);
		return out;
	}
	public static ImageWriterSpi getPNGWriter() {
		ImageWriterSpi pngWriter = null;
		try {
			IIORegistry r = IIORegistry.getDefaultInstance();
			Iterator si = r.getServiceProviders( Class.forName("javax.imageio.spi.ImageWriterSpi"), true );
			while ( si.hasNext() ) {
				Object p;
				p = si.next();
				if ( p instanceof ImageWriterSpi ) {
					pngWriter = (ImageWriterSpi)p;
					if ( pngWriter.getDescription(null).matches(".*PNG.*") ) {
						//System.out.println("found PNG writer");
						break;
					}
					pngWriter = null;
				} else {
					System.out.println( "not an ImageWriterSpi: " + p.toString() );
				}
			}
		} catch ( Exception e ) {
			e.printStackTrace();
		}
		return pngWriter;
	}
	public static ImageOutputStreamSpi getOSIOSS() {
		ImageOutputStreamSpi oosws = null;
		try {
			IIORegistry r = IIORegistry.getDefaultInstance();
			Class outstreamclass = Class.forName("java.io.OutputStream");
			Iterator si = r.getServiceProviders( Class.forName("javax.imageio.spi.ImageOutputStreamSpi"), true );
			while ( si.hasNext() ) {
				Object p;
				p = si.next();
				oosws = (ImageOutputStreamSpi)p;
				Class oc;
				oc = oosws.getOutputClass();
				if ( oc.equals( outstreamclass ) ) {
					break;
				}
				//System.out.println( "wraps " + oc.toString() );
				oosws = null;
			}
		} catch ( Exception e ) {
			e.printStackTrace();
		}
		return oosws;
	}
	
	public void writePngFile(OutputStream fos, byte[] dsz) throws IOException {
		ImageWriterSpi pngWriter = getPNGWriter();
		ImageOutputStreamSpi oosws = getOSIOSS();
		ImageWriter piw = pngWriter.createWriterInstance();
		ImageOutputStream ios = oosws.createOutputStreamInstance( fos );
		piw.setOutput( ios );
		piw.write(makeImage(dsz));
		fos.flush();
	}

	public static void main(String[] argv) throws Exception {
		byte[] dsz = null;
		MapCanvas it = new MapCanvas();
		int i;
		String outname = null;
		i = 0;
		while (i < argv.length) {
			if (argv[i].equals("-d")) {
				++i;
				dsz = readDSZ(new File(argv[i]));
				System.out.println("read points: " + dsz.length);
			} else if (argv[i].equals("--mppb")) {
				++i;
				FileInputStream fin = new FileInputStream(argv[i]);
				InflaterInputStream gzin = new InflaterInputStream(fin);
				MapRasterization newpix = MapRasterization.parseFrom(gzin);
				it.setMap(newpix);
			} else if (argv[i].equals("--out")) {
				++i;
				outname = argv[i];
			} else if (argv[i].equals("-B")) {
				++i;
				it.data = new MapData();
				it.data.readGZFile(new File(argv[i]));
			} else {
				System.err.println("bogus arg: " + argv[i]);
				return;
			}
			++i;
		}
		System.out.println(it.toString());
		if (outname != null) {
			it.writePngFile(new FileOutputStream(outname), dsz);
		}
	}
	
	/**
	 * 
	 */
	private static final long serialVersionUID = 1L;

}
