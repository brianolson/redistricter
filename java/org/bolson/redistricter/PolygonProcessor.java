package org.bolson.redistricter;


/**
 * Receives (and does not modify) polygon and runs its processing.
 * @author bolson
 */
interface PolygonProcessor {
	/*
	 * Receive but do not modify this polygon.
	 * @param p some data
	 */
	public void process(Polygon p);
	
	/**
	 * Close out whatever was started. All polygons have been delivered.
	 */
	public void finish() throws Exception;
	
	
	/**
	 * Simple identifier, a word or short phrase.
	 * @return
	 */
	public String name();
}