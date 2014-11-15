/* Nik Zaborovsky, Nov-2014 */

var txt = {length: 140, xsize: 20, ysize: 20};
var threads = 29;


var show_uber_table = function(results) {
	d3.selectAll("#demo").remove();

	d3.select("body").select("#tab").append("div")
		.attr("id", "demo")
		.style("width", "100%");

	$(document).ready(function() {
  		$('#demo').html( '<table cellpadding="0" cellspacing="0" border="0" class="display" id="example"></table>' );

  		$('#example').dataTable( {
  			//dom: 'T<"clear"><"clear">lfrtip',
  			dom: '<"clear">Tftlp',
			"tableTools": {
            	"aButtons": [
                {
                    "sExtends": "csv",
                    "sButtonText": "Save to CSV",
                    "sButtonClass": "btn"
                },
                {
                    "sExtends": "print",
                    "sButtonText": "Print",
                    "sButtonClass": "btn"
                }
            	]
        	},
			"autoWidth": false,
			//"lengthChange": false,
			//"paging": false,
    		"data": results,
    		"columns": [
    			{ "title": "Group #" },
    			{ "title": "Address" },
     			{ "title": "Sharing Intensity*, KPMI" },
     			{ "title": "Function" },
		    	{ "title": "Source Location" },
    			{ "title": "Variable Name", "class": "center" },
    			{ "title": "Allocation Site", "class": "center" }
    		]
  		}); // table
 
	} );
}

var results = [];
var remainingFiles = 0;
var allFiles = 0;

function callback(details, funcname, num) {
	details = details.filter(function(d) {
	for(var i = 0; i < threads; i++) {
		var field = "inf" + i;
		var parts = funcname.split(/:/);
		var field_parts = d[field].split(/,|\(|\)/);

		if (parts.length == 1 && field_parts[1] == funcname)
			return true;
		
		if (parts.length == 2 && field_parts[1] == parts[1] && field_parts[2].substring(1) == parts[0])
			return true;
	}	
	return false;
	})

	details.forEach(function(line) {
		var sum = 0;
		var parts = [];
		for(var i = 0; i < threads; i++) {
			var infN = "inf" + i;
			var refN = "ref" + i;
			if (line[refN] != "0") {
				var info = line[infN];
				parts = info.split(/,|\(|\)/);
				sum += +line[refN];
			}
		}
		// Recent results are calculated using 500K sampling intervals,
		// as we are calcualting KPMI, we need to *2/1000.
		var Ksum = Math.floor(sum * 2 / 10) / 100;
		results[results.length] = [num, "0x" + (+line.addr).toString(16), Ksum, parts[2], parts[0], parts[1], parts[3]];
	});

	remainingFiles--;
	
	d3.select(".meter").selectAll('span').remove();
	d3.select(".meter").append('span').style("width", function() { return (100 * (allFiles - remainingFiles) / allFiles) + "%"; })
		.text("  Loading data...");

	if (remainingFiles == 0) {
		d3.select(".meter").remove();
		show_uber_table(results);
	}
}

function function_details(subfile) {
	results = [];
	remainingFiles = subfile.length;
	allFiles = remainingFiles;
	d3.selectAll(".details_view").remove();
	var details;

	subfile.forEach(function(filename) {
		d3.json(filename[0], function(error, det) {
			details = det;
			callback(details, filename[1], filename[2])
		});
	});

};


function parseSecond(val) {
    var result = undefined,
        tmp = [];
    var items = location.search.substr(1).split("&");
    for (var index = 0; index < items.length; index++) {
        tmp = items[index].split("=");
        if (tmp[0] === val) result = decodeURIComponent(tmp[1]);
    }
    return result;
}


