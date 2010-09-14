package org.bolson.redistricter;

import java.awt.Font;
import java.awt.Graphics2D;
import java.awt.geom.Rectangle2D;
import java.awt.image.BufferedImage;
import java.io.DataInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.Writer;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.logging.Level;
import java.util.zip.GZIPOutputStream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

import com.google.protobuf.ByteString;

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
	
	public static class RasterizationOptions {
		public double minx = Double.NaN;
		public double miny = Double.NaN;
		public double maxx = Double.NaN;
		public double maxy = Double.NaN;
		public int xpx = -1;
		public int ypx = -1;
		String maskOutName;
		String rastOut;
		boolean outline = false;
		boolean optimizePb = true;
		
		public String toString() {
			return "RasterizationOptions(" + minx + "<x<" + maxx + ", " + miny + "<y<" + maxy + ", px=(" + xpx + "," + ypx + "))";
		}
		/**
		 * Bound will become the greater of this and shp.
		 * @param shp
		 * @throws IOException
		 */
		public void increaseBoundsFromShapefile(Shapefile shp) throws IOException {
			Shapefile.Header header = shp.getHeader();
			if (Double.isNaN(minx) || (minx > header.xmin)) {
				minx = header.xmin;
			}
			if (Double.isNaN(miny) || (miny > header.ymin)) {
				miny = header.ymin;
			}
			if (Double.isNaN(maxx) || (maxx < header.xmax)) {
				maxx = header.xmax;
			}
			if (Double.isNaN(maxy) || (maxy < header.ymax)) {
				maxy = header.ymax;
			}
		}
		
		public void setBoundsFromShapefile(Shapefile shp, boolean override) throws IOException {
			Shapefile.Header header = shp.getHeader();
			if (override || Double.isNaN(minx)) {
				minx = header.xmin;
			}
			if (override || Double.isNaN(miny)) {
				miny = header.ymin;
			}
			if (override || Double.isNaN(maxx)) {
				maxx = header.xmax;
			}
			if (override || Double.isNaN(maxy)) {
				maxy = header.ymax;
			}
		}
		
		/**
		 * Set xpx,ypx, up to boundx,boundy.
		 * @param boundx
		 * @param boundy
		 */
		public void updatePixelSize(int boundx, int boundy) {
			double width = maxx - minx;
			assert(width > 0.0);
			double height = maxy - miny;
			assert(height > 0.0);
			double w2 = width * Math.cos(Math.abs((maxy + miny) / 2.0) * Math.PI / 180.0);
			double ratio = height / w2;
			double boundRatio = (boundy * 1.0) / boundx;
			if (ratio > boundRatio) {
				// state is too tall
				ypx = boundy;
				xpx = (int)(ypx / ratio);
			} else {
				xpx = boundx;
				ypx = (int)(ratio * xpx);
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
		
		/**
		 * 
		 * @param shp
		 * @param px
		 * @param py
		 * @deprecated
		 */
		public RasterizationContext(ShapefileBundle shp, int px,
				int py) {
			xpx = px;
			ypx = py;
			minx = shp.shp.header.xmin;
			miny = shp.shp.header.ymin;
			maxx = shp.shp.header.xmax;
			maxy = shp.shp.header.ymax;
			updateParams();
		}
		public RasterizationContext(RasterizationOptions opts) {
			xpx = opts.xpx;
			ypx = opts.ypx;
			minx = opts.minx;
			miny = opts.miny;
			maxx = opts.maxx;
			maxy = opts.maxy;
			updateParams();
		}
		/** Call this after changing {min,max}{x,y} or [xy]px */
		public void updateParams() {
			pixelHeight = (maxy - miny) / ypx;
			pixelWidth = (maxx - minx) / xpx;
		}
		public void setBounds(double xmin, double ymin, double xmax, double ymax) {
			minx = xmin;
			miny = ymin;
			maxx = xmax;
			maxy = ymax;
			updateParams();
		}
		public void growPixels() {
			int[] npx = new int[pixels.length * 2];
			System.arraycopy(pixels, 0, npx, 0, pixels.length);
			pixels = npx;
		}
		public void addPixel(int x, int y) {
			// TODO: this is lame. clean up the math so I don't need a pixels of spill.
			if (y == ypx) {return;}
			if (y == -1) {return;}
			if (x == xpx) {return;}
			if (x == -1) {return;}
			assert(x >= 0);
			assert(x < xpx);
			assert(y >= 0);
			assert(y < ypx);
			if (pxPos == pixels.length) {
				growPixels();
			}
			pixels[pxPos] = x;
			pixels[pxPos+1] = y;
			pxPos += 2;
		}
	}

	ZipFile bundle = null;
	Shapefile shp = null;
	DBase dbf = null;
	
	public static class PolygonLinker implements PolygonProcessor {
		PolygonBucketArray pba = null;
		String linksOut = null;
		boolean tree;
		int threads;

		PolygonLinker(Shapefile shp, String linksOut, boolean tree, int threads) throws IOException {
			pba = new PolygonBucketArray(shp, 20, 20);
			this.linksOut = linksOut;
			this.tree = tree;
			this.threads = threads;
		}
		
		void growForShapefile(Shapefile shp) throws IOException {
			pba.growBoundsForShapefile(shp);
		}
		
		@Override
		public void process(Polygon p) {
			pba.add(p);
		}
		
		public void finish() throws IOException {
			pba.doLinks(linksOut, tree, threads);
		}

		@Override
		public String name() {
			return "linking";
		}
	}
	
	public static class PolygonRasterizer implements PolygonProcessor {
		RasterizationOptions rastOpts;
		BufferedImageRasterizer.Options birOpts;
		RasterizationContext ctx = null;
		PolygonDrawMode drawMode = null;
		ArrayList<RasterizationReciever> outputs = new ArrayList<RasterizationReciever>();
		BufferedImage maskImage = null;
		OutputStream maskOutput = null;
		FileOutputStream fos = null;
		GZIPOutputStream gos = null;
		MapRasterizationReceiver mrr = null;
		
		@Override
		public void process(Polygon p) {
			ctx.pxPos = 0;
			drawMode.draw(p, ctx);
			for (RasterizationReciever rr : outputs) {
				rr.setRasterizedPolygon(ctx, p);
			}
		}
		
		public PolygonRasterizer(RasterizationOptions rastOpts) throws IOException {
			this.rastOpts = rastOpts;
			ctx = new RasterizationContext(rastOpts);
			assert rastOpts.xpx > 0;
			assert rastOpts.ypx > 0;
			
			if (rastOpts.maskOutName != null) {
				log.info("will make mask \"" + rastOpts.maskOutName + "\"");
				log.info("x=" + rastOpts.xpx + " y=" + rastOpts.ypx);
				maskImage = new BufferedImage(rastOpts.xpx, rastOpts.ypx, BufferedImage.TYPE_4BYTE_ABGR);
				BufferedImageRasterizer bir = new BufferedImageRasterizer(maskImage, birOpts);
				outputs.add(bir);
				maskOutput = new FileOutputStream(rastOpts.maskOutName);
			}
			
			if (rastOpts.rastOut != null) {
				log.info("will make rast data \"" + rastOpts.rastOut + "\"");
				fos = new FileOutputStream(rastOpts.rastOut);
				gos = new GZIPOutputStream(fos);
				mrr = new MapRasterizationReceiver(rastOpts.optimizePb);
				outputs.add(mrr);
			}
			
			drawMode = PolygonFillRasterize.singleton;
			if (rastOpts.outline) {
				drawMode = PolygonDrawEdges.singleton;
			}
			for (RasterizationReciever rr : outputs) {
				rr.setSize(rastOpts.xpx, rastOpts.ypx);
			}
		}
		public void finish() throws IOException {
			if (mrr != null && gos != null && fos != null) {
				Redata.MapRasterization mr = mrr.getMapRasterization();
				mr.writeTo(gos);
				gos.flush();
				fos.flush();
				gos.close();
				fos.close();
			}
			if (maskOutput != null && maskImage != null) {
				MapCanvas.writeBufferedImageAsPNG(maskOutput, maskImage);
			}
		}

		@Override
		public String name() {
			return "rasterization";
		}
	}
	
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

	public interface RasterizationReciever {
		/**
		 * Set the size of the total rasterization we are about to make.
		 * @param x
		 * @param y
		 */
		void setSize(int x, int y);
		
		/**
		 * Polygon loaded from Shapefile, with pixels in the RasterizationContext.
		 * @param ctx pixels in here, ctx.pixels[] in x,y pairs
		 * @param p other info in here
		 */
		void setRasterizedPolygon(RasterizationContext ctx, Polygon p);
	}
	
	/**
	 * Write pixels to a java.awt.image.BufferedImage
	 * @author bolson
	 * @see java.awt.image.BufferedImage
	 */
	public static class BufferedImageRasterizer implements RasterizationReciever {
		/**
		 * Destination for mask image.
		 */
		protected BufferedImage mask;
		/**
		 * Used to rotate through colors.
		 */
		protected int polyindex = 0;
		/**
		 * Drawing context for mask.
		 * @see mask
		 */
		protected Graphics2D g_ = null;
		
		/**
		 * This should be treated as const. Read-only.
		 */
		public Options opts = null;
		
		public static class Options {
			public boolean colorMask = true;
			public boolean colorMaskRandom = true;
			public boolean doPolyNames = false;
			public java.awt.Font baseFont = new Font("Helvectica", 0, 12);
			public java.awt.Color textColor = new java.awt.Color(235, 235, 235, 50);
			public int waterColor = 0x996666ff;
			public int randColorRange = 150;
			public int randColorOffset = 10;
		}
		
		/**
		 * @param imageOut where to render to
		 * @param optsIn may be null
		 */
		BufferedImageRasterizer(BufferedImage imageOut, Options optsIn) {
			mask = imageOut;
			opts = optsIn;
			if (opts == null) {
				opts = new Options();
			}
		}
		
		Graphics2D graphics() {
			if (g_ == null) {
				g_ = mask.createGraphics();
			}
			return g_;
		}
		
		public void setRasterizedPolygon(RasterizationContext ctx, Polygon p) {
			int argb;
			int randColorOffset = opts.randColorOffset;
			int randColorRange = opts.randColorRange;
			if (opts.colorMask) {
				if (opts.colorMaskRandom) {
					argb = 0xff000000 |
					(((int)(Math.random() * randColorRange) + randColorOffset) << 16) |
					(((int)(Math.random() * randColorRange) + randColorOffset) << 8) |
					((int)(Math.random() * randColorRange) + randColorOffset);
				} else {
					argb = MapCanvas.colorsARGB[polyindex % MapCanvas.colorsARGB.length];
				}
			} else {
				argb = ((int)(Math.random() * randColorRange) + randColorOffset);
				argb = argb | (argb << 8) | (argb << 16) | 0xff000000;
			}
			if (p.isWater) {
				argb = opts.waterColor;
			}
			//log.log(Level.INFO, "poly {0} color {1}", new Object[]{new Integer(polyindex), Integer.toHexString(argb)});
			int minx = ctx.pixels[0];
			int maxx = ctx.pixels[0];
			int miny = ctx.pixels[1];
			int maxy = ctx.pixels[1];
			for (int i = 0; i < ctx.pxPos; i += 2) {
				mask.setRGB(ctx.pixels[i], ctx.pixels[i+1], argb);
				if (minx > ctx.pixels[i]) {
					minx = ctx.pixels[i];
				}
				if (maxx < ctx.pixels[i]) {
					maxx = ctx.pixels[i];
				}
				if (miny > ctx.pixels[i+1]) {
					miny = ctx.pixels[i+1];
				}
				if (maxy < ctx.pixels[i+1]) {
					maxy = ctx.pixels[i+1];
				}
			}
			if (opts.doPolyNames) {
				Graphics2D g = graphics();
				String polyName = blockidToString(p.blockid);
				Rectangle2D stringsize = opts.baseFont.getStringBounds(polyName, g.getFontRenderContext());
				// target height ((maxy - miny) / 5.0)
				// actual height stringsize.getHeight()
				// baseFont size 12.0
				// newFontSize  / 12.0 === target / actual
				// newFontSize = (target / actual) * 12.0
				double ysize = 12.0 * ((maxy - miny) / 3.0) / stringsize.getHeight();
				double xsize = 12.0 * ((maxx - minx) * 0.9) / stringsize.getWidth();
				double newFontSize;
				if (ysize < xsize) {
					newFontSize = ysize;
				} else {
					newFontSize = xsize;
				}
				Font currentFont = opts.baseFont.deriveFont((float)newFontSize);
				g.setFont(currentFont);
				g.setColor(opts.textColor);
				stringsize = currentFont.getStringBounds(polyName, g.getFontRenderContext());
				g.drawString(polyName, 
						(float)((maxx + minx - stringsize.getWidth()) / 2),
						(float)((maxy + miny + stringsize.getHeight()) / 2));
			}
		}

		// @Override // one of my Java 1.5 doesn't like this
		/**
		 * Doesn't actually set size in this implementation, but asserts that buffer is at least that big.
		 */
		public void setSize(int x, int y) {
			assert(mask.getHeight() >= y);
			assert(mask.getWidth() >= x);
		}
		
	}
	
	/**
	 * Efficient in-memory storage of MapRasterization.Block
	 * @author bolson
	 *
	 */
	public static class TemporaryBlockHolder implements Comparable<TemporaryBlockHolder> {
		long ubid;
		byte[] blockid;
		int[] xy;
		int[] water;

		/** Header only, for hashCode lookup */
		TemporaryBlockHolder(Polygon p) {
			ubid = blockidToUbid(p.blockid);
			if (ubid < 0) {
				blockid = p.blockid.clone();
			}
		}
		TemporaryBlockHolder(RasterizationContext ctx, Polygon p) {
			ubid = blockidToUbid(p.blockid);
			if (ubid < 0) {
				blockid = p.blockid.clone();
			}
			setRast(ctx, p);
		}
		public void setRast(RasterizationContext ctx, Polygon p) {
			if (p.isWater) {
				water = new int[ctx.pxPos];
				System.arraycopy(ctx.pixels, 0, water, 0, ctx.pxPos);
			} else {
				xy = new int[ctx.pxPos];
				System.arraycopy(ctx.pixels, 0, xy, 0, ctx.pxPos);
			}
		}
		
		public static int[] growIntArray(int[] orig, int[] src, int morelen) {
			if (orig == null) {
				int[] out = new int[morelen];
				System.arraycopy(out, 0, src, 0, morelen);
				return out;
			}
			int[] nxy = new int[orig.length + morelen];
			System.arraycopy(orig, 0, nxy, 0, orig.length);
			System.arraycopy(src, 0, nxy, orig.length, morelen);
			return nxy;
		}
		
		public void add(RasterizationContext ctx, Polygon p) {
			long tubid = blockidToUbid(p.blockid);
			assert tubid == ubid;
			if (ubid < 0) {
				assert java.util.Arrays.equals(blockid, p.blockid);
			}
			if (p.isWater) {
				water = growIntArray(water, ctx.pixels, ctx.pxPos);
			} else {
				xy = growIntArray(xy, ctx.pixels, ctx.pxPos);
			}
		}
		
		public int hashCode() {
			if (ubid >= 0) {
				return (int)ubid;
			}
			return blockid.hashCode();
		}
		
		public boolean equals(Object o) {
			if (ubid >= 0) {
				return ubid == ((TemporaryBlockHolder)o).ubid;
			}
			return java.util.Arrays.equals(blockid, ((TemporaryBlockHolder)o).blockid);
		}
		
		public Redata.MapRasterization.Block.Builder build() {
			Redata.MapRasterization.Block.Builder bb = Redata.MapRasterization.Block.newBuilder();
			if (ubid >= 0) {
				bb.setUbid(ubid);
			} else {
				bb.setBlockid(ByteString.copyFrom(blockid));
			}
			if (water != null) {
				for (int i = 0; i < water.length; ++i) {
					bb.addWaterxy(water[i]);
				}
			}
			if (xy != null) {
				for (int i = 0; i < xy.length; ++i) {
					bb.addXy(xy[i]);
				}
			}
			return bb;
		}
		@Override
		public int compareTo(TemporaryBlockHolder o) {
			if (ubid >= 0) {
				if (o.ubid >= 0) {
					if (ubid < o.ubid) {
						return -1;
					}
					if (ubid > o.ubid) {
						return 1;
					}
					return 0;
				} else {
					throw new ClassCastException();
				}
			}
			if (o.ubid >= 0) {
				throw new ClassCastException();
			}
			for (int i = 0; i < blockid.length && i < o.blockid.length; ++i) {
				if (blockid[i] < o.blockid[i]) {
					return -1;
				}
				if (blockid[i] > o.blockid[i]) {
					return 1;
				}
			}
			if (blockid.length < o.blockid.length) {
				return -1;
			}
			if (blockid.length > o.blockid.length) {
				return 1;
			}
			return 0;
		}
	}
	/**
	 * Write pixels to a MapRasterization protobuf.
	 * @author bolson
	 * @see Redata.MapRasterization
	 */
	public static class MapRasterizationReceiver implements RasterizationReciever {
		protected Redata.MapRasterization.Builder rastb = Redata.MapRasterization.newBuilder();
		TreeMap<TemporaryBlockHolder, TemporaryBlockHolder> blocks = null;
		
		MapRasterizationReceiver() {
		}
		/**
		 * 
		 * @param optimize Combine faces for the same block into one MapRasterization.Block structure,
		 * producing a slightly smaller output. Increases memory usage proportional to the number of pixels rendered.
		 * Only makes sense to set this when processing 'faces' data set, 'tabblock' doesn't need it.
		 */
		MapRasterizationReceiver(boolean optimize) {
			if (optimize) {
				blocks = new TreeMap<TemporaryBlockHolder, TemporaryBlockHolder>();
			}
		}
		// @Override // one of my Java 1.5 doesn't like this
		public void setRasterizedPolygon(RasterizationContext ctx, Polygon p) {
			if (p.blockid != null) {
				log.log(Level.FINE, "blockid {0}", new String(p.blockid));
				if (blocks != null) {
					// Save for later.
					TemporaryBlockHolder key = new TemporaryBlockHolder(p);
					TemporaryBlockHolder tbh = blocks.get(key);
					if (tbh == null) {
						key.setRast(ctx, p);
						blocks.put(key, key);
					} else {
						tbh.add(ctx, p);
					}
					return;
				}
				Redata.MapRasterization.Block.Builder bb = Redata.MapRasterization.Block.newBuilder();
				long ubid = blockidToUbid(p.blockid);
				if (ubid >= 0) {
					bb.setUbid(ubid);
				} else {
					bb.setBlockid(ByteString.copyFrom(p.blockid));
				}
				if (p.isWater) {
					for (int i = 0; i < ctx.pxPos; ++i) {
						bb.addWaterxy(ctx.pixels[i]);
					}
				} else {
					for (int i = 0; i < ctx.pxPos; ++i) {
						bb.addXy(ctx.pixels[i]);
					}
				}
				rastb.addBlock(bb);
			} else {
				log.warning("polygon with no blockid");
			}
		}

		// @Override  // one of my Java 1.5 doesn't like this
		public void setSize(int x, int y) {
			rastb.setSizex(x);
			rastb.setSizey(y);
		}
		
		public Redata.MapRasterization getMapRasterization() {
			if (blocks != null) {
				while (!blocks.isEmpty()) {
					TemporaryBlockHolder tb = blocks.firstKey();
					assert tb != null;
					rastb.addBlock(tb.build());
					// To make a temporary free-able, remove it from the map.
					blocks.remove(tb);
				}
			}
			return rastb.build();
		}
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
		ZipEntry shpEntry = bundle.getEntry(nameroot + ".shp");
		assert(shpEntry != null);
		InputStream shpIs = bundle.getInputStream(shpEntry);
		InputStream dbfIs = bundle.getInputStream(bundle.getEntry(nameroot + ".dbf"));
		
		shp = new Shapefile();
		shp.setInputStream(shpIs);
		
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
		boolean y2kmode = true;
		// BLKIDFP part of tabblock
		DBaseFieldDescriptor blockIdField = dbf.getField("BLKIDFP");
		if (blockIdField == null) {
			// BLKIDFP00 tabblock00
			blockIdField = dbf.getField("BLKIDFP00");
		}
		if (blockIdField == null) {
			// NAMELSAD part of places
			blockIdField = dbf.getField("NAMELSAD");
		}
		if (blockIdField == null) {
			// maybe synthesize blockId from parts of faces file
			DBaseFieldDescriptor state = dbf.getField("STATEFP00");
			DBaseFieldDescriptor county = dbf.getField("COUNTYFP00");
			DBaseFieldDescriptor tract = dbf.getField("TRACTCE00");
			DBaseFieldDescriptor block = dbf.getField("BLOCKCE00");
			DBaseFieldDescriptor suffix = dbf.getField("SUFFIX1CE");
			if ((state != null) && (county != null) && (tract != null) && (block != null) && (y2kmode || (suffix != null))) {
				CompositeDBaseField cfield = new CompositeDBaseField();
				cfield.add(state);
				cfield.add(county);
				cfield.add(tract);
				cfield.add(block);
				if (!y2kmode) {
					cfield.add(suffix);
				}
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
"--color-mask\n" +
"--threads <int>\n" +
"--boundx <int>\n" +
"--boundy <int>\n" +
"--outline\n" +
"--verbose\n" +
"tl_2009_09_tabblock00.zip\n";

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
	
	public static void main(String[] argv) throws IOException {
		loggingInit();
		// TODO: take county and place (and more?) at the same time as tabblock and co-render all the layers
		boolean tree = true;
		int boundx = 1920;
		int boundy = 1080;
		ArrayList<String> inputPaths = new ArrayList<String>();
		String linksOut = null;
		int threads = 3;
		RasterizationOptions rastOpts = new RasterizationOptions();
		double minx = Double.NaN;
		double miny = Double.NaN;
		double maxx = Double.NaN;
		double maxy = Double.NaN;
		BufferedImageRasterizer.Options birOpts = new BufferedImageRasterizer.Options();
		
		for (int i = 0; i < argv.length; ++i) {
			if (argv[i].endsWith(".zip")) {
				inputPaths.add(argv[i]);
			} else if (argv[i].equals("--links")) {
				i++;
				linksOut = argv[i];
			} else if (argv[i].equals("--rast")) {
				i++;
				rastOpts.rastOut = argv[i];
			} else if (argv[i].equals("--color-mask")) {
				birOpts.colorMask = true;
			} else if (argv[i].equals("--mask")) {
				i++;
				rastOpts.maskOutName = argv[i];
			} else if (argv[i].equals("--threads")) {
				i++;
				threads = Integer.parseInt(argv[i]);
			} else if (argv[i].equals("--boundx")) {
				// upper bound on pixel size
				i++;
				boundx = Integer.parseInt(argv[i]);
			} else if (argv[i].equals("--boundy")) {
				// upper bound on pixel size
				i++;
				boundy = Integer.parseInt(argv[i]);
			} else if (argv[i].equals("--minx")) {
				i++;
				minx = Double.parseDouble(argv[i]);
			} else if (argv[i].equals("--maxx")) {
				i++;
				maxx = Double.parseDouble(argv[i]);
			} else if (argv[i].equals("--miny")) {
				i++;
				miny = Double.parseDouble(argv[i]);
			} else if (argv[i].equals("--maxy")) {
				i++;
				maxy = Double.parseDouble(argv[i]);
			} else if (argv[i].equals("--outline")) {
				rastOpts.outline = true;
			} else if (argv[i].equals("--simple-rast")) {
				rastOpts.optimizePb = false;
			} else if (argv[i].equals("--verbose")) {
				log.setLevel(Level.FINEST);
				//log.info(log.getLevel().toString());
			} else {
				System.err.println("bogus arg: " + argv[i]);
				System.err.print(usage);
				System.exit(1);
				return;
			}
		}

		if (inputPaths.size() == 0) {
			System.err.println("no input shapefile zip bundle specified");
			System.err.print(usage);
			System.exit(1);
		}
		
		long start = System.currentTimeMillis();
		ArrayList<ShapefileBundle> bundles = new ArrayList<ShapefileBundle>();
		for (String path : inputPaths) {
			ShapefileBundle x = new ShapefileBundle();
			x.open(path);
			bundles.add(x);
		}
		ArrayList<PolygonProcessor> pps = new ArrayList<PolygonProcessor>();
		if (linksOut != null) {
			log.info("calculating links");
			PolygonLinker linker = new PolygonLinker(bundles.get(0).shp, linksOut, tree, threads);
			for (int i = 1; i < bundles.size(); ++i) {
				linker.growForShapefile(bundles.get(i).shp);
			}
			pps.add(linker);
		}
		if ((rastOpts.rastOut != null) || (rastOpts.maskOutName != null)) {
			log.info("rasterizing");
			rastOpts.increaseBoundsFromShapefile(bundles.get(0).shp);
			for (int i = 1; i < bundles.size(); ++i) {
				log.log(Level.FINE, "pre : {0}", rastOpts);
				rastOpts.increaseBoundsFromShapefile(bundles.get(i).shp);
				log.log(Level.FINE, "post: {0}", rastOpts);
			}
			if (!Double.isNaN(minx)) {
				rastOpts.minx = minx;
			}
			if (!Double.isNaN(miny)) {
				rastOpts.miny = miny;
			}
			if (!Double.isNaN(maxx)) {
				rastOpts.maxx = maxx;
			}
			if (!Double.isNaN(maxy)) {
				rastOpts.maxy = maxy;
			}
			rastOpts.updatePixelSize(boundx, boundy);
			log.info(rastOpts.toString());
			pps.add(new PolygonRasterizer(rastOpts));
		}
		long end = System.currentTimeMillis();
		log.info("setup done in " + ((end - start) / 1000.0) + " seconds");
		start = end;
		int count = 0;
		for (ShapefileBundle x : bundles) {
			log.info("processing " + x.toString());
			count += x.read(pps);
		}
		end = System.currentTimeMillis();
		log.info("read " + count + " in " + ((end - start) / 1000.0) + " seconds");
		start = end;
		for (PolygonProcessor pp : pps) {
			try {
				log.log(Level.INFO, "finishing {0}...", pp.name());
				pp.finish();
				end = System.currentTimeMillis();
				log.info(pp.name() + " finished in " + ((end - start) / 1000.0) + " seconds");
				start = end;
			} catch (Exception e) {
				e.printStackTrace();
			}
		}
	}
	
	public String toString() {
		return bundle.getName();
	}
}
