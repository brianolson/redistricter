package org.bolson.redistricter;

import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.Reader;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;
import java.util.zip.GZIPInputStream;

import com.vividsolutions.jts.geom.Coordinate;
import com.vividsolutions.jts.geom.Geometry;
import com.vividsolutions.jts.geom.GeometryFactory;
import com.vividsolutions.jts.geom.LinearRing;
import com.vividsolutions.jts.geom.impl.CoordinateArraySequence;

/**
 * merge polygons for districts into large polygons that can have a border drawn in KML or ESRI shape
 * @author bolson
 *
 */
public class DistrictMapOutline implements PolygonProcessor {
	static java.util.logging.Logger log = java.util.logging.Logger.getLogger("org.bolson.redistricter");
	
	DistrictMapOutline(String districtCsv, String outPath) {
		this.districtCsv = districtCsv;
		this.outPath = outPath;
	}
	String outPath;
	String districtCsv;
	String stateDataPath = null;

	Map<String, Integer> blockToDistrict = null;
	ArrayList<Geometry> dgeos = new ArrayList<Geometry>();
	
	/**
	 * Convert internal polygon to JTS Geometry
	 * @param p
	 * @return
	 */
	public static Geometry convert(Polygon p) {
		GeometryFactory grf = new GeometryFactory();
		Geometry out = null;
		
		for (int parti = 0; parti < p.parts.length; ++parti) {
			int partstart = p.parts[parti];
			int partend;
			if (parti + 1 < p.parts.length) {
				partend = p.parts[parti+1] - 1;
			} else {
				partend = (p.points.length / 2) - 1;
			}
			Coordinate[] coords = new Coordinate[partend - partstart + 1];
			int ci = 0;
			for (int pointi = partstart; pointi <= partend; ++pointi) {
				coords[ci] = new Coordinate(p.points[pointi*2], p.points[pointi*2 + 1]);
				ci++;
			}
			LinearRing tlr = grf.createLinearRing(coords);
			if (out == null) {
				out = tlr;
			} else {
				if (out.covers(tlr)) {
					out = out.difference(tlr);
				} else if (tlr.covers(out)){
					out = tlr.difference(out);
				} else {
					out = out.union(tlr);
				}
			}
		}
		return out;
	}
	
	public static Map<String, Integer> readDistrictCsv(String path) {
		HashMap<String, Integer> out = new HashMap<String, Integer>();
		try {
			FileInputStream fin = new FileInputStream(path);
			GZIPInputStream gzin = null;
			Reader fr = null;
			if (path.endsWith(".gz")) {
				gzin = new GZIPInputStream(fin);
				fr = new InputStreamReader(gzin);
			} else {
				fr = new InputStreamReader(fin);
			}
			BufferedReader bin = new BufferedReader(fr);
			String line = null;
			while (true) {
				line = bin.readLine();
				if (line == null) {
					break;
				}
				String[] parts = line.split(",", 2);
				out.put(parts[0], Integer.parseInt(parts[1]));
			}
			bin.close();
			fr.close();
			if (gzin != null) {
				gzin.close();
			}
			fin.close();
		} catch (FileNotFoundException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
			return null;
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		return out;
	}
	
	Integer districtForBlock(byte[] id) {
		if (blockToDistrict == null) {
			synchronized (this) {
				if (blockToDistrict == null) {
					blockToDistrict = readDistrictCsv(districtCsv);
					log.info("loaded district csv: " + blockToDistrict.size());
				}
			}
		}
		return blockToDistrict.get(new String(id));
	}
	
	int numPolysNoDistrict = 0;
	
	@Override
	public void process(Polygon p) {
		// TODO Auto-generated method stub
		Integer districtNumber = districtForBlock(p.blockid);
		if (districtNumber == null) {
			numPolysNoDistrict++;
			return;
		}
		while (dgeos.size() <= districtNumber) {
			dgeos.add(null);
		}
		Geometry old = dgeos.get(districtNumber);
		if (old == null) {
			old = convert(p);
		} else {
			old = old.union(convert(p));
		}
		dgeos.set(districtNumber, old);
	}

	@Override
	public void finish() throws Exception {
		// TODO Auto-generated method stub
		FileWriter out = new FileWriter(outPath);
		log.info("polys no district: " + numPolysNoDistrict);
		log.info("num geoms: " + dgeos.size());
		for (Geometry geom : dgeos) {
			out.write(geom.toText());
		}
		out.close();
	}

	@Override
	public String name() {
		return "map outline";
	}

}
