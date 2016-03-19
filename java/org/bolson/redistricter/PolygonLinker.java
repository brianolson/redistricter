package org.bolson.redistricter;

import java.io.IOException;

public class PolygonLinker implements PolygonProcessor {
	PolygonBucketArray pba = null;
	String linksOut = null;
	boolean tree;
	int threads;

	PolygonLinker(Shapefile shp, String linksOut, boolean tree, int threads, int awidth, int aheight) throws IOException {
		pba = new PolygonBucketArray(shp, awidth, aheight);
		this.linksOut = linksOut;
		this.tree = tree;
		this.threads = threads;
	}
	PolygonLinker(Shapefile shp, String linksOut, boolean tree, int threads) throws IOException {
		pba = new PolygonBucketArray(shp, 20, 20);
		this.linksOut = linksOut;
		this.tree = tree;
		this.threads = threads;
	}
	
	void growForShapefile(Shapefile shp) throws IOException {
		pba.growBoundsForShapefile(shp);
	}
	
	@Override
	public void process(Polygon p) {
		pba.add(p);
	}
	
	public void finish() throws IOException {
		pba.doLinks(linksOut, tree, threads);
	}

	@Override
	public String name() {
		return "linking";
	}
}