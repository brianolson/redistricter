package org.bolson.redistricter;

import java.awt.image.BufferedImage;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.zip.GZIPOutputStream;

import org.bolson.redistricter.ShapefileBundle.PolygonDrawEdges;
import org.bolson.redistricter.ShapefileBundle.PolygonDrawMode;
import org.bolson.redistricter.ShapefileBundle.PolygonFillRasterize;

public class PolygonRasterizer implements PolygonProcessor {
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
	BufferedImageRasterizer bir = null;
	
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
			ShapefileBundle.log.info("will make mask \"" + rastOpts.maskOutName + "\"");
			ShapefileBundle.log.info("x=" + rastOpts.xpx + " y=" + rastOpts.ypx);
			maskImage = new BufferedImage(rastOpts.xpx, rastOpts.ypx, BufferedImage.TYPE_4BYTE_ABGR);
			bir = new BufferedImageRasterizer(maskImage, birOpts);
			outputs.add(bir);
		}
		
		if (rastOpts.rastOut != null) {
			ShapefileBundle.log.info("will make rast data \"" + rastOpts.rastOut + "\"");
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
		if (bir != null) {
			if (bir.collisionCount > 0) {
				ShapefileBundle.log.warning("pixel collisions: " + bir.collisionCount);
			}
			ShapefileBundle.log.info("land pixels to mask image: " + bir.pxCount);
		}
		if ((maskOutput == null) && (rastOpts.maskOutName != null)) {
			maskOutput = new FileOutputStream(rastOpts.maskOutName);
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
