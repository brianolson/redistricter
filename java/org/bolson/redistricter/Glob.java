package org.bolson.redistricter;

import java.io.IOException;
import java.nio.file.*;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Iterator;

/**
 * Created by bolson on 2017-5-13.
 */
public class Glob {
    public static Collection<Path> glob(String pattern) {
        FileSystem fs = FileSystems.getDefault();
        Path pp = fs.getPath(pattern);
        boolean abs = pp.isAbsolute();
        Iterator<Path> parts = pp.iterator();
        ArrayList<Path> activePaths = new ArrayList<>();
        {
            Path root = null;
            if (pp.isAbsolute()) {
                root = pp.getRoot();
            } else {
                root = Paths.get(".");
            }
            Path firstpart = parts.next();
            try {
                try (DirectoryStream<Path> ds = Files.newDirectoryStream(root, firstpart.toString())) {
                    for (Path match : ds) {
                        activePaths.add(root.resolve(match).normalize());
                    }
                }
            } catch (IOException e) {
                // can't read that, okay
            }
        }
        while (parts.hasNext()) {
            Path part = parts.next();
            ArrayList<Path> prevPaths = activePaths;
            activePaths = new ArrayList<>();
            for (Path root : prevPaths) {
                if (!Files.isDirectory(root)) {
                    continue;
                }
                try {
                    try (DirectoryStream<Path> ds = Files.newDirectoryStream(root, part.toString())) {
                        for (Path match : ds) {
                            activePaths.add(root.resolve(match));
                        }
                    }
                } catch (IOException e) {
                    // can't read that, okay
                }
            }
        }
        return activePaths;
    }

    public static void main(String argv[]) {
        for (String arg : argv) {
            for (Path p : glob(arg)) {
                System.out.println(p);
            }
        }
    }
}
