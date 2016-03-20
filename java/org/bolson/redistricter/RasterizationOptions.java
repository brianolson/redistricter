package org.bolson.redistricter;

import java.io.IOException;
import java.util.ArrayList;

public class RasterizationOptions {
	public double minx = Double.NaN;
	public double miny = Double.NaN;
	public double maxx = Double.NaN;
	public double maxy = Double.NaN;
	public boolean minxSet = false;
	public boolean minySet = false;
	public boolean maxxSet = false;
	public boolean maxySet = false;
	public int xpx = -1;
	public int ypx = -1;
	String maskOutName = null;
	String rastOut = null;
	boolean outline = false;
	boolean optimizePb = true;
	private Proj projection;

	public String[] parseOpts(String[] argv) {
		ArrayList<String> out = new ArrayList<String>();
		for (int i = 0; i < argv.length; ++i) {
			if (argv[i].equals("--xpx")) {
				i++;
				xpx = Integer.parseInt(argv[i]);
			} else if (argv[i].equals("--ypx")) {
				i++;
				ypx = Integer.parseInt(argv[i]);
			} else if (argv[i].equals("--minx")) {
				i++;
				minx = Double.parseDouble(argv[i]);
				minxSet = true;
			} else if (argv[i].equals("--maxx")) {
				i++;
				maxx = Double.parseDouble(argv[i]);
				maxxSet = true;
			} else if (argv[i].equals("--miny")) {
				i++;
				miny = Double.parseDouble(argv[i]);
				minySet = true;
			} else if (argv[i].equals("--maxy")) {
				i++;
				maxy = Double.parseDouble(argv[i]);
				maxySet = true;
			} else if (argv[i].equals("--rast")) {
				i++;
				rastOut = argv[i];
			} else if (argv[i].equals("--mask")) {
				i++;
				maskOutName = argv[i];
			} else if (argv[i].equals("--outline")) {
				outline = true;
			} else if (argv[i].equals("--simple-rast")) {
				optimizePb = false;
			} else {
				out.add(argv[i]);
			}
		}
		return out.toArray(new String[out.size()]);
	}

	public String[] getOpts() {
		ArrayList<String> out = new ArrayList<String>();
		if (xpx != -1) {
			out.add("--xpx");
			out.add(Integer.toString(xpx));
		}
		if (ypx != -1) {
			out.add("--ypx");
			out.add(Integer.toString(ypx));
		}
		if (!Double.isNaN(minx)) {
			out.add("--minx");
			out.add(Double.toString(minx));
		}
		if (!Double.isNaN(miny)) {
			out.add("--miny");
			out.add(Double.toString(miny));
		}
		if (!Double.isNaN(maxx)) {
			out.add("--maxx");
			out.add(Double.toString(maxx));
		}
		if (!Double.isNaN(maxy)) {
			out.add("--maxy");
			out.add(Double.toString(maxy));
		}
		/*
		 * TODO: this is somewhat irregular. Right now I actually only care
		 * about the geometry parameters applied, but the design doesn't signify
		 * that. if (rastOut != null) { out.add("--rast"); out.add(rastOut); }
		 * if (maskOutName != null) { out.add("--mask"); out.add(maskOutName); }
		 * if (outline) { out.add("--outline"); } if (!optimizePb) {
		 * out.add("--simple-rast"); }
		 */
		return out.toArray(new String[out.size()]);
	}

	/**
	 * Doesn't do anything smart about shell escaping. Paths with spaces will
	 * break it.
	 * 
	 * @return
	 */
	public String getOptString(String sep) {
		String[] argv = getOpts();
		if (argv == null || argv.length == 0) {
			return "";
		}
		StringBuilder sb = new StringBuilder(argv[0]);
		for (int i = 1; i < argv.length; ++i) {
			sb.append(sep);
			sb.append(argv[i]);
		}
		return sb.toString();
	}

	public String toString() {
		return "RasterizationOptions(" + minx + "<x<" + maxx + ", " + miny
				+ "<y<" + maxy + ", px=(" + xpx + "," + ypx + "))";
	}

	/**
	 * Bound will become the greater of this and shp.
	 * 
	 * @param shp
	 * @throws IOException
	 */
	public void increaseBoundsFromShapefile(Shapefile shp) throws IOException {
		Shapefile.Header header = shp.getHeader();
		if (Double.isNaN(minx) || ((minx > header.xmin) && !minxSet)) {
			minx = header.xmin;
		}
		if (Double.isNaN(miny) || ((miny > header.ymin) && !minySet)) {
			miny = header.ymin;
		}
		if (Double.isNaN(maxx) || ((maxx < header.xmax) && !maxxSet)) {
			maxx = header.xmax;
		}
		if (Double.isNaN(maxy) || ((maxy < header.ymax) && !maxySet)) {
			maxy = header.ymax;
		}
	}

	/*
	 * @Deprecated public void setBoundsFromShapefile(Shapefile shp, boolean
	 * override) throws IOException { Shapefile.Header header = shp.getHeader();
	 * if (override || Double.isNaN(minx)) { minx = header.xmin; } if (override
	 * || Double.isNaN(miny)) { miny = header.ymin; } if (override ||
	 * Double.isNaN(maxx)) { maxx = header.xmax; } if (override ||
	 * Double.isNaN(maxy)) { maxy = header.ymax; } }
	 */

	/**
	 * Set xpx,ypx, up to boundx,boundy.
	 * 
	 * @param boundx
	 * @param boundy
	 */
	public void updatePixelSize(int boundx, int boundy) {
		if ((xpx != -1) && (ypx != -1)) {
			return;
		}
		double width = maxx - minx;
		assert (width > 0.0);
		double height = maxy - miny;
		assert (height > 0.0);
		double w2;
		if (projection == null) {
			// do lame projection according to cos of latitude
			w2 = width * Math.cos(Math.abs((maxy + miny) / 2.0) * Math.PI / 180.0);
		} else {
			// projection has already been done, go with it
			w2 = width;
		}
		double ratio = height / w2;
		double boundRatio = (boundy * 1.0) / boundx;
		if (ratio > boundRatio) {
			// state is too tall
			ypx = boundy;
			xpx = (int) (ypx / ratio);
			assert xpx > 0;
			assert ypx > 0;
		} else {
			xpx = boundx;
			ypx = (int) (ratio * xpx);
			assert xpx > 0;
			assert ypx > 0;
		}
	}

	public void setProjection(Proj projection) {
		this.projection = projection;
	}
}