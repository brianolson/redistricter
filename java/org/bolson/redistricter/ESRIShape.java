package org.bolson.redistricter;

import org.bolson.redistricter.ShapefileBundle.RasterizationContext;

/**
 * tagging type
 * @author bolson
 *
 */
public abstract class ESRIShape {
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
	static final boolean intersect( double x1, double y1, double x2, double y2, double y, RasterizationContext ctx) {
		if ( y1 < y2 ) {
			if ( (y < y1) || (y > y2) ) {
				return false;
			}
	    } else if ( y1 > y2 ) {
	    	if ( (y > y1) || (y < y2) ) {
	    		return false;
	    	}
	    } else {
	    	if ( y != y1 ) {
	    		return false;
	    	}
	    }
	    double x = (x1-x2) * ((y-y2) / (y1-y2)) + x2;
	    if ( x1 < x2 ) {
	    	if ( (x < x1) || (x > x2) ) {
	    		return false;
	    	}
	    } else if ( x1 > x2 ) {
	    	if ( (x > x1) || (x < x2) ) {
	    		return false;
	    	}
	    } else {
	    	if ( x != x1 ) {
	    		return false;
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
				return false;
	    	} else {
	    		break;
	    	}
	    }
	    ctx.xIntersectScratch[i] = x;
	    ctx.xIntersects++;
	    return true;
	}

	public static final void bresenham(double x0, double y0, double x1, double y1, RasterizationContext ctx) {
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
	public static final void bresenham(int x0, int y0, int x1, int y1, RasterizationContext ctx) {
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
}
