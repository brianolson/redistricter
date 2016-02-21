package org.bolson.redistricter;

import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.logging.Level;

/**
 * It's like public static void main, only an object.
 * @author bolson
 *
 */
public class RunContext {
	boolean tree = true;
	int boundx = 1920;
	int boundy = 1080;
	ArrayList<String> inputPaths = new ArrayList<String>();
	String linksOut = null;
	int threads = 3;
	RasterizationOptions rastOpts = new RasterizationOptions();
	String rastOptOut = null;
	BufferedImageRasterizer.Options birOpts = new BufferedImageRasterizer.Options();
	int awidth = 20;
	int aheight = 20;
	
	String districtCsvPath = null;
	String outlineOutPath = null;
	
	String stateDataPath = null;
	
	public void main(String[] argv) throws IOException {
		parseArgv(argv);
		run();
	}

	public void parseArgv(String[] argv) throws IOException {
		argv = rastOpts.parseOpts(argv);
		for (int i = 0; i < argv.length; ++i) {
			if (argv[i].endsWith(".zip")) {
				inputPaths.add(argv[i]);
			} else if (argv[i].equals("--flagfile")) {
				i++;
				ArrayList<String> targva = ShapefileBundle.readLines(argv[i]);
				parseArgv(targva.toArray(new String[targva.size()]));
			} else if (argv[i].equals("--links")) {
				i++;
				linksOut = argv[i];
			} else if (argv[i].equals("--rastgeom")) {
				i++;
				rastOptOut = argv[i];
			} else if (argv[i].equals("--color-mask")) {
				birOpts.colorMask = true;
			} else if (argv[i].equals("--threads")) {
				i++;
				threads = Integer.parseInt(argv[i]);
			} else if (argv[i].equals("--boundx")) {
				// upper bound on pixel size
				i++;
				boundx = Integer.parseInt(argv[i]);
			} else if (argv[i].equals("--boundy")) {
				// upper bound on pixel size
				i++;
				boundy = Integer.parseInt(argv[i]);
			} else if (argv[i].equals("--awidth")) {
				// PolygonBucketArray buckets wide
				i++;
				awidth = Integer.parseInt(argv[i]);
			} else if (argv[i].equals("--aheight")) {
				// PolygonBucketArray buckets high
				i++;
				aheight = Integer.parseInt(argv[i]);
			} else if (argv[i].equals("--verbose")) {
				ShapefileBundle.log.setLevel(Level.FINEST);
				//log.info(log.getLevel().toString());
			} else if (argv[i].equals("--csvDist")) {
				i++;
				districtCsvPath = argv[i];
			} else if (argv[i].equals("--outlineOut")) {
				i++;
				outlineOutPath = argv[i];
			} else if (argv[i].equals("-P")) {
				i++;
				stateDataPath = argv[i];
			} else {
				System.err.println("bogus arg: " + argv[i]);
				System.err.print(ShapefileBundle.usage);
				System.exit(1);
				return;
			}
		}
	}
	public void run() throws IOException {
		if (inputPaths.size() == 0) {
			System.err.println("no input shapefile zip bundle specified");
			System.err.print(ShapefileBundle.usage);
			System.exit(1);
		}
		
		long start = System.currentTimeMillis();
		ArrayList<ShapefileBundle> bundles = new ArrayList<ShapefileBundle>();
		for (String path : inputPaths) {
			ShapefileBundle x = new ShapefileBundle();
			try {
				x.open(path);
			} catch (IOException e) {
				System.err.println(path + ": error: " + e.toString());
				e.printStackTrace();
				System.exit(1);
			}
			bundles.add(x);
		}
		ArrayList<PolygonProcessor> pps = new ArrayList<PolygonProcessor>();
		
		// Setup PolygonLinker
		if (linksOut != null) {
			ShapefileBundle.log.info("calculating links");
			PolygonLinker linker = new PolygonLinker(bundles.get(0).shp, linksOut, tree, threads, awidth, aheight);
			for (int i = 1; i < bundles.size(); ++i) {
				linker.growForShapefile(bundles.get(i).shp);
			}
			pps.add(linker);
		}
		
		// Setup PolygonRasterizer
		if ((rastOpts.rastOut != null) || (rastOpts.maskOutName != null)) {
			ShapefileBundle.log.info("rasterizing");
			rastOpts.increaseBoundsFromShapefile(bundles.get(0).shp);
			for (int i = 1; i < bundles.size(); ++i) {
				ShapefileBundle.log.log(Level.FINE, "pre : {0}", rastOpts);
				rastOpts.increaseBoundsFromShapefile(bundles.get(i).shp);
				ShapefileBundle.log.log(Level.FINE, "post: {0}", rastOpts);
			}
			rastOpts.updatePixelSize(boundx, boundy);
			ShapefileBundle.log.info(rastOpts.toString());
			ShapefileBundle.log.info(rastOpts.getOptString(" "));
			if (rastOptOut != null) {
				FileWriter fw = new FileWriter(rastOptOut);
				fw.write(rastOpts.getOptString("\n"));
				fw.close();
			}
			pps.add(new PolygonRasterizer(rastOpts));
		}
		
		// Setup district solution outliner
		if ((districtCsvPath != null) && (outlineOutPath != null)) {
			ShapefileBundle.log.info("will emit outline");
			pps.add(new DistrictMapOutline(districtCsvPath, outlineOutPath));
		}
		
		long end = System.currentTimeMillis();
		ShapefileBundle.log.info("setup done in " + ((end - start) / 1000.0) + " seconds");
		start = end;
		int count = 0;
		for (ShapefileBundle x : bundles) {
			ShapefileBundle.log.info("processing " + x.toString());
			count += x.read(pps);
		}
		end = System.currentTimeMillis();
		ShapefileBundle.log.info("read " + count + " in " + ((end - start) / 1000.0) + " seconds");
		start = end;
		for (PolygonProcessor pp : pps) {
			try {
				ShapefileBundle.log.log(Level.INFO, "finishing {0}...", pp.name());
				pp.finish();
				end = System.currentTimeMillis();
				ShapefileBundle.log.info(pp.name() + " finished in " + ((end - start) / 1000.0) + " seconds");
				start = end;
			} catch (Exception e) {
				e.printStackTrace();
			}
		}
	}
}