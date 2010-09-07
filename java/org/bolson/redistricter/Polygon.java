package org.bolson.redistricter;

import org.bolson.redistricter.ShapefileBundle.RasterizationContext;

/**
 * An ESRI Shapefile Polygon (type 5) object.
 * Knows how to rasterize itself.
 * @author Brian Olson
 *
 */
class Polygon {
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
		int type = ShapefileBundle.bytesToIntLE(data, pos); pos += 4;
		assert(type == 5);
		xmin = ShapefileBundle.bytesToDoubleLE(data, pos); pos += 8;
		ymin = ShapefileBundle.bytesToDoubleLE(data, pos); pos += 8;
		xmax = ShapefileBundle.bytesToDoubleLE(data, pos); pos += 8;
		ymax = ShapefileBundle.bytesToDoubleLE(data, pos); pos += 8;
		int numParts = ShapefileBundle.bytesToIntLE(data, pos); pos += 4;
		int numPoints = ShapefileBundle.bytesToIntLE(data, pos); pos += 4;
		parts = new int[numParts];
		points = new double[numPoints*2];
		for (int i = 0; i < numParts; ++i) {
			parts[i] = ShapefileBundle.bytesToIntLE(data, pos);
			pos += 4;
		}
		for (int i = 0; i < numPoints * 2; ++i) {
			points[i] = ShapefileBundle.bytesToDoubleLE(data, pos);
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
		// insert sort
	    while (i > 0) {
	    	if (x < ctx.xIntersectScratch[i-1]) {
	    		ctx.xIntersectScratch[i] = ctx.xIntersectScratch[i-1];
	    		--i;
			} else if (x == ctx.xIntersectScratch[i-1]) {
				// don't double-add a duplicate
				// TODO: epsilon of 1/2 or 1/4 pixel size?
				// TODO: if this is a point /\ or \/, drop it.
				return;
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
			if (ctx.xIntersects % 2 != 0) {
				System.err.println("mismatch in line segments intersecting y=" + y);
				System.err.println(this.toString());
				// Re-run and emit debug messages
				ctx.xIntersects = 0;
				int oldIntersects = ctx.xIntersects;
				for (int parti = 0; parti < parts.length; ++parti) {
					int partend;
					if (parti + 1 < parts.length) {
						partend = parts[parti+1] - 1;
					} else {
						partend = (points.length / 2) - 1;
					}
					for (int pointi = parts[parti]; pointi < partend; ++pointi) {
						intersect(points[pointi*2], points[pointi*2 + 1], points[pointi*2 + 2], points[pointi*2 + 3], y, ctx);
						if (ctx.xIntersects != oldIntersects) {
							System.err.print("hit: (" + points[pointi*2] + ", " + points[pointi*2 + 1] + "),(" + points[pointi*2 + 2] + ", " + points[pointi*2 + 3] + ") x = [" + ctx.xIntersectScratch[0]);
							for (int xi = 1; xi < ctx.xIntersects; ++xi) {
								System.err.print(", " + ctx.xIntersectScratch[xi]);
							}
							System.err.println("]");
							oldIntersects = ctx.xIntersects;
						}
					}
				}
				assert(ctx.xIntersects % 2 == 0);
				// if asserts are off, try to continue somewhat reasonably
				py++;
				y = pcenterY(py, ctx.maxy, ctx.pixelHeight);
				continue;
			}
			
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