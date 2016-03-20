package org.bolson.redistricter;

import org.osgeo.proj4j.CRSFactory;
import org.osgeo.proj4j.CoordinateReferenceSystem;
import org.osgeo.proj4j.ProjCoordinate;
import org.osgeo.proj4j.Registry;
import org.osgeo.proj4j.datum.Datum;
import org.osgeo.proj4j.proj.Projection;

public class Proj {
public static void main(String argv[]) {
	for (Datum d : Registry.datums) {
		System.out.println(d.getCode() + "," + d.getName());
	}
	
	CRSFactory crsf = new CRSFactory();
	CoordinateReferenceSystem crs = crsf.createFromName("NAD83:2001"); // Massachusetts
	Projection proj = crs.getProjection();
	ProjCoordinate loc = new ProjCoordinate(40.0, 78.0);
	ProjCoordinate out = new ProjCoordinate();
	out = proj.project(loc, out);
	System.out.println(loc + " -> " + out);
}

protected static CRSFactory crsf;
protected static CRSFactory getFactory() {
	if (crsf == null) {
		synchronized(Proj.class) {
			if (crsf == null) {
				crsf = new CRSFactory();
			}
		}
	}
	return crsf;
}

protected CoordinateReferenceSystem crs;
protected Projection proj;
protected ProjCoordinate loc, out;
public Proj(String projectionName) {
	crs = getFactory().createFromName(projectionName);
	proj = crs.getProjection();
	loc = new ProjCoordinate();
	out = new ProjCoordinate();
}

public ProjCoordinate project(double lon, double lat) {
	loc.x = lon;
	loc.y = lat;
	out = proj.project(loc,  out);
	return out;
}
}
