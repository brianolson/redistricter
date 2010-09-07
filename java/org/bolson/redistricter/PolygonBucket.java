package org.bolson.redistricter;

import java.io.IOException;
import java.io.OutputStream;
import java.io.Writer;
import java.util.ArrayList;

import org.bolson.redistricter.ShapefileBundle.SetLink;

class PolygonBucket {
	public double minx, maxx, miny, maxy;
	public ArrayList<Polygon> polys = new ArrayList<Polygon>();
	PolygonBucket(double minx, double maxx, double miny, double maxy) {
		this.minx = minx;
		this.maxx = maxx;
		this.miny = miny;
		this.maxy = maxy;
	}
	public boolean contains(double x, double y) {
		return ((x >= minx) && (x <= maxx) && (y >= miny) && (y <= maxy));
	}
	public boolean add(Polygon p) {
		polys.add(p);
		/*
		if (contains(p.xmin, p.ymin) || contains(p.xmin, p.ymax) || contains(p.xmax,p.ymin) || contains(p.xmax, p.ymax)) {
			polys.add(p);
			return true;
		}
		return false;
		*/
		return true;
	}
	
	@Deprecated
	public int writeLinks(Writer out, Polygon b) throws IOException {
		int count = 0;
		for (Polygon a : polys) {
			if (a == b) {
				continue;
			}
			if (a.hasTwoPointsInCommon(b)) {
				ShapefileBundle.writeLink(out, a, b);
				count++;
			}
		}
		return count;
	}
	
	@Deprecated
	public int writeLinks(OutputStream out, Polygon b) throws IOException {
		int count = 0;
		for (Polygon a : polys) {
			if (a == b) {
				continue;
			}
			if (a.hasTwoPointsInCommon(b)) {
				ShapefileBundle.writeLink(out, a, b);
				count++;
			}
		}
		return count;
	}
	public int mapLinks(SetLink out, Polygon b) {
		int count = 0;
		for (Polygon a : polys) {
			if (a == b) {
				continue;
			}
			if (a.hasTwoPointsInCommon(b)) {
				if (out.setLink(a.blockid, b.blockid)) {
					count++;
				}
			}
		}
		return count;
	}
}