var invalidate = function() {

d3.json("data/main.json", function(error, table) {
	// Assiging indices to functions
	var bfunctions = new Array();
	var functions = [];
	// Assigning indices to sample numbers - "num"
	var btimes = new Array();
	var times = [];

	var fcount = 0;
	var tcount = 0;

	var max_sharing = 0;

	var threshold = 400;

	// {"file": "sharing-mystery1-new json","ver": "0 2 0 0","threadnum": "29"}
	threads = +table[0].threadnum;
	var pfile = table[0].file;
	var vers = table[0].ver;
	table.shift(); // remove the first line

	var new_threshold = parseSecond("threshold");
	if (new_threshold != undefined)
		threshold = new_threshold;
	else
		location.href = "index.html?threshold=" + threshold;

	filtered_table = table.filter(function(d){ return +d.refs > threshold; })

	filtered_table.forEach(function(line) {
		if (bfunctions[line.func] == undefined) {
			bfunctions[line.func] = fcount;
			fcount++;
			functions[functions.length] = line.func;
		}

		if (+line.refs > +max_sharing)
			max_sharing = +line.refs;
	});

	table.forEach(function(line) {
		if (btimes[line.num] == undefined) {
			btimes[line.num] = tcount;
			tcount++;
			times[times.length] = line.num;
		}
	});

	var margin = {top: 30, right: 0, bottom: 0, left: 80},
    	width = tcount * txt.xsize + txt.xsize,
    	height = fcount * txt.ysize;

	var svg = d3.select("body").select("#main")
		.append("svg")
    	.attr("width", width)
    	.attr("height", height + margin.top + margin.bottom)
//    	.style("margin-left", -200 + "px")
    	.append("svg:g")
    	.attr("transform", function() {
    		return "translate(0, " + margin.top + ")"; 
		})
//		.call(d3.behavior.zoom().on("zoom", redraw))
//    	.append("g");

//	function redraw() {
//		svg.attr("transform",
//      		"translate(" + d3.event.translate + ")"
//      		+ " scale(" + d3.event.scale + ")");
//	}

	svg.append("rect")
		.attr("class", "background")
		.attr("width", width)
		.attr("height", height);

	// ==================== FUNC NAMES left panel =====================

	var leftpan = d3.select("#funcnames").append("svg")
    	.attr("width", txt.length)
    	.attr("height", height + margin.top + margin.bottom)
    	//.style("margin-left", -margin.left + "px")
    	.append("svg:g")
    	.attr("transform", function() {
    		var x = txt.length;
    		return "translate(" + x + ", " + margin.top + ")"; 
    	});

	var leftrows = leftpan.selectAll(".row")
		.data(functions)
    	.enter().append("g")
      		.attr("class", "row")
      		.attr("transform", function(d, i) { return "translate(0," + ((i) * txt.ysize) + ")"; });

  	leftrows.append("text")
		.attr("x", 0)
		.attr("y", function(d, i) { return -txt.ysize / 2; })
		.attr("dy", ".32em")
		.attr("text-anchor", "end")
		.text(function(d, i) {
			var parts = functions[i].split(/:/);
			return parts[0].substring(0, 20);
		});

	// ===================== VAR NAMES panel =========================

	var varpan = d3.select("#varnames").append("svg")
    	.attr("width", 100)
    	.attr("height", height + margin.top + margin.bottom)
    	.append("svg:g")
    	.attr("transform", function() {
    		var x = 0;
    		return "translate(" + x + ", " + margin.top + ")"; 
    	});

	var varrows = varpan.selectAll(".rowv")
		.data(functions)
    	.enter().append("g")
      		.attr("class", "rowv")
      		.attr("transform", function(d, i) { return "translate(0," + ((i + 1) * txt.ysize) + ")"; });

  	varrows.append("text")
		.attr("x", 0)
		.attr("y", function(d, i) { return -txt.ysize / 2; })
		.attr("dy", ".32em")
		.attr("text-anchor", "start")
		.text(function(d, i) {
			var parts = functions[i].split(/:/);
			if (1 == parts.length)
				return ""; 
			return ":" + parts[1].substring(0, 10);
		});

	// ===================== FILE line =================================


    d3.select("#file").html(function() {
    	return "File: " + pfile.replace(/#/g, '.').replace(/@/g, '/') + "<br>" + "Version: " + vers.replace(/ /g, ".");
    });

    // ====================== MAIN central panel =======================

	var row = svg.selectAll(".row")
		.data(functions)
    	.enter().append("g")
      		.attr("class", "row")
      		.attr("transform", function(d, i) { return "translate(0," + ((i + 1) * txt.ysize) + ")"; });

  	row.append("line")
		.attr("x2", width);

	var column = svg.selectAll(".column")
		.data(times)
    	.enter().append("g")
      		.attr("class", "column")
      		.attr("transform", function(d, i) { return "translate(" + ((i + 1) * txt.xsize) + ")rotate(-90)"; });

  	column.append("line")
    	.attr("x1", -width);

	column.append("text")
		.attr("x", 25)
      	.attr("y", -txt.ysize / 2)
      	.attr("dy", ".32em")
      	.attr("text-anchor", "end")
      	.text(function(d, i) { return times[i]; });

    var filesandfunctions = [];

	filtered_table.forEach(function(line) {

		var cell = svg.append("rect")
			.datum(line)
        	.attr("x", function(d) { return +txt.xsize * +btimes[d.num] + 1; })
        	.attr("y", function(d) { return +txt.ysize * +bfunctions[d.func] + 1; })
        	.attr("width", txt.xsize - 2)
       		.attr("height", txt.ysize - 2)
       		.style("fill-opacity", function(d) { return 0.2 + 0.8 * d.refs / max_sharing; })
       		.style("fill", function(d) { return "#FFA465"; })
       		.attr("class", "cell")
       		.on("click", function(d) {
       			console.log("data/" + d.num + ".json");
       			function_details([["data/" + d.num + ".json", d.func, d.num]]);
				svg.append("rect")
					.datum(d)
					.attr("class", "details_view")
        			.attr("x", function(d) { return +txt.xsize * +btimes[d.num] + 1; })
        			.attr("y", function(d) { return +txt.ysize * +bfunctions[d.func] + 1; })
        			.attr("width", txt.xsize - 2)
       				.attr("height", txt.ysize - 2)
       				.style("stroke", "black")
       				.style("stroke-width", 3)
       				.style("fill", "none");

       		})
		    .on("mouseover", function(p) {
		    	d3.select(this).style("stroke", "#4A0D3F").style("stroke-width", 2);
		    	var rows = d3.selectAll(".row text");
		    	var y_p = +bfunctions[p.func];
		    	var x_p = +btimes[p.num];
		    	d3.selectAll(".rowv text").classed("highlighted", function(d, i) { return i == y_p; });
		    	d3.selectAll(".row text").classed("highlighted", function(d, i) { return i == y_p; });
		    	d3.selectAll(".column text").classed("highlighted", function(d, i) { return i == x_p; });
		    })
		    .on("mouseout", function() {
		    	d3.select(this).style("stroke", "none");
		    	d3.selectAll(".rowv text").classed("highlighted", false);
		    	d3.selectAll(".row text").classed("highlighted", false);
		    	d3.selectAll(".column text").classed("highlighted", false);		    	
		    })

		if (+line.refs > threshold)
			filesandfunctions[filesandfunctions.length] = ["data/" + line.num + ".json", line.func, line.num];
	});
	function_details(filesandfunctions);
});
}; // function

invalidate();

