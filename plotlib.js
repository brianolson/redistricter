/** plotlib.js
 * Copyright 2010 Brian Olson
 * GPLv2 http://www.gnu.org/licenses/old-licenses/gpl-2.0.html */

/*
options dict:
'xlabels': ['min x label string', 'max x label string']
'ylabels': ['min y label string', 'max y label string', (optional)'last y label string', (optional,repeated)[yvalue, 'y label string']...]
'ytitle': 'y title string'
'miny': minimum y value, otherwise derive from data
'target': a y value for horizontal line for Cal plot
'lineStyle': default 'red', or other canvas style string

*/

function PlotCommon() {};

PlotCommon.prototype.setupxy = function(xy) {
    if (!this.firstdatas) {
	this.minx = xy[0];
	this.miny = xy[1];
	this.maxx = xy[0];
	this.maxy = xy[1];
	this.lasty = xy[1];
	this.firstdatas = true;
    }
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
}

PlotCommon.prototype.setup = function(canvas, xy, opt) {
    if (opt && opt.data) {
		for (var di = 0, ds; ds = opt.data[di]; di++) {
			this.setupxy(ds.xy);
		}
    } else {
	this.setupxy(xy);
    }
	this.ctx = canvas.getContext('2d');
  if (opt && opt['ylabels']) {
    this.miny_str = opt['ylabels'][0] || '';
    this.maxy_str = opt['ylabels'][1] || '';
    this.lasty_str = opt['ylabels'][2] || '';
  } else {
	this.miny_str = new String(this.miny);
	this.maxy_str = new String(this.maxy);
	this.lasty_str = new String(this.lasty);
  }

	this.miny_width = this.ctx.measureText(this.miny_str).width;
	this.maxy_width = this.ctx.measureText(this.maxy_str).width;
	this.lasty_width = this.ctx.measureText(this.lasty_str).width;
	this.max_ystr_width = Math.max(this.maxy_width, this.miny_width, this.lasty_width);
  if (opt && opt['ylabels'] && (opt['ylabels'].length > 3)) {
    for (var yli = 3, yl; yl = opt['ylabels'][yli]; yli++) {
      var yl_width = this.ctx.measureText(yl[1]).width;
      if (yl_width > this.max_ystr_width) {
        this.max_ystr_width = yl_width;
      }
    }
  }
	if (opt && (opt['miny'] != undefined)) {
		this.miny = opt['miny'];
	}
	if (opt && (opt['maxy'] != undefined)) {
		this.maxy = opt['maxy'];
	}
	if (opt && (opt['minx'] != undefined)) {
		this.minx = opt['minx'];
	}
	if (opt && (opt['maxx'] != undefined)) {
		this.maxx = opt['maxx'];
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
	var minxlabel = null;
	var maxxlabel = null;
	if (opt && opt['xlabels']) {
		minxlabel = opt['xlabels'][0];
		maxxlabel = opt['xlabels'][1];
	} else {
		minxlabel = new String(this.minx);
		maxxlabel = new String(this.maxx);
	}
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
		this.ctx.fillText(minxlabel, this.px(this.minx), this.py(this.miny));
		this.ctx.textAlign = 'right';
		this.ctx.fillText(maxxlabel, this.px(this.maxx), this.py(this.miny));

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
	if (opt && opt['ytitle']) {
/* put y title right of right edge at center */
		this.ctx.fillStyle = '#900';
		this.ctx.textAlign = 'left';
		this.ctx.textBaseline = 'middle';
		this.ctx.fillText(new String(opt['ytitle']), this.px(this.maxx), this.py((this.maxy + this.miny)/2));
	}
  if (opt && opt['ylabels'] && (opt['ylabels'].length > 3)) {
    this.ctx.fillStyle = '#555';
    this.ctx.textAlign = 'left';
    this.ctx.textBaseline = 'middle';
    for (var yli = 3, yl; yl = opt['ylabels'][yli]; yli++) {
      this.ctx.fillText(yl[1], this.px(this.maxx), this.py(yl[0]));
    }
  }
	if ((this.lasty != this.miny) && (this.lasty != this.maxy) && this.lasty_str) {
		// Put the last Y value on the right edge in red.
		this.ctx.fillStyle = '#900';
		this.ctx.textAlign = 'left';
		this.ctx.textBaseline = 'middle';
		// TODO: something smart about how if lasty is close to miny or maxy, adjust text baselines so they don't clobber each other
		this.ctx.fillText(this.lasty_str, this.px(this.maxx), this.py(this.lasty));
	}
    if (opt && opt.data) {
	this.ctx.textAlign = 'left';
	this.ctx.textBaseline = 'top';
	var lx = this.px(this.maxx);
	var ly = this.py(this.maxy);
	for (var di = 0, ds; ds = opt.data[di]; di++) {
	    if (!ds.name) {continue;}
	    this.ctx.strokeStyle = ds.strokeStyle || '#000';
	    this.ctx.fillStyle = ds.fillStyle || '#000';
	    ly += 10;
	    this.ctx.fillText(ds.name, lx, ly);
	}
    }
	if (opt && opt.vlines) {
		// draw vertical lines at various X values
		this.ctx.strokeStyle = 'rgba(0,0,255,128)';
		//this.ctx.strokeStyle = '#00f';
		for (var vi = 0, vv; vv = opt.vlines[vi]; vi++) {
			this.ctx.beginPath();
			this.ctx.moveTo(this.px(vv), this.py(this.miny));
			this.ctx.lineTo(this.px(vv), this.py(this.maxy));
			this.ctx.stroke();
		}
	}
}

function LinePlot() {};
LinePlot.prototype = new PlotCommon;

LinePlot.prototype.plot = function(canvas, xy, opt) {
    this.setup(canvas, xy, opt);
	this.ctx.clearRect(0,0, canvas.width, canvas.height);
	this.ctx.strokeStyle = '#000';
	this.ctx.beginPath();
	this.ctx.moveTo(this.px(xy[0]), this.py(xy[1]));
	for (var i = 2; i < xy.length; i += 2) {
		this.ctx.lineTo(this.px(xy[i]), this.py(xy[i+1]));
	}
	this.ctx.stroke();
	this.axisLabels(opt);
};

function lineplot(canvas, xy, opt) {
	var lp = new LinePlot();
	lp.plot(canvas, xy, opt);
};

function MultiLinePlot() {};
MultiLinePlot.prototype = new PlotCommon;

var defaultLineStyles = ['#900', '#00b', '#aa0', '#0aa', '#444'];

// canvas HTMLCanvasElement
// datas {'name', {'data': [x,y, ...], 'strokeStyle': '#000'}, ...}
MultiLinePlot.prototype.plot = function(canvas, datas, opt) {
    var defaultLineStylesIter = 0;
    var allxy = [];
    for (var dataname in datas) {
	var dat = datas[dataname];
	var xy = dat.data;
	allxy = allxy.concat(xy);
    }
    this.setup(canvas, allxy, opt);
	this.ctx.clearRect(0,0, canvas.width, canvas.height);
    for (var dataname in datas) {
	var dat = datas[dataname];
	var xy = dat.data;
	var strokeStyle = dat.strokeStyle;
	if (!strokeStyle) {
	    strokeStyle = defaultLineStyles[defaultLineStylesIter];
	    defaultLineStylesIter = (defaultLineStylesIter + 1) % defaultLineStyles.length;
	}
	this.ctx.strokeStyle = strokeStyle;
	this.ctx.beginPath();
	this.ctx.moveTo(this.px(xy[0]), this.py(xy[1]));
	for (var i = 2; i < xy.length; i += 2) {
		this.ctx.lineTo(this.px(xy[i]), this.py(xy[i+1]));
	}
	this.ctx.stroke();
    }
	this.axisLabels(opt);
};

function multilineplot(canvas, datas, opt) {
	var lp = new MultiLinePlot();
	lp.plot(canvas, datas, opt);
};

function ScatterPlot() {};
ScatterPlot.prototype = new PlotCommon;

/**
accepts data either in xy = [x,y, x,y, ...] or opt['data'] = [{'fillStyle':'#123', 'xy':[x,y, x,y, ...]}, ... ]
*/
ScatterPlot.prototype.plot = function(canvas, xy, opt) {
    this.setup(canvas, xy, opt);
	this.ctx.clearRect(0,0, canvas.width, canvas.height);
	if (opt && opt.data) {
	    for (var di = 0, ds; ds = opt.data[di]; di++) {
		this.ctx.strokeStyle = ds.strokeStyle || '#000';
		this.ctx.fillStyle = ds.fillStyle || '#000';
		xy = ds.xy;
		for (var i = 0; i < xy.length; i += 2) {
		    this.ctx.fillRect(this.px(xy[i])-1, this.py(xy[i+1])-1, 3, 3);
		}
	    }
	} else {
	    this.ctx.strokeStyle = '#000';
	    this.ctx.fillStyle = '#000';
	for (var i = 0; i < xy.length; i += 2) {
		this.ctx.fillRect(this.px(xy[i])-1, this.py(xy[i+1])-1, 3, 3);
	}
	}
	this.ctx.strokeStyle = '#000';
	this.ctx.fillStyle = '#000';
	this.axisLabels(opt);
}
function scatterplot(canvas, xy, opt) {
	var sp = new ScatterPlot();
	sp.plot(canvas, xy, opt);
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
	this.axisLabels(opt);
};

function calplot(canvas, xy, opt) {
	var cp = new CalPlot();
	cp.plot(canvas, xy, opt);
};

/* end plotlib.js */
