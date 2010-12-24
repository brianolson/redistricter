package org.bolson.redistricter;

import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.io.Writer;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;

import org.bolson.redistricter.ShapefileBundle.BlockIdComparator;
import org.bolson.redistricter.ShapefileBundle.MapSetLinkWrapper;
import org.bolson.redistricter.ShapefileBundle.SetLink;
import org.bolson.redistricter.ShapefileBundle.SynchronizingSetLink;

class PolygonBucketArray {
	/**
	 * Buckets wide.
	 */
	int width;
	
	/**
	 * Buckets high.
	 */
	int height;
	
	/**
	 * Bounds of polygon data.
	 */
	double minx, miny, maxx, maxy;
	
	/**
	 * Polygon-space per bucket.
	 */
	double dx, dy;
	
	/**
	 * 2d array in 1d array.
	 * PolygonBucket[width * height]
	 */
	protected PolygonBucket[] they;
	
	/**
	 * List of all polys as they were added.
	 */
	protected ArrayList<Polygon> polys = new ArrayList<Polygon>();
	
	/**
	 * 
	 * @param minx minimum x dimension
	 * @param miny minimum y dimension
	 * @param maxx maximum x dimension
	 * @param maxy maximum y dimension
	 * @param width buckets wide
	 * @param height buckets high
	 */
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
	
	/**
	 * Convenience method gets dimensions from shapefile.
	 * @param shp get {y,x}{min,max} from this
	 * @param width number of buckets wide this array is
	 * @param height number of buckets tall this array is
	 * @throws IOException 
	 */
	public PolygonBucketArray(Shapefile shp, int width, int height) throws IOException {
		Shapefile.Header header = shp.getHeader();
		this.minx = header.xmin;
		this.miny = header.ymin;
		this.maxx = header.xmax;
		this.maxy = header.ymax;
		this.width = width;
		this.height = height;
		init();
	}
	
	/**
	 * Bounds will become the outer bounding box of current bounds and shapefile.
	 * @param shp
	 * @throws IOException getting a shapefile header can be dangerous.
	 */
	public void growBoundsForShapefile(Shapefile shp) throws IOException {
		Shapefile.Header header = shp.getHeader();
		if (header.xmin < this.minx) {
			this.minx = header.xmin;
		}
		if (header.ymin < this.miny) { 
			this.miny = header.ymin;
		}
		if (header.xmax > this.maxx) {
			this.maxx = header.xmax;
		}
		if (header.ymax > this.maxy) {
			this.maxy = header.ymax;
		}
	}
	
	protected void init() {
		they = new PolygonBucket[width * height];
		// TODO: rework this math to be more roundoff resistant like bucketX
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
		ix = (int)Math.floor((width * (x-minx)) / (maxx - minx));
		if (ix == width) {
			// floating point error fixup. allow one fence step.
			ix = width - 1;
		}
		assert(ix >= 0);
		assert(ix < width);
		return ix;
	}
	
	int bucketY(double y) {
		assert(y >= miny);
		assert(y <= maxy);
		int iy;
		iy = (int)Math.floor((height * (y - miny)) / (maxy - miny)); 
		if (iy == height) {
			// Fixup. Allow one pixel of fence reigning in.
			iy = height - 1;
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
	
	@Deprecated
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
	
	@Deprecated
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
	
	/**
	 * Find links between a Polygon and everything in this container.
	 * @param out report links here
	 * @param p the Polygon to compare
	 * @return number of links found
	 */
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
	
	/**
	 * Find all links from all polys to all polys.
	 * @param out report links here
	 * @return number of links found
	 */
	public int mapLinks(SetLink out) {
		int count = 0;
		for (Polygon p : polys) {
			count += mapLinks(out, p);
		}
		return count;
	}
	
	/**
	 * Find all links from all polys to all polys, in parallel!
	 * @param out report links here
	 * @param threads number of threads to run
	 * @return number of links found
	 */
	public int mapLinks(SetLink out, int threads) {
		if (threads == 1) {
			return mapLinks(out);
		}
		int count = 0;
		MapLinkThread[] linkers = new MapLinkThread[threads];
		Thread[] runners = new Thread[threads];
		SetLink sout = new SynchronizingSetLink(out);
		Iterator<Polygon> pi = polys.iterator();
		for (int i = 0; i < threads; ++i) {
			linkers[i] = new MapLinkThread(pi, sout, this);
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
	
	/**
	 * Simple thread to process all polygons against each other for mapLinks()
	 * @author bolson
	 * @see #mapLinks(SetLink,int)
	 */
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
	
	/**
	 * Add polygon to all sub-buckets it could possibly touch.
	 * @param p
	 */
	void add(Polygon p) {
		polys.add(p);
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
	
	static java.util.logging.Logger log = java.util.logging.Logger.getLogger("org.bolson.redistricter");
}