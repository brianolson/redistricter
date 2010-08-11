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
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
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
	static java.util.logging.Logger log = java.util.logging.Logger.getLogger(
		"org.bolson.redistricter");
	
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
				assert(false);
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
		public int mapLinks(SetLink out, Polygon b) {
			int count = 0;
			for (Polygon a : polys) {
				if (a == b) {
					continue;
				}
				if (a.hasTwoPointsInCommon(b)) {
					if (out.setLink(a.blockid, b.blockid)) {
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
		
		public int mapLinks(SetLink out, Polygon p) {
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
	
	public static class RasterizationOptions {
		public double minx = Double.NaN;
		public double miny = Double.NaN;
		public double maxx = Double.NaN;
		public double maxy = Double.NaN;
		public int xpx = -1;
		public int ypx = -1;
		
		public void setBoundsFromShapefile(ShapefileBundle shp, boolean override) {
			if (override || Double.isNaN(minx)) {
				minx = shp.shp.header.xmin;
			}
			if (override || Double.isNaN(miny)) {
				miny = shp.shp.header.ymin;
			}
			if (override || Double.isNaN(maxx)) {
				maxx = shp.shp.header.xmax;
			}
			if (override || Double.isNaN(maxy)) {
				maxy = shp.shp.header.ymax;
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

	/**
	 * An ESRI Shapefile Polygon (type 5) object.
	 * Knows how to rasterize itself.
	 * @author Brian Olson
	 *
	 */
	static class Polygon {
		public double xmin, xmax, ymin, ymax;
		public int[] parts;
		public double[] points;
		public byte[] blockid;
		boolean isWater = false;
		
		/**
		 * Calls init.
		 * @param data
		 * @param offset
		 * @param length
		 * @see #init(byte[],int,int)
		 */
		public Polygon(byte[] data, int offset, int length) {
			init(data, offset, length);
		}
		/**
		 * Parse binary data into structure in memory.
		 * @param data The bytes after the (numer,length) record header.
		 * @param offset offset into data[]
		 * @param length number of bytes in record
		 */
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
			assert(isConsistent());
		}
		/**
		 * Check that all point are within min-max and that loops are closed.
		 * @return true if all is well
		 */
		public boolean isConsistent() {
			for (int i = 0; i < parts.length; ++i) {
				int start = parts[i];
				int end;
				if ((i + 1) < parts.length) {
					end = parts[i+1] - 1;
				} else {
					end = (points.length / 2) - 1;
				}
				start *= 2;
				end *= 2;
				if (points[start] != points[end]) {
					return false;
				}
				if (points[start + 1] != points[end + 1]) {
					return false;
				}
			}
			for (int i = 0; i < points.length; i += 2) {
				if (points[i] > xmax) {
					return false;
				}
				if (points[i] < xmin) {
					return false;
				}
				if (points[i+1] > ymax) {
					return false;
				}
				if (points[i+1] < ymin) {
					return false;
				}
			}
			return true;
		}
		
		/**
		 * Compare points lists, return true if two are shared.
		 * TODO: this would be better if it were two <em>consecutive</em> points. OR, use per-county edge+face data for block adjacency.
		 * @param b the other Polygon
		 * @return true if both polygons have two points in common.
		 */
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
		
		/** for some y, what is the next pixel center below that? */
		static final int pcenterBelow(double somey, double maxy, double pixelHeight) {
			return (int)Math.floor( ((maxy - somey) / pixelHeight) + 0.5 );
		}
		/** for some x, what is the next pixel center to the right? */
		static final int pcenterRight(double somex, double minx, double pixelWidth) {
			return (int)Math.floor( ((somex - minx) / pixelWidth) + 0.5 );
		}
		/** for some y, what is the nearest pixel center? */
		static final int posToPixelY(double somey, double maxy, double pixelHeight) {
			return (int)Math.round( ((maxy - somey) / pixelHeight) + 0.5 );
		}
		/** for some x, what is the nearest pixel center? */
		static final int posToPixelX(double somex, double minx, double pixelWidth) {
			return (int)Math.round( ((somex - minx) / pixelWidth) + 0.5 );
		}
		/** The y coordinate of the center of a pixel */
		static final double pcenterY( int py, double maxy, double pixelHeight ) {
			return maxy - ((py + 0.5) * pixelHeight);
		}
		/** The x coordinate of the center of a pixel */
		static final double pcenterX( int px, double minx, double pixelWidth ) {
			return minx + ((px + 0.5) * pixelWidth);
		}
		
		/**
		 * Scanning row at y, set x intercept for line segment (x1,y1)(x2,y2) in ctx. 
		 * @param x1
		 * @param y1
		 * @param x2
		 * @param y2
		 * @param y
		 * @param ctx
		 */
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
		    int i = ctx.xIntersects;
		    while (i > 0) {
		    	if (x < ctx.xIntersectScratch[i-1]) {
		    		ctx.xIntersectScratch[i] = ctx.xIntersectScratch[i-1];
		    		--i;
		    	} else {
		    		break;
		    	}
		    }
		    ctx.xIntersectScratch[i] = x;
		    ctx.xIntersects++;
		}
		
		/**
		 * Calculate which pixels (pixel centers) this polygon covers.
		 * RasterizationContext.pxPos should probably be 0 before entering this function,
		 * unless you want to run pixels from multiple polygons together.
		 * 
		 * Top left pixel is (0,0), but geometry coordinates have bottom left (0,0), so
		 * Y is inverted throughout.
		 * 
		 * @param ctx geometry comes in here, scratch space for x intercepts, pixels out
		 * @return list of x,y pairs of pixels that this polygon rasterizes to (left in ctx)
		 */
		public void rasterize(RasterizationContext ctx) {
			// double imMinx, double imMaxy, double pixelHeight, double pixelWidth, int ypx, int xpx
			// Pixel 0,0 is top left at minx,maxy
			double rymax = ymax;
			if (rymax > ctx.maxy){
				rymax = ctx.maxy;
			}
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
		
		/**
		 * TODO: draw just the outline of the Polygon. Bresenham!
		 * @param ctx Destination for pixels of the outline of the Polygon.
		 */
		public void drawEdges(RasterizationContext ctx) {
			for (int parti = 0; parti < parts.length; ++parti) {
				// for each loop of lines...
				int partend;
				if (parti + 1 < parts.length) {
					partend = parts[parti+1] - 1;
				} else {
					partend = (points.length / 2) - 1;
				}
				for (int pointi = parts[parti]; pointi < partend; ++pointi) {
					// for each line in each loop, draw it
					bresenham(points[pointi*2], points[pointi*2 + 1], points[pointi*2 + 2], points[pointi*2 + 3], ctx);
				}
			}
		}
		
		public static void bresenham(double x0, double y0, double x1, double y1, RasterizationContext ctx) {
			bresenham(
					/*
					posToPixelX(x0, ctx.minx, ctx.pixelWidth),
					posToPixelY(y0, ctx.maxy, ctx.pixelHeight),
					posToPixelX(x1, ctx.minx, ctx.pixelWidth),
					posToPixelY(y1, ctx.maxy, ctx.pixelHeight),
					*/
					pcenterRight(x0, ctx.minx, ctx.pixelWidth),
					pcenterBelow(y0, ctx.maxy, ctx.pixelHeight),
					pcenterRight(x1, ctx.minx, ctx.pixelWidth),
					pcenterBelow(y1, ctx.maxy, ctx.pixelHeight),

					ctx
					);
		}
		public static void bresenham(int x0, int y0, int x1, int y1, RasterizationContext ctx) {
			/* http://en.wikipedia.org/wiki/Bresenham's_line_algorithm
			 function line(x0, x1, y0, y1)
			     boolean steep := abs(y1 - y0) > abs(x1 - x0)
			     if steep then
			         swap(x0, y0)
			         swap(x1, y1)
			     if x0 > x1 then
			         swap(x0, x1)
			         swap(y0, y1)
			     int deltax := x1 - x0
			     int deltay := abs(y1 - y0)
			     int error := deltax / 2
			     int ystep
			     int y := y0
			     if y0 < y1 then ystep := 1 else ystep := -1
			     for x from x0 to x1
			         if steep then plot(y,x) else plot(x,y)
			         error := error - deltay
			         if error < 0 then
			             y := y + ystep
			             error := error + deltax
						 */
			boolean steep = Math.abs(y1 - y0) > Math.abs(x1 - x0);
			if (steep) {
				// "x" is the steep axis that always increments, "y" sometimes increments.
				int t = x0;
				x0 = y0;
				y0 = t;
				t = x1;
				x1 = y1;
				y1 = t;
			}
			if (x0 > x1) {
				int t = x0;
				x0 = x1;
				x1 = t;
				t = y0;
				y0 = y1;
				y1 = t;
			}
			int deltax = x1 - x0;
			int deltay = Math.abs(y1 - y0);
			int error = deltax / 2;
			int y = y0;
			int ystep = (y0 < y1) ? 1 : -1;
			for (int x = x0; x <= x1; x++) {
				if (steep) {
					ctx.addPixel(y, x);
				} else {
					ctx.addPixel(x, y);
				}
				error -= deltay;
				if (error < 0) {
					y += ystep;
					error += deltax;
				}
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
	Shapefile shp = null;
	DBase dbf = null;
	ArrayList<Polygon> polys = new ArrayList<Polygon>();
	PolygonBucketArray pba = null;
	
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
	
	int blocklimit = 0x7fffffff;
	static final int randColorRange = 150;
	static final int randColorOffset = 100;
	//boolean colorMask = false;
	//boolean colorMaskRandom = false;
	public boolean outline = false;
	private RasterizationOptions rastOpts;
	
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
		BufferedImage mask;
		public boolean colorMask = true;
		public boolean colorMaskRandom = true;
		int polyindex = 0;
		public boolean doPolyNames = true;
		public java.awt.Font baseFont = new Font("Helvectica", 0, 12);
		public java.awt.Color textColor = new java.awt.Color(235, 235, 235, 50);
		public int waterColor = 0x996666ff;
		Graphics2D g_ = null;
		
		BufferedImageRasterizer(BufferedImage imageOut) {
			mask = imageOut;
		}
		
		Graphics2D graphics() {
			if (g_ == null) {
				g_ = mask.createGraphics();
			}
			return g_;
		}
		
		@Override
		public void setRasterizedPolygon(RasterizationContext ctx, Polygon p) {
			int argb;
			if (colorMask) {
				if (colorMaskRandom) {
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
				argb = waterColor;
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
			if (doPolyNames) {
				Graphics2D g = graphics();
				String polyName = blockidToString(p.blockid);
				Rectangle2D stringsize = baseFont.getStringBounds(polyName, g.getFontRenderContext());
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
				Font currentFont = baseFont.deriveFont((float)newFontSize);
				g.setFont(currentFont);
				g.setColor(textColor);
				stringsize = currentFont.getStringBounds(polyName, g.getFontRenderContext());
				g.drawString(polyName, 
						(float)((maxx + minx - stringsize.getWidth()) / 2),
						(float)((maxy + miny + stringsize.getHeight()) / 2));
			}
		}

		@Override
		/**
		 * Doesn't actually set size in this implementation, but asserts that buffer is at least that big.
		 */
		public void setSize(int x, int y) {
			assert(mask.getHeight() >= y);
			assert(mask.getWidth() >= x);
		}
		
	}
	
	/**
	 * Write pixels to a MapRasterization protobuf.
	 * @author bolson
	 * @see Redata.MapRasterization
	 */
	public static class MapRasterizationReceiver implements RasterizationReciever {
		public Redata.MapRasterization.Builder rastb = Redata.MapRasterization.newBuilder();
		
		@Override
		public void setRasterizedPolygon(RasterizationContext ctx, Polygon p) {
			if (p.blockid != null) {
				log.log(Level.FINE, "blockid {0}", new String(p.blockid));
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

		@Override
		public void setSize(int x, int y) {
			rastb.setSizex(x);
			rastb.setSizey(y);
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
		@Override
		public void draw(Polygon p, RasterizationContext ctx) {
			p.rasterize(ctx);
		}
		public static final PolygonFillRasterize singleton = new PolygonFillRasterize();
		/** Only use the singleton */
		private PolygonFillRasterize() {}
	}
	
	public void makeRasterization(Iterable<RasterizationReciever> they, PolygonDrawMode drawMode) {
		RasterizationContext ctx = new RasterizationContext(rastOpts);
		assert rastOpts.xpx > 0;
		assert rastOpts.ypx > 0;
		for (RasterizationReciever rr : they) {
			rr.setSize(rastOpts.xpx, rastOpts.ypx);
		}
		for (Polygon p : polys) {
			ctx.pxPos = 0;
			drawMode.draw(p, ctx);
			for (RasterizationReciever rr : they) {
				rr.setRasterizedPolygon(ctx, p);
			}
			if (--blocklimit < 0) {
				break;
			}
		}
	}
	
	/**
	 * Read shapefile bundle from foo.zip
	 * @param filename
	 * @throws IOException
	 */
	public void read(String filename) throws IOException {
		int lastSlash = filename.lastIndexOf('/');
		assert(filename.endsWith(".zip"));
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
		read(shp, dbf);
		f.close();
	}
	
	public static class CompositeDBaseField extends DBaseFieldDescriptor {
		protected ArrayList<DBaseFieldDescriptor> subFields = new ArrayList<DBaseFieldDescriptor>(); 

		public CompositeDBaseField() {
			super();
			length = 0;
		}
		
		public void add(DBaseFieldDescriptor field) {
			subFields.add(field);
			length += field.length;
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
	 * @throws IOException
	 */
	public void read(Shapefile shp, DBase dbf) throws IOException {
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
			if ((state != null) && (county != null) && (tract != null) && (block != null) && (suffix != null)) {
				CompositeDBaseField cfield = new CompositeDBaseField();
				cfield.add(state);
				cfield.add(county);
				cfield.add(tract);
				cfield.add(block);
				cfield.add(suffix);
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
		Polygon p = shp.next();
		
		pba = new PolygonBucketArray(shp, 20, 20);

		while (p != null) {
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
			polys.add(p);
			pba.add(p);
			//System.out.println(p);
			p = shp.next();
		}
		// TODO: assert that polys and dbf are both empty at the end.
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
	public static class MapLinkThread implements Runnable {
		Iterator<Polygon> polys;
		SetLink out;
		PolygonBucketArray pba;
		public int count = 0;
		
		public MapLinkThread(Iterator<Polygon> source, SetLink dest, PolygonBucketArray data) {
			polys = source;
			out = dest;
			pba = data;
		}
		
		//@Override // javac 1.5 doesn't like this
		public void run() {
			while (true) {
				Polygon p = null;
				synchronized (polys) {
					if (!polys.hasNext()) {
						return;
					}
					p = polys.next();
				}
				count += pba.mapLinks(out, p);
			}
		}
	}
	public int mapLinks(SetLink out, int threads) {
		int count = 0;
		MapLinkThread[] linkers = new MapLinkThread[threads];
		Thread[] runners = new Thread[threads];
		SetLink sout = new SynchronizingSetLink(out);
		Iterator<Polygon> pi = polys.iterator();
		for (int i = 0; i < threads; ++i) {
			linkers[i] = new MapLinkThread(pi, sout, pba);
			runners[i] = new Thread(linkers[i]);
			runners[i].start();
		}
		for (int i = 0; i < threads; ++i) {
			try {
				runners[i].join();
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
			count += linkers[i].count;
		}
		return count;
	}

	// TODO: check for whole-map contiguity, add fix-up links.
	// Or I could just leave that to be done in the existing C++ tool.
	public int mapLinks(SetLink out) {
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
		//@Override // javac 1.5 doesn't like this
		public int compare(byte[] o1, byte[] o2) {
			return ShapefileBundle.cmp(o1, o2);
		}
	}

	public void doLinks(String linksOut, boolean tree, int threads) throws IOException {
		long start = System.currentTimeMillis();
		Map<byte[], Set<byte[]> > links;
		if (tree) {
			links = new TreeMap<byte[], Set<byte[]> >(new BlockIdComparator());
		} else {
			links = new HashMap<byte[], Set<byte[]> >();
		}
		int linksMapped;
		if (threads > 1) {
			linksMapped = mapLinks(new MapSetLinkWrapper(links), threads);
		} else {
			linksMapped = mapLinks(new MapSetLinkWrapper(links));
		}
		long end = System.currentTimeMillis();
		log.info("calculated " + linksMapped + " links in " + ((end - start) / 1000.0) + " seconds, links.size()=" + links.size());
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
		log.info("wrote " + linkCount + " links in " + ((end - start) / 1000.0) + " seconds");
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
	
	public static void main(String[] argv) throws IOException {
		// TODO: take county and place (and more?) at the same time as tabblock and co-render all the layers
		boolean tree = true;
		//int px = -1;int py = -1;
		int boundx = 1920;
		int boundy = 1080;
		String inname = null;
		String linksOut = null;
		String rastOut = null;
		String maskOutName = null;
		int threads = 3;
		boolean colorMask = false;
		boolean outline = false;
		RasterizationOptions rastOpts = new RasterizationOptions();
		
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
			} else if (argv[i].equals("--color-mask")) {
				colorMask = true;
			} else if (argv[i].equals("--mask")) {
				i++;
				maskOutName = argv[i];
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
				rastOpts.minx = Double.parseDouble(argv[i]);
			} else if (argv[i].equals("--maxx")) {
				i++;
				rastOpts.maxx = Double.parseDouble(argv[i]);
			} else if (argv[i].equals("--miny")) {
				i++;
				rastOpts.miny = Double.parseDouble(argv[i]);
			} else if (argv[i].equals("--maxy")) {
				i++;
				rastOpts.maxy = Double.parseDouble(argv[i]);
			} else if (argv[i].equals("--outline")) {
				outline = true;
			} else if (argv[i].equals("--verbose")) {
				log.setLevel(Level.FINEST);
				log.info(log.getLevel().toString());
			} else {
				System.err.println("bogus arg: " + argv[i]);
				System.err.print(usage);
				System.exit(1);
				return;
			}
		}

		if (inname == null) {
			System.err.println("no input shapefile zip bundle specified");
			System.err.print(usage);
			System.exit(1);
		}
		
		ShapefileBundle x = new ShapefileBundle();
		x.outline  = outline;
		long start = System.currentTimeMillis();
		x.read(inname);
		long end = System.currentTimeMillis();
		log.info("read " + x.records() + " in " + ((end - start) / 1000.0) + " seconds");
		start = end;
		if (linksOut != null) {
			x.doLinks(linksOut, tree, threads);
			start = System.currentTimeMillis();
		}
		if ((rastOut != null) || (maskOutName != null)) {
			x.rastOpts = rastOpts;
			x.rastOpts.setBoundsFromShapefile(x, false);
			x.rastOpts.updatePixelSize(boundx, boundy);
			
			ArrayList<RasterizationReciever> outputs = new ArrayList<RasterizationReciever>();
			
			BufferedImage maskImage = null;
			OutputStream maskOutput = null;
			if (maskOutName != null) {
				maskImage = new BufferedImage(x.rastOpts.xpx, x.rastOpts.ypx, BufferedImage.TYPE_4BYTE_ABGR);
				BufferedImageRasterizer bir = new BufferedImageRasterizer(maskImage);
				bir.colorMask = colorMask;
				outputs.add(bir);
				maskOutput = new FileOutputStream(maskOutName);
			}
			
			FileOutputStream fos = null;
			GZIPOutputStream gos = null;
			MapRasterizationReceiver mrr = null;
			if (rastOut != null) {
				fos = new FileOutputStream(rastOut);
				gos = new GZIPOutputStream(fos);
				mrr = new MapRasterizationReceiver();
				outputs.add(mrr);
			}
			
			PolygonDrawMode mode = PolygonFillRasterize.singleton;
			if (outline) {
				mode = PolygonDrawEdges.singleton;
			}
			x.makeRasterization(outputs, mode);
			
			if (mrr != null && gos != null && fos != null) {
				Redata.MapRasterization mr = mrr.rastb.build();
				mr.writeTo(gos);
				gos.flush();
				fos.flush();
				gos.close();
				fos.close();
			}
			if (maskOutput != null && maskImage != null) {
				MapCanvas.writeBufferedImageAsPNG(maskOutput, maskImage);
			}
			end = System.currentTimeMillis();
			log.info("rasterized and written in " + ((end - start) / 1000.0) + " seconds");
			start = end;
		}
	}
}
