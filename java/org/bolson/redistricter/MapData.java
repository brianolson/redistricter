package org.bolson.redistricter;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.zip.GZIPInputStream;
import java.util.zip.InflaterInputStream;

import org.bolson.redistricter.Redata.RedistricterData;

/**
 * More efficient in-memory version of Redata.RedistricterData
 * @author bolson
 */
public class MapData {
	/**
	 * lat,lon pairs in microdegrees
	 */
	public int[] intpoints;
	/**
	 * lat,lon pairs in degrees
	 */
	public double[] fpoints;
	/**
	 * population per block
	 */
	public int[] population;
	/**
	 * square meters
	 */
	public int[] area;
	/**
	 * my internal number
	 */
	public long[] ubids;
	/**
	 * LOGRECNO from census file
	 */
	public int[] recnos;
	/**
	 * index,index pairs
	 */
	public int[] edges;
	
	public int numDistricts = -1;
	
	static class OtherData {
		String name;
		int[] data;
	}
	ArrayList<OtherData> other = new ArrayList<OtherData>();
	
	protected long[] sortedUbids = null;
	protected int[] sortedUbidIndecies = null;
	
	public void buildSortedUbids() {
		sortedUbids = new long[ubids.length];
		sortedUbidIndecies = new int[ubids.length];
		for (int i = 0; i < ubids.length; ++i) {
			sortedUbids[i] = ubids[i];
			sortedUbidIndecies[i] = i;
		}
		boolean notDone = true;
		while (notDone) {
			notDone = false;
			for (int i = 1; i < sortedUbids.length; ++i) {
				if (sortedUbids[i] < sortedUbids[i-1]) {
					notDone = true;
					long tu = sortedUbids[i];
					sortedUbids[i] = sortedUbids[i-1];
					sortedUbids[i - 1] = tu;
					int ti = sortedUbidIndecies[i];
					sortedUbidIndecies[i] = sortedUbidIndecies[i-1];
					sortedUbidIndecies[i-1] = ti;
				}
			}
		}
	}
	
	public int indexForUbid(long ubid) {
		if (sortedUbids == null) {
			buildSortedUbids();
		}
		int ai = Arrays.binarySearch(sortedUbids, ubid);
		if (ai < 0) {
			return -1;
		}
		return sortedUbidIndecies[ai];
	}
	
	public StringBuffer toStringBuffer(StringBuffer sb) {
		sb.append("MapData(");
		if (intpoints != null) {
			sb.append(intpoints.length / 2);
			sb.append(" intpoints,");
		}
		if (fpoints != null) {
			sb.append(fpoints.length / 2);
			sb.append(" fpoints,");
		}
		if (population != null) {
			sb.append(population.length);
			sb.append(" pop points,");
		}
		if (area != null) {
			sb.append(area.length);
			sb.append(" area points,");
		}
		if (ubids != null) {
			sb.append(ubids.length);
			sb.append(" ubids,");
		}
		if (recnos != null) {
			sb.append(recnos.length);
			sb.append(" recnos,");
		}
		if (edges != null) {
			sb.append(edges.length / 2);
			sb.append(" edges,");
		}
		sb.append(numDistricts);
		sb.append(" districts,");
		for (OtherData to : other) {
			sb.append(to.data.length);
			sb.append(' ');
			sb.append(to.name);
			sb.append(',');
		}
		sb.append(')');
		return sb;
	}
	
	public String toString() {
		return toStringBuffer(new StringBuffer()).toString();
	}
	
	public boolean copyFrom(RedistricterData it) {
		if (it.getIntpointsCount() > 0) {
			intpoints = new int[it.getIntpointsCount()];
			for (int i = 0; i < intpoints.length; ++i) {
				intpoints[i] = it.getIntpoints(i);
			}
		}
		if (it.getFpointsCount() > 0) {
			fpoints = new double[it.getFpointsCount()];
			for (int i = 0; i < fpoints.length; ++i) {
				fpoints[i] = it.getFpoints(i);
			}
		}
		if (it.getPopulationCount() > 0) {
			population = new int[it.getPopulationCount()];
			for (int i = 0; i < population.length; ++i) {
				population[i] = it.getPopulation(i);
			}
		}
		if (it.getAreaCount() > 0) {
			area = new int[it.getAreaCount()];
			for (int i = 0; i < area.length; ++i) {
				area[i] = it.getArea(i);
			}
		}
		if (it.getUbidsCount() > 0) {
			ubids = new long[it.getUbidsCount()];
			for (int i = 0; i < ubids.length; ++i) {
				ubids[i] = it.getUbids(i);
			}
		}
		if (it.getRecnosCount() > 0) {
			recnos = new int[it.getRecnosCount()];
			for (int i = 0; i < recnos.length; ++i) {
				recnos[i] = it.getRecnos(i);
			}
		}
		if (it.getEdgesCount() > 0) {
			edges = new int[it.getEdgesCount()];
			for (int i = 0; i < edges.length; ++i) {
				edges[i] = it.getEdges(i);
			}
		}
		if (it.hasNumDistricts()) {
			numDistricts = it.getNumDistricts();
		}
		other = new ArrayList<OtherData>();
		if (it.getOtherCount() > 0) {
			for (int o = 0; o < it.getOtherCount(); ++o) {
				RedistricterData.Other to = it.getOther(o);
				OtherData od = new OtherData();
				od.name = to.getName();
				od.data = new int[to.getOdataCount()];
				for (int i = 0; i < od.data.length; ++i) {
					od.data[i] = to.getOdata(i);
				}
				other.add(od);
			}
		}
		return true;
	}
	public boolean parseFrom(InputStream is) throws IOException {
		RedistricterData trd = RedistricterData.parseFrom(is);
		return copyFrom(trd);
	}
	public boolean readGZFile(File f) throws IOException {
		FileInputStream fis = new FileInputStream(f);
		InputStream gzis = new InflaterInputStream(fis);
		boolean ok = parseFrom(gzis);
		gzis.close();
		fis.close();
		return ok;
	}
	
	public static void main(String[] argv) throws Exception {
		long start = System.currentTimeMillis();
		MapData it = new MapData();
		it.readGZFile(new File(argv[0]));
		long done = System.currentTimeMillis();
		System.out.println(it.toString());
		System.out.println("in " + ((done - start) / 1000.0) + " seconds");
	}
}
