package redistricter;

import static org.junit.Assert.*;
import junit.framework.Assert;

import org.bolson.redistricter.ESRIShape;
import org.junit.Test;

public class ShapeMath {

	@Test
	public void test() {
		assertEquals(1, ESRIShape.pcenterBelow(9.4, 10.0, 1.0));
		assertEquals(0, ESRIShape.pcenterBelow(9.6, 10.0, 1.0));
		
		assertEquals(9.5, ESRIShape.pcenterY(0, 10.0, 1.0), 0.001);
		assertEquals(8.5, ESRIShape.pcenterY(1, 10.0, 1.0), 0.001);
		assertEquals(0.5, ESRIShape.pcenterY(9, 10.0, 1.0), 0.001);
		
		assertEquals(0, ESRIShape.posToPixelY(9.6, 10.0, 1.0));
		assertEquals(0, ESRIShape.posToPixelY(9.4, 10.0, 1.0));
		assertEquals(0, ESRIShape.posToPixelY(9.01, 10.0, 1.0));
		assertEquals(1, ESRIShape.posToPixelY(8.99, 10.0, 1.0));
		
		assertEquals(0.5, ESRIShape.pcenterX(0, 0.0, 1.0), 0.001);
		assertEquals(1.5, ESRIShape.pcenterX(1, 0.0, 1.0), 0.001);
		
		assertEquals(0, ESRIShape.posToPixelX(0.1, 0.0, 1.0));
		assertEquals(0, ESRIShape.posToPixelX(0.5, 0.0, 1.0));
		assertEquals(0, ESRIShape.posToPixelX(0.9, 0.0, 1.0));
		assertEquals(1, ESRIShape.posToPixelX(1.1, 0.0, 1.0));
		
		assertEquals(0, ESRIShape.pcenterRight(0.49, 0.0, 1.0));
		assertEquals(1, ESRIShape.pcenterRight(0.55, 0.0, 1.0));
		assertEquals(1, ESRIShape.pcenterRight(1.45, 0.0, 1.0));

		assertEquals(0, ESRIShape.pcenterLeft(1.49, 0.0, 1.0));
		assertEquals(1, ESRIShape.pcenterLeft(1.55, 0.0, 1.0));
		assertEquals(1, ESRIShape.pcenterLeft(2.45, 0.0, 1.0));
	}

}
