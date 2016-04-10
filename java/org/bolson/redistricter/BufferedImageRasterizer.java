package org.bolson.redistricter;

import java.awt.Font;
import java.awt.Graphics2D;
import java.awt.geom.Rectangle2D;
import java.awt.image.BufferedImage;

/**
 * Write pixels to a java.awt.image.BufferedImage
 * @author bolson
 * @see java.awt.image.BufferedImage
 */
public class BufferedImageRasterizer implements RasterizationReciever {
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
	public BufferedImageRasterizer.Options opts = null;
	
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
	
	int collisionCount = 0;
	
	/**
	 * @param imageOut where to render to
	 * @param optsIn may be null
	 */
	BufferedImageRasterizer(BufferedImage imageOut, BufferedImageRasterizer.Options optsIn) {
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
			int oldcolor = mask.getRGB(ctx.pixels[i], ctx.pixels[i+1]);
			if (oldcolor != 0) {
				collisionCount++;
				//ShapefileBundle.log.warning("collinging pixel: " + ctx.pixels[i] +","+ ctx.pixels[i+1]);
			}
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
			String polyName = ShapefileBundle.blockidToString(p.blockid);
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