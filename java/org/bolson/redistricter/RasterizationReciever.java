package org.bolson.redistricter;


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