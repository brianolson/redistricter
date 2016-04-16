package org.bolson.redistricter;

import com.google.protobuf.ByteString;

/**
 * Efficient in-memory storage of MapRasterization.Block
 * @author bolson
 *
 */
public class TemporaryBlockHolder implements Comparable<TemporaryBlockHolder> {
	long ubid;
	byte[] blockid;
	int[] xy;
	int[] water;

	/** Header only, for hashCode lookup */
	TemporaryBlockHolder(Polygon p) {
		ubid = ShapefileBundle.blockidToUbid(p.blockid);
		if (ubid < 0) {
			blockid = p.blockid.clone();
		}
	}
	TemporaryBlockHolder(RasterizationContext ctx, Polygon p) {
		ubid = ShapefileBundle.blockidToUbid(p.blockid);
		if (ubid < 0) {
			blockid = p.blockid.clone();
		}
		setRast(ctx, p);
	}
	public int numLandPoints() {
		if (xy == null) {
			return 0;
		}
		return xy.length / 2;
	}
	public void setRast(RasterizationContext ctx, Polygon p) {
		if (p.isWater) {
			water = new int[ctx.pxPos];
			System.arraycopy(ctx.pixels, 0, water, 0, ctx.pxPos);
		} else {
			xy = new int[ctx.pxPos];
			System.arraycopy(ctx.pixels, 0, xy, 0, ctx.pxPos);
		}
	}
	
	public static int[] growIntArray(int[] orig, int[] src, int morelen) {
		if (orig == null) {
			int[] out = new int[morelen];
			System.arraycopy(src, 0, out, 0, morelen);
			return out;
		}
		int[] nxy = new int[orig.length + morelen];
		System.arraycopy(orig, 0, nxy, 0, orig.length);
		System.arraycopy(src, 0, nxy, orig.length, morelen);
		return nxy;
	}
	
	public void add(RasterizationContext ctx, Polygon p) {
		long tubid = ShapefileBundle.blockidToUbid(p.blockid);
		assert tubid == ubid;
		if (ubid < 0) {
			assert java.util.Arrays.equals(blockid, p.blockid);
		}
		if (p.isWater) {
			water = growIntArray(water, ctx.pixels, ctx.pxPos);
		} else {
			xy = growIntArray(xy, ctx.pixels, ctx.pxPos);
		}
	}
	
	public int hashCode() {
		if (ubid >= 0) {
			return (int)ubid;
		}
		return blockid.hashCode();
	}
	
	public boolean equals(Object o) {
		if (ubid >= 0) {
			return ubid == ((TemporaryBlockHolder)o).ubid;
		}
		return java.util.Arrays.equals(blockid, ((TemporaryBlockHolder)o).blockid);
	}
	
	public Redata.MapRasterization.Block.Builder build() {
		Redata.MapRasterization.Block.Builder bb = Redata.MapRasterization.Block.newBuilder();
		if (ubid >= 0) {
			bb.setUbid(ubid);
		} else {
			bb.setBlockid(ByteString.copyFrom(blockid));
		}
		if (water != null) {
			for (int i = 0; i < water.length; ++i) {
				bb.addWaterxy(water[i]);
			}
		}
		if (xy != null) {
			for (int i = 0; i < xy.length; ++i) {
				bb.addXy(xy[i]);
			}
		}
		return bb;
	}
	@Override
	public int compareTo(TemporaryBlockHolder o) {
		if (ubid >= 0) {
			if (o.ubid >= 0) {
				if (ubid < o.ubid) {
					return -1;
				}
				if (ubid > o.ubid) {
					return 1;
				}
				return 0;
			} else {
				throw new ClassCastException();
			}
		}
		if (o.ubid >= 0) {
			throw new ClassCastException();
		}
		for (int i = 0; i < blockid.length && i < o.blockid.length; ++i) {
			if (blockid[i] < o.blockid[i]) {
				return -1;
			}
			if (blockid[i] > o.blockid[i]) {
				return 1;
			}
		}
		if (blockid.length < o.blockid.length) {
			return -1;
		}
		if (blockid.length > o.blockid.length) {
			return 1;
		}
		return 0;
	}
}
