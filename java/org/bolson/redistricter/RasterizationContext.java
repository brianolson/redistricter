package org.bolson.redistricter;


class RasterizationContext {
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
	
	/*
	 * 
	 * @param shp
	 * @param px
	 * @param py
	 * @deprecated
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
	*/
	
	
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