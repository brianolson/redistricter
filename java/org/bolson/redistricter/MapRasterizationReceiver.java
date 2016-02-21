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
	TreeMap<TemporaryBlockHolder, TemporaryBlockHolder> blocks = null;
	
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
			blocks = new TreeMap<TemporaryBlockHolder, TemporaryBlockHolder>();
		}
	}
	// @Override // one of my Java 1.5 doesn't like this
	public void setRasterizedPolygon(RasterizationContext ctx, Polygon p) {
		if (p.blockid != null) {
			ShapefileBundle.log.log(Level.FINE, "blockid {0}", new String(p.blockid));
			if (blocks != null) {
				// Save for later.
				TemporaryBlockHolder key = new TemporaryBlockHolder(p);
				TemporaryBlockHolder tbh = blocks.get(key);
				if (tbh == null) {
					key.setRast(ctx, p);
					blocks.put(key, key);
				} else {
					tbh.add(ctx, p);
				}
				return;
			}
			Redata.MapRasterization.Block.Builder bb = Redata.MapRasterization.Block.newBuilder();
			long ubid = ShapefileBundle.blockidToUbid(p.blockid);
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
			while (!blocks.isEmpty()) {
				TemporaryBlockHolder tb = blocks.firstKey();
				assert tb != null;
				rastb.addBlock(tb.build());
				// To make a temporary free-able, remove it from the map.
				blocks.remove(tb);
			}
		}
		return rastb.build();
	}
}