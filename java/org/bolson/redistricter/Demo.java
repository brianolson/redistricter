package org.bolson.redistricter;

import java.awt.image.BufferedImage;
import java.io.IOException;

public class Demo {

	public static void main(String[] args) throws IOException {
		Polygon pa = new Polygon(new double[]{1,0, 1,1, 0,1, 1,0});
		Polygon pb = new Polygon(new double[]{0,0, 1,0, 0,1, 0,0});

		RasterizationOptions ropts = new RasterizationOptions();
		ropts.xpx = 1000;
		ropts.ypx = 1000;
		ropts.minx = -0.5;
		ropts.miny = -0.5;
		ropts.maxx = 10.5;
		ropts.maxy = 10.5;
		ropts.maskOutName = "/tmp/demo_mask.png";

/*		BufferedImageRasterizer.Options birOpts = new BufferedImageRasterizer.Options();
		BufferedImage maskImage = new BufferedImage(100, 100, BufferedImage.TYPE_4BYTE_ABGR);
		BufferedImageRasterizer bir = new BufferedImageRasterizer(maskImage, birOpts);
		
		RasterizationContext ctx = new RasterizationContext(ropts);
		bir.setRasterizedPolygon(ctx, pa);
		bir.setRasterizedPolygon(ctx, pb);*/
		
		PolygonRasterizer rast = new PolygonRasterizer(ropts);
		rast.process(pa);
		rast.process(pb);
		
		int yo = 0;
		int xo = 1;
		double a = 0.05;
		while (a < 1.0) {
			double x = xo;
			double y = yo;
			pa = new Polygon(new double[]{x+1,y+a, x+1,y+1, x+0,y+1, x+0,y+1-a, x+1,y+a});
			pb = new Polygon(new double[]{x+0,y+0, x+1,y+0, x+1,y+a, x+0,y+1-a, x+0,y+0});
			rast.process(pa);
			rast.process(pb);
			xo++;
			if (xo == 10) {
				xo = 0;
				yo++;
			}
			a += 0.05;
		}
		a = 0.05;
		while (a < 1.0) {
			double x = xo;
			double y = yo;
			pa = new Polygon(new double[]{x+1,y+0, x+1,y+1, x+1-a,y+1, x+a,y+0, x+1,y+0});
			pb = new Polygon(new double[]{x+0,y+0, x+a,y+0, x+1-a,y+1, x+0,y+1, x+0,y+0});
			rast.process(pa);
			rast.process(pb);
			xo++;
			if (xo == 10) {
				xo = 0;
				yo++;
			}
			a += 0.05;
		}
		while (yo < 10) {
			a = Math.random();
			double x = xo;
			double y = yo;
			pa = new Polygon(new double[]{x+1,y+a, x+1,y+1, x+0,y+1, x+0,y+1-a, x+1,y+a});
			pb = new Polygon(new double[]{x+0,y+0, x+1,y+0, x+1,y+a, x+0,y+1-a, x+0,y+0});
			rast.process(pa);
			rast.process(pb);
			xo++;
			if (xo == 10) {
				xo = 0;
				yo++;
			}
		}
		rast.finish();
	}

}
