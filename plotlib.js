/** plotlib.js
 * Copyright 2010 Brian Olson
 * GPLv2 http://www.gnu.org/licenses/old-licenses/gpl-2.0.html */

function PlotCommon() {};

PlotCommon.prototype.setup = function(canvas, xy, opt) {
	this.minx = xy[0];
	this.miny = xy[1];
	this.maxx = xy[0];
	this.maxy = xy[1];
	this.lasty = xy[1];
	for (var i = 2; i < xy.length; i += 2) {
		if (xy[i] < this.minx) {
			this.minx = xy[i];
		}
		if (xy[i] > this.maxx) {
			this.maxx = xy[i];
		}
		if (xy[i+1] < this.miny) {
			this.miny = xy[i+1];
		}
		if (xy[i+1] > this.maxy) {
			this.maxy = xy[i+1];
		}
		this.lasty = xy[i+1];
	}
	this.ctx = canvas.getContext('2d');
	this.miny_str = new String(this.miny);
	this.miny_width = this.ctx.measureText(this.miny_str).width;
	this.maxy_str = new String(this.maxy);
	this.maxy_width = this.ctx.measureText(this.maxy_str).width;
	this.lasty_str = new String(this.lasty);
	this.lasty_width = this.ctx.measureText(this.lasty_str).width;
	this.max_ystr_width = Math.max(this.maxy_width, this.miny_width, this.lasty_width);
	if (opt && (opt['miny'] != undefined)) {
		this.miny = opt['miny'];
	}
	this.insetx = 0;//max_ystr_width * 1.1;
	this.insety = canvas.height - 11;
	this.scalex = (canvas.width - this.max_ystr_width) / (this.maxx - this.minx);
	this.scaley = (canvas.height - 11) / (this.maxy - this.miny);
};

PlotCommon.prototype.px = function(dx) {
	return ((dx - this.minx) * this.scalex) + this.insetx;
}
PlotCommon.prototype.py = function(dy) {
	return this.insety - ((dy - this.miny) * this.scaley);
};

PlotCommon.prototype.axisLabels = function(opt) {
	if (opt && opt['ylabels']) {
		
	} else {
		/* Label miny and maxy on right edge of graph. */
		this.ctx.strokeStyle = '#555';
		this.ctx.beginPath();
		this.ctx.moveTo(this.px(this.maxx), this.py(this.maxy));
		this.ctx.lineTo(this.px(this.maxx) + 3, this.py(this.maxy));
		this.ctx.moveTo(this.px(this.maxx), this.py(this.miny));
		this.ctx.lineTo(this.px(this.maxx) + 3, this.py(this.miny));
		this.ctx.stroke();
		this.ctx.textAlign = 'left';
		this.ctx.textBaseline = 'bottom';
		this.ctx.fillText(this.miny_str, this.px(this.maxx), this.py(this.miny));
		this.ctx.textBaseline = 'top';
		this.ctx.fillText(this.maxy_str, this.px(this.maxx), this.py(this.maxy));
	}
	if (opt && opt['xlabels']) {
		
	} else {
		/* Label minx and this.maxx on bottom of graph */
		this.ctx.strokeStyle = '#555';
		this.ctx.beginPath();
		this.ctx.moveTo(this.px(this.minx), this.py(this.miny));
		this.ctx.lineTo(this.px(this.minx), this.py(this.miny) + 3);
		this.ctx.moveTo(this.px(this.maxx), this.py(this.miny));
		this.ctx.lineTo(this.px(this.maxx), this.py(this.miny) + 3);
		this.ctx.stroke();
		this.ctx.textAlign = 'left';
		this.ctx.textBaseline = 'top';
		this.ctx.fillText(new String(this.minx), this.px(this.minx), this.py(this.miny));
		this.ctx.textAlign = 'right';
		this.ctx.fillText(new String(this.maxx), this.px(this.maxx), this.py(this.miny));
	}
	if ((this.lasty != this.miny) && (this.lasty != this.maxy)) {
		this.ctx.fillStyle = '#900';
		this.ctx.textAlign = 'left';
		this.ctx.textBaseline = 'middle';
		this.ctx.fillText(new String(this.lasty), this.px(this.maxx), this.py(this.lasty));
	}
}

function LinePlot() {};
LinePlot.prototype = new PlotCommon;

LinePlot.prototype.plot = function(canvas, xy, opt) {
	this.setup(canvas, xy);
	this.ctx.clearRect(0,0, canvas.width, canvas.height);
	this.ctx.strokeStyle = '#000';
	this.ctx.beginPath();
	this.ctx.moveTo(this.px(xy[0]), this.py(xy[1]));
	for (var i = 2; i < xy.length; i += 2) {
		this.ctx.lineTo(this.px(xy[i]), this.py(xy[i+1]));
	}
	this.ctx.stroke();
	this.axisLabels();
};

function lineplot(canvas, xy, opt) {
	var lp = new LinePlot();
	lp.plot(canvas, xy, opt);
};

function CalPlot() {};
CalPlot.prototype = new PlotCommon;

CalPlot.prototype.plot = function(canvas, xy, opt) {
	this.setup(canvas, xy, opt);
	this.ctx.clearRect(0,0, canvas.width, canvas.height);
	if (opt && opt['target']) {
		this.ctx.strokeStyle = '#090';
		this.ctx.beginPath();
		this.ctx.moveTo(this.px(this.minx), this.py(opt['target']));
		this.ctx.lineTo(this.px(this.maxx), this.py(opt['target']));
		this.ctx.stroke();
	}
	if (opt && opt['lineStyle']) {
		this.ctx.strokeStyle = opt['lineStyle'];
	} else {
		this.ctx.strokeStyle = 'red';
	}
	this.ctx.beginPath();
	for (var i = 0; i < xy.length; i += 2) {
		var tx = Math.floor(this.px(xy[i]));
		this.ctx.moveTo(tx, this.py(this.miny));
		this.ctx.lineTo(tx, this.py(xy[i+1]));
	}
	this.ctx.stroke();
	this.axisLabels();
};

function calplot(canvas, xy, opt) {
	var cp = new CalPlot();
	cp.plot(canvas, xy, opt);
};

/* end plotlib.js */
