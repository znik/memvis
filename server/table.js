/* Nik Zaborovsky, Nov-2014 */

var txt = {length: 140, xsize: 20, ysize: 20};
var threads = 29;
var field = "";

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
    			//{ "title": "Range #" },
    			{ "title": "Address" },
     			{ "title": "Sharing Intensity*, KPMI" },
     			{ "title": "Function" },
		    	{ "title": "Source Location" },
		    	{ "title": "Variable Type"},
    			{ "title": "Variable Name", "class": "center" },
    			{ "title": "Allocation Site", "class": "center" }
    		]
  		}); // table
 
	} );
}

var results = [];
var remainingFiles = 0;
var allFiles = 0;
var intaddr = new Array();
var intcnt = new Array();

function callback(details, funcname, num, metric) {
	details = details.filter(function(d) {
	for(var i = 0; i < threads; i++) {
		var field = "inf" + i;
		var parts = funcname.split(/:/);

		if (d[field] != undefined) {
			var field_parts = d[field].split(/,|\(|\)/);

			if (parts.length == 1 && field_parts[1] == funcname)
				return true;
		
			if (parts.length >= 2 && field_parts[1] == parts[1] && field_parts[2].substring(1) == parts[0])
				return true;
		}
	}	
	return false;
	})
	var results1 = [];
	details.forEach(function(line) {
		var sum = 0;
		var parts = [];
		for(var i = 0; i < threads; i++) {
			var infN = "inf" + i;
			var refN = "ref" + i;
			if (line[refN] != undefined && line[refN] != "0") {
				var info = line[infN];
				parts = info.split(/,|\(|\)/);
				//var var_and_field = parts[4].split(/->/);
				//parts.pop();
				//parts.pop();
				//parts[parts.length] = var_and_field[0];
				//if (var_and_field[1] != undefined)
				//	parts[parts.length] = var_and_field[1];
				sum += +line[refN];
			}
		}
		// Recent results are calculated using 500K sampling intervals,
		// as we are calcualting KPMI, we need to *2/1000.
		
		//var Ksum = Math.floor(metric * 2 / 10) / 100;
		var Ksum = metric;
		results1[results1.length] = [num, "0x" + (+line.addr).toString(16), Ksum, parts[2], parts[4], parts[0], parts[1], parts[3], parts[5]];
	});

	results1.forEach(function(line) {
		var new_val = 0;
		if (intaddr[line[1]] != undefined)
			new_val = +(intaddr[line[1]][1]) + +line[2];
		else
			new_val = +line[2];
		//if (line[8] == undefined || line[8] == "")
			intaddr[line[1]] = [line[1], new_val, line[3], line[5], line[4], line[6], line[7]];
		//else
		//	intaddr[line[1]] = [line[1], new_val, line[3], line[5], line[4], line[6] + "->" + line[8], line[7]];

		if (intcnt[line[1]] == undefined)
			intcnt[line[1]] = 1;
		else
			intcnt[line[1]] += 1;
	})

	remainingFiles--;
	
	d3.select(".meter").selectAll('span').remove();
	d3.select(".meter").append('span').style("width", function() { return (100 * (allFiles - remainingFiles) / allFiles) + "%"; })
		.text("  Loading data...");

	if (remainingFiles <= 0) {
		
		for (var i in intaddr) {
			intaddr[i][1] = Math.floor(intaddr[i][1] / intcnt[i] * 2/10) / 100;
			results[results.length] = intaddr[i];
		};

		d3.select(".meter").remove();
		show_uber_table(results);
		$('select').removeAttr('disabled');
	}
}

function function_details(subfile) {
	results = [];
	intaddr = new Array();
	intcnt = new Array();
	remainingFiles = subfile.length;
	allFiles = remainingFiles;
	d3.selectAll(".details_view").remove();
	var details;

	subfile.forEach(function(filename) {
		d3.json(filename[0], function(error, det) {
			details = det;
			callback(details, filename[1], filename[2], filename[3])
		});
	});
	if (subfile.length == 0)
		callback([], "", "", "");
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

d3.json("data/info.json", function(error, info) {
	// {"file": "sharing-mystery1-new json","ver": "0 2 0 0","threadnum": "29"}
	threads = +info[0].threadnum;
	var pfile = info[0].file;
	var vers = info[0].ver;

	// ===================== FILE line =================================
    d3.select("#file").html(function() {
    	return "File: " + pfile.replace(/#/g, '.').replace(/@/g, '/') + "<br>" + "Version: " + vers.replace(/ /g, ".");
    });
})

var invalidate = function() {

	d3.selectAll("svg").remove();
	var val = document.getElementById("soflow-color").value;
	if (val == "Sharing")
		field = "refs";
	if (val == "False Sharing")
		field = "fals";

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

	var threshold = 25*2/1000;

//	table.shift(); // remove the first line

	var new_threshold = parseSecond("threshold");
	if (new_threshold != undefined)
		threshold = new_threshold;
	else
		location.href = "index.html?threshold=" + threshold;

	threshold = threshold / 2 * 1000;

	filtered_table = table.filter(function(d){ return +d[field] > threshold; })

	filtered_table.forEach(function(line) {
		if (bfunctions[line.func] == undefined) {
			bfunctions[line.func] = fcount;
			fcount++;
			functions[functions.length] = line.func;
		}

		if (+line[field] > +max_sharing)
			max_sharing = +line[field];
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
       		.style("fill-opacity", function(d) { return 0.2 + 0.8 * d[field] / max_sharing; })
       		.style("fill", function(d) {
       			if (field == "refs")
       				return "#FFA465";
       			if (field == "fals")
       				return "#91234C";
       		})
       		.attr("class", "cell")
       		.on("click", function(d) {
       			console.log("data/" + d.num + ".json");
       			function_details([["data/" + d.num + ".json", d.func, d.num, d[field]]]);
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

		if (+line[field] > threshold)
			filesandfunctions[filesandfunctions.length] = ["data/" + line.num + ".json", line.func, line.num, line[field]];
	});
	function_details(filesandfunctions);
});
}; // function

invalidate();

function visualTypeChanged() {
	invalidate();
};