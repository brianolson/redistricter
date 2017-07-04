package org.bolson.redistricter;

import org.apache.commons.cli.*;

import java.io.IOException;
import java.nio.file.FileSystem;
import java.nio.file.FileSystems;
import java.nio.file.Path;
import java.nio.file.PathMatcher;
import java.util.ArrayList;

/**
 * Read block shapes and block assignment and write out per-district shapes.
 *
 * Read a state's *faces*zip for polygons.
 * Read state's protobuf for ubid:ubid links and canonical block order.
 * Read .dsz gzipped block assignment file.
 * Flood fill traverse each district joining adjacent polygons and dropping interior line segments.
 * Gather resulting polygon and write out a new .shp for each district or for district set.
 *
 * Created by bolson on 2017-4-30.
 */
public class BlockToShape {
    public static void main(String argv[]) {
        DefaultParser ap = new DefaultParser();
        Options options = new Options();
        options.addOption("f", "faces", true, "path or glob of *faces*.zip to read");
        options.addOption("pb", true, "state protobuf file with nodes and links and data");
        options.addOption("s", "dszSolution", true, "dsz block assignment list of solution");
        // TODO: also add a CSV read mode?
        options.addOption("o", "out", true, "path to write output");

        CommandLine args = null;
        try {
            args = ap.parse(options, argv);
        } catch (ParseException e) {
            System.err.print(e);
            (new HelpFormatter()).printHelp("BlockToShape", options);
            System.exit(1);
            return;
        }

        String[] rawFaces = args.getOptionValues("faces");
        if (rawFaces == null || rawFaces.length == 0) {
            System.err.print("must specify *faces*.zip files with -f or --faces");
            System.exit(1);
            return;
        }
        ArrayList<Path> facesZipsPaths = new ArrayList<>();
        for (String faceArg : rawFaces) {
            facesZipsPaths.addAll(Glob.glob(faceArg));
        }
        if (facesZipsPaths.size() == 0) {
            System.err.println("found no faces zip files");
        }

        for (Path path : facesZipsPaths) {
            ShapefileBundle x = new ShapefileBundle();
            //x.setProjection(projection); // no projection, raw shapefile lat-lon straight through
            try {
                x.open(path);
            } catch (IOException e) {
                System.err.println(path + ": error: " + e.toString());
                e.printStackTrace();
                System.exit(1);
            }
            // TODO: process polygons from bundle x
        }
    }
}
