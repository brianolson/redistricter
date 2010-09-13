package org.bolson.redistricter;

/**
 * TODO: this is nearly the same as Polygon, but with lines that aren't closed loops. Unify some code?
 * @author bolson
 *
 */
public class PolyLine extends ESRIShape {
	public double xmin, xmax, ymin, ymax;
	public int[] parts;
	public double[] points;
	public PolyLine(byte[] recordBuffer, int i, int recordContentLength) {
		// TODO Auto-generated constructor stub
	}
	/**
	 * Parse binary data into structure in memory.
	 * @param data The bytes after the (numer,length) record header.
	 * @param offset offset into data[]
	 * @param length number of bytes in record
	 */
	public void init(byte[] data, int offset, int length) {
		int pos = 0;
		int type = ShapefileBundle.bytesToIntLE(data, pos); pos += 4;
		assert(type == 5);
		xmin = ShapefileBundle.bytesToDoubleLE(data, pos); pos += 8;
		ymin = ShapefileBundle.bytesToDoubleLE(data, pos); pos += 8;
		xmax = ShapefileBundle.bytesToDoubleLE(data, pos); pos += 8;
		ymax = ShapefileBundle.bytesToDoubleLE(data, pos); pos += 8;
		int numParts = ShapefileBundle.bytesToIntLE(data, pos); pos += 4;
		int numPoints = ShapefileBundle.bytesToIntLE(data, pos); pos += 4;
		parts = new int[numParts];
		points = new double[numPoints*2];
		for (int i = 0; i < numParts; ++i) {
			parts[i] = ShapefileBundle.bytesToIntLE(data, pos);
			pos += 4;
		}
		for (int i = 0; i < numPoints * 2; ++i) {
			points[i] = ShapefileBundle.bytesToDoubleLE(data, pos);
			pos += 8;
		}
		assert(pos == length);
		assert(isConsistent());
	}
	
	/**
	 * Check that all point are within min-max.
	 * @return true if all is well
	 */
	public boolean isConsistent() {
		for (int i = 0; i < points.length; i += 2) {
			if (points[i] > xmax) {
				return false;
			}
			if (points[i] < xmin) {
				return false;
			}
			if (points[i+1] > ymax) {
				return false;
			}
			if (points[i+1] < ymin) {
				return false;
			}
		}
		return true;
	}
	public String toString() {
		StringBuffer out = new StringBuffer("(PolyLine ");
		out.append(xmin);
		out.append("<=x<=");
		out.append(xmax);
		out.append(' ');
		out.append(ymin);
		out.append("<=y<=");
		out.append(ymax);
		int part = 0;
		int pos = parts[part];
		part++;
		while (part < parts.length) {
			out.append(" (");
			out.append(points[pos*2]);
			out.append(',');
			out.append(points[pos*2 + 1]);
			pos++;
			while (pos != parts[part]) {
				out.append(' ');
				out.append(points[pos*2]);
				out.append(',');
				out.append(points[pos*2 + 1]);
				pos++;
			}
			out.append(')');
			part++;
		}
		// last/only part
		out.append(" (");
		out.append(points[pos*2]);
		out.append(',');
		out.append(points[pos*2 + 1]);
		pos++;
		while (pos*2 < points.length) {
			out.append(' ');
			out.append(points[pos*2]);
			out.append(',');
			out.append(points[pos*2 + 1]);
			pos++;
		}
		out.append("))");
		return out.toString();
	}
}
