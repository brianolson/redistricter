/**
 * Just enough implementation of the "Shapefile" GIS format to read Census shapefiles.
 */
package org.bolson.redistricter;

import java.io.DataInputStream;
import java.io.EOFException;
import java.io.IOException;
import java.io.InputStream;

import org.osgeo.proj4j.ProjCoordinate;


class Shapefile {
	Proj projection = null;
	
	static class Header {
		/*
		 * 	'fileCode': ShapefileHeaderField(0,4,'>i'),
	'fileLength': ShapefileHeaderField(24,28,'>i'),
	'version': ShapefileHeaderField(28,32,'<i'),
	'shapeType': ShapefileHeaderField(32,36,'<i'),
	'xmin': ShapefileHeaderField(36,44,'<d'),
	'ymin': ShapefileHeaderField(44,52,'<d'),
	'xmax': ShapefileHeaderField(52,60,'<d'),
	'ymax': ShapefileHeaderField(60,68,'<d'),
	'zmin': ShapefileHeaderField(68,76,'<d'),
	'zmax': ShapefileHeaderField(76,84,'<d'),
	'mmin': ShapefileHeaderField(84,92,'<d'),
	'mmax': ShapefileHeaderField(92,100,'<d'),
	
		 */
		int fileCode;
		int fileLength;
		int version;
		int shapeType;
		double xmin, ymin, xmax, ymax, zmin, zmax, mmin, mmax;
		public void read(DataInputStream in) throws IOException {
			byte[] data = new byte[100];
			in.readFully(data);
			fileCode = ShapefileBundle.bytesToInt(data, 0);
			fileLength = ShapefileBundle.bytesToInt(data, 24);
			version = ShapefileBundle.bytesToIntLE(data, 28);
			shapeType = ShapefileBundle.bytesToIntLE(data, 32);
			xmin = ShapefileBundle.bytesToDoubleLE(data, 36);
			ymin = ShapefileBundle.bytesToDoubleLE(data, 44);
			xmax = ShapefileBundle.bytesToDoubleLE(data, 52);
			ymax = ShapefileBundle.bytesToDoubleLE(data, 60);
			zmin = ShapefileBundle.bytesToDoubleLE(data, 68);
			zmax = ShapefileBundle.bytesToDoubleLE(data, 76);
			mmin = ShapefileBundle.bytesToDoubleLE(data, 84);
			mmax = ShapefileBundle.bytesToDoubleLE(data, 92);
		}
		
		public String toString() {
			return "(Shapefile code=" + fileCode + " length=" + fileLength + " version=" + version +
			" shape=" + shapeType +
			" x=[" + xmin + "," + xmax + "] y=[" + ymin + "," + ymax +
			"] z=[" + zmin + "," + zmax + "] m=[" + mmin + "," + mmax + "])";
		}
		
		public void project(Proj projection) {
			if (projection == null) {
				return;
			}
			ProjCoordinate b = projection.project(xmin, ymin);
			xmin = b.x;
			ymin = b.y;
			b = projection.project(xmax, ymax);
			xmax = b.x;
			ymax = b.y;
		}
	}
	DataInputStream in;
	public Header header = null;
	byte[] recordHeader = new byte[8];
	byte[] recordBuffer = null;
	public int recordCount = 0;
	
	public void setInputStream(InputStream in) {
		if (in instanceof DataInputStream) {
			this.in = (DataInputStream)in;
		} else {
			this.in = new DataInputStream(in);
		}
	}
	public Header getHeader() throws IOException {
		if (header != null) {
			return header;
		}
		header = new Header();
		header.read(in);
		header.project(projection);
		return header;
	}
	public ESRIShape next() throws IOException {
		getHeader();
		int recordContentLength;
		try {
			in.skipBytes(4);//int recordNumber = in.readInt();
			recordContentLength = in.readInt() * 2;
		} catch (EOFException e) {
			return null;
		}
		if (recordBuffer == null || recordBuffer.length < recordContentLength) {
			recordBuffer = new byte[recordContentLength];
		}
		in.readFully(recordBuffer, 0, recordContentLength);
		recordCount++;
		int type = ShapefileBundle.bytesToIntLE(recordBuffer, 0);
		ESRIShape out = null;
		if (type == 5) {
			out = new Polygon(recordBuffer, 0, recordContentLength);
		} if (type == 3) {
			out = new PolyLine(recordBuffer, 0, recordContentLength);
		}
		out.project(projection);
		return out;
	}
	public void setProjection(Proj projection) {
		this.projection = projection;
		if (this.projection != null) {
			ShapefileBundle.log.info("shp using projection " + this.projection);
		}
	}
}
