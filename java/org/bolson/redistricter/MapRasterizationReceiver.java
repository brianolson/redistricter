package org.bolson.redistricter;

import java.util.TreeMap;
import java.util.logging.Level;

import com.google.protobuf.ByteString;

/**
 * Write pixels to a MapRasterization protobuf.
 * @author bolson
 * @see Redata.MapRasterization
 */
public class MapRasterizationReceiver implements RasterizationReciever {
	protected Redata.MapRasterization.Builder rastb = Redata.MapRasterization.newBuilder();
	TreeMap<Long, TemporaryBlockHolder> blocks = null;
	int pxCount = 0;
	
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
			blocks = new TreeMap<Long, TemporaryBlockHolder>();
		}
	}
	// @Override // one of my Java 1.5 doesn't like this
	public void setRasterizedPolygon(RasterizationContext ctx, Polygon p) {
		if (p.blockid != null) {
			long ubid = ShapefileBundle.blockidToUbid(p.blockid);
			
			ShapefileBundle.log.log(Level.FINE, "blockid {0}", new String(p.blockid));
			if (blocks != null) {
				// Save for later.
				pxCount += ctx.pxPos / 2;
				TemporaryBlockHolder tbh = blocks.get(ubid);
				if (tbh == null) {
					tbh = new TemporaryBlockHolder(ctx, p);
					blocks.put(ubid, tbh);
				} else {
					tbh.add(ctx, p);
				}
				return;
			}
			Redata.MapRasterization.Block.Builder bb = Redata.MapRasterization.Block.newBuilder();
			
			if (ubid >= 0) {
				bb.setUbid(ubid);
			} else {
				bb.setBlockid(ByteString.copyFrom(p.blockid));
			}
			// validate the data about to go out
			/*
			for (int i = 2; i < ctx.pxPos; i += 2) {
				if ((ctx.pixels[i-2] == ctx.pixels[i]) && (ctx.pixels[i-1] == ctx.pixels[i+1])) {
					ShapefileBundle.log.warning("dup pixels in block " + ubid + " " + p.blockid + " (" + ctx.pixels[i-2] + ", " + ctx.pixels[i-1] + ") == (" + ctx.pixels[i] + ", " + ctx.pixels[i+1] + ")");
				}
			}
			*/
			if (p.isWater) {
				for (int i = 0; i < ctx.pxPos; ++i) {
					bb.addWaterxy(ctx.pixels[i]);
				}
			} else {
				for (int i = 0; i < ctx.pxPos; ++i) {
					bb.addXy(ctx.pixels[i]);
				}
				pxCount += ctx.pxPos / 2;
				// validate the outgoing data just set
				/*
				for (int i = 2; i < bb.getXyCount(); i += 2) {
					int xa = bb.getXy(i-2);
					int xb = bb.getXy(i);
					int ya = bb.getXy(i-1);
					int yb = bb.getXy(i+1);
					if ((xa == xb) && (ya == yb)) {
						ShapefileBundle.log.warning("dup pixels in block " + ubid + " " + p.blockid + " (" + xa + ", " + ya + ") == (" + xb + ", " + yb + ")");
					}
				}
				*/
			}
			rastb.addBlock(bb);
			bb = null;
		} else {
			ShapefileBundle.log.warning("polygon with no blockid");
		}
	}

	// @Override  // one of my Java 1.5 doesn't like this
	public void setSize(int x, int y) {
		rastb.setSizex(x);
		rastb.setSizey(y);
	}
	
	public Redata.MapRasterization getMapRasterization() {
		if (blocks != null) {
			int outPxCount = 0;
			for (TemporaryBlockHolder tb : blocks.values()) {
				outPxCount += tb.numLandPoints();
				rastb.addBlock(tb.build());
			}
			ShapefileBundle.log.info(pxCount + " land pixels to in, " + outPxCount + " out to mppb");
		} else {
			ShapefileBundle.log.info(pxCount + " land pixels to mppb");
		}
		return rastb.build();
	}
}
