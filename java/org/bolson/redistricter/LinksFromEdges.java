package org.bolson.redistricter;

import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.logging.Level;

import org.bolson.redistricter.ShapefileBundle.BlockIdComparator;
import org.bolson.redistricter.ShapefileBundle.CompositeDBaseField;
import org.bolson.redistricter.ShapefileBundle.MapSetLinkWrapper;

public class LinksFromEdges {
	static java.util.logging.Logger log = java.util.logging.Logger.getLogger("org.bolson.redistricter");
	
	TreeMap<Long, byte[]> tfidUbid = new TreeMap<Long, byte[]>();
	Map<byte[], Set<byte[]> > links;
	ShapefileBundle.SetLink linkSetter;
	
	LinksFromEdges() {
		boolean tree = true;
		if (tree) {
			links = new TreeMap<byte[], Set<byte[]> >(new BlockIdComparator());
		} else {
			links = new HashMap<byte[], Set<byte[]> >();
		}
		linkSetter = new MapSetLinkWrapper(links);
	}
	
	/**
	 * Gather tfid:ubid mappings.
	 * @param path shapefile bundle .zip file to process.
	 * @return number of faces processed
	 * @throws IOException
	 */
	int readFaces(String path) throws IOException {
		ShapefileBundle sb = new ShapefileBundle();
		sb.open(path);
		return readFaces(sb, true);
	}
	
	/**
	 * Gather tfid:ubid mappings.
	 * @param sb data to process
	 * @param y2kmode 2000 Census mode
	 * @return number of faces processed
	 * @throws IOException
	 */
	int readFaces(ShapefileBundle sb, boolean y2kmode) throws IOException {
		DBaseFieldDescriptor blockIdField = null;
		// synthesize blockId from parts of faces file
		DBaseFieldDescriptor state = sb.dbf.getField("STATEFP00");
		DBaseFieldDescriptor county = sb.dbf.getField("COUNTYFP00");
		DBaseFieldDescriptor tract = sb.dbf.getField("TRACTCE00");
		DBaseFieldDescriptor block = sb.dbf.getField("BLOCKCE00");
		DBaseFieldDescriptor suffix = sb.dbf.getField("SUFFIX1CE");
		if ((state != null) && (county != null) && (tract != null) && (block != null) && (y2kmode || (suffix != null))) {
			CompositeDBaseField cfield = new CompositeDBaseField();
			cfield.add(state);
			cfield.add(county);
			cfield.add(tract);
			cfield.add(block);
			if (!y2kmode) {
				cfield.add(suffix);
			}
			blockIdField = cfield;
		}
		assert blockIdField != null;
		DBaseFieldDescriptor tfid = sb.dbf.getField("TFID");
		assert tfid != null;

		int count = 0;
		Polygon pl = (Polygon)sb.shp.next();
		while (pl != null) {
			count++;
			byte[] rowbytes = sb.dbf.next();
			
			byte[] blockid = blockIdField.getBytes(rowbytes, 0, rowbytes.length);
			long id = tfid.getLong(rowbytes, 0, rowbytes.length);
			//long ubid = ShapefileBundle.blockidToUbid(ubidBuffer);
			tfidUbid.put(id, blockid);
			pl = (Polygon)sb.shp.next();
		}
		
		return count;
	}
	
	int readEdges(String path) throws IOException {
		ShapefileBundle sb = new ShapefileBundle();
		sb.open(path);
		return readEdges(sb, true);
	}

	int readEdges(ShapefileBundle sb, boolean b) throws IOException {
		DBaseFieldDescriptor tfidl = sb.dbf.getField("TFIDL");
		DBaseFieldDescriptor tfidr = sb.dbf.getField("TFIDR");
		int count = 0;
		int errorCount = 0;
		PolyLine pl = (PolyLine)sb.shp.next();
		while (pl != null) {
			count++;
			byte[] rowbytes = sb.dbf.next();

			try {
				long idl = tfidl.getLong(rowbytes);
				long idr = tfidr.getLong(rowbytes);
				byte[] ubidl = tfidUbid.get(idl);
				byte[] ubidr = tfidUbid.get(idr);
				if ((ubidl != null) && (ubidr != null)) {
					linkSetter.setLink(ubidl, ubidr);
				}
			} catch (NumberFormatException e) {
				errorCount++;
			}
			
			pl = (PolyLine)sb.shp.next();
		}
		if (errorCount > 0) {
			log.warning("error entries: " + errorCount);
		}
		return count;
	}

	/**
	 * @param args
	 * @throws IOException 
	 */
	public static void main(String[] argv) throws IOException {
		long totalStart = System.currentTimeMillis();
		ShapefileBundle.loggingInit();
		ArrayList<String> inputPaths = new ArrayList<String>();
		String linksOut = null;
		
		for (int i = 0; i < argv.length; ++i) {
			if (argv[i].endsWith(".zip")) {
				inputPaths.add(argv[i]);
			} else if (argv[i].equals("--links")) {
				i++;
				linksOut = argv[i];
			} else if (argv[i].equals("--verbose")) {
				log.setLevel(Level.FINEST);
			} else {
				System.err.println("bogus arg: " + argv[i]);
				//System.err.print(usage);
				System.exit(1);
				return;
			}
		}
		LinksFromEdges it = new LinksFromEdges();
		long start = System.currentTimeMillis();
		int faceCount = 0;
		int edgeCount = 0;
		for (String path : inputPaths) {
			if (path.contains("faces")) {
				log.info(path);
				faceCount += it.readFaces(path);
			}
		}
		long end = System.currentTimeMillis();
		log.info("read " + faceCount + " faces in " + ((end - start) / 1000.0) + " seconds");
		start = end;
		for (String path : inputPaths) {
			if (path.contains("edges")) {
				log.info(path);
				edgeCount += it.readEdges(path);
			}
		}
		end = System.currentTimeMillis();
		log.info("processed " + edgeCount + " edges in " + ((end - start) / 1000.0) + " seconds");
		start = end;
		OutputStream out = new FileOutputStream(linksOut);
		int linkCount = 0;
		for (byte[] key : it.links.keySet()) {
			for (byte[] value : it.links.get(key)) {
				linkCount++;
				out.write(key);
				out.write(',');
				out.write(value);
				out.write('\n');
			}
		}
		out.close();
		end = System.currentTimeMillis();
		log.info("wrote " + linkCount + " links in " + ((end - start) / 1000.0) + " seconds");
		long totalEnd = System.currentTimeMillis();
		log.info("total time: " + ((totalEnd - totalStart) / 1000.0) + " seconds");
	}
}
