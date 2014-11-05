/* Nik Zaborovsky, Nov-2014 */

var txt = {length: 100, xsize: 20, ysize: 20};
var threads = 29; // FIXME: dynamic getting from file


var show_uber_table = function(results) {
	d3.selectAll("#demo").remove();

	d3.select("body").select("#tab").append("div")
		.attr("id", "demo")
		.style("width", "1200px");

	$(document).ready(function() {
  		$('#demo').html( '<table cellpadding="0" cellspacing="0" border="0" class="display" id="example"></table>' );
	} );

	$('#example').dataTable( {
		"autoWidth": false,
		//"lengthChange": false,
		//"paging": false,
    	"data": results,
    	"columns": [
    		{ "title": "#" },
    		{ "title": "Address" },
     		{ "title": "Sharing Intensity, KPMI" },
     		{ "title": "Function" },
		    { "title": "Source Location" },
    		{ "title": "Variable Name", "class": "center" },
    		{ "title": "Allocation Site", "class": "center" }
    	]
  	});
}

var results = [];
var remainingFiles = 0;

function callback(details, funcname, num) {
	details = details.filter(function(d) {
	for(var i = 0; i < threads; i++) {
		var field = "inf" + i;
		if (d[field].indexOf(funcname) != -1)
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
		var Ksum = Math.floor(sum * 2 / 10) / 100;
		results[results.length] = [num, "0x" + (+line.addr).toString(16), Ksum, parts[2], parts[0], parts[1], ""];
	});

	remainingFiles--;
	
	if (remainingFiles == 0)
		show_uber_table(results);
}

function function_details(subfile) {
	results = [];
	remainingFiles = subfile.length;
	d3.selectAll(".details_view").remove();
	var details;

	subfile.forEach(function(filename) {
		d3.json(filename[0], function(error, det) {
			details = det;
			callback(details, filename[1], filename[2])
		});
	});

};



var invalidate = function() {

d3.json("data/main.json", function(table) {
	// Assiging indices to functions
	var bfunctions = new Array();
	var functions = [];
	// Assigning indices to sample numbers - "num"
	var btimes = new Array();
	var times = [];

	var fcount = 0;
	var tcount = 0;

	var max_sharing = 0;

	table = table.filter(function(d){ return +d.refs > 400; })

	table.forEach(function(line) {
		if (bfunctions[line.func] == undefined) {
			bfunctions[line.func] = fcount;
			fcount++;
			functions[functions.length] = line.func;
		}
		if (btimes[line.num] == undefined) {
			btimes[line.num] = tcount;
			tcount++;
			times[times.length] = line.num;
		}
		if (+line.refs > +max_sharing)
			max_sharing = +line.refs;
	});

	var margin = {top: 20, right: 0, bottom: 40, left: 80},
    	width = tcount * txt.xsize + txt.length,
    	height = fcount * txt.ysize;

	var svg = d3.select("body").select("#main")
		.append("svg")
    	.attr("width", width + margin.left + margin.right)
    	.attr("height", height + margin.top + margin.bottom)
    	.style("margin-left", -margin.left + "px")
    	.append("svg:g")
    	.attr("transform", function() {
    		var x = margin.left + txt.length;
    		return "translate(" + x + ", " + margin.top + ")"; 
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

	var row = svg.selectAll(".row")
		.data(functions)
    	.enter().append("g")
      		.attr("class", "row")
      		.attr("transform", function(d, i) { return "translate(0," + ((i + 1) * txt.ysize) + ")"; });

  	row.append("line")
		.attr("x2", width);

  	row.append("text")
		.attr("x", -5)
		.attr("y", function(d, i) { return -txt.ysize / 2; })
		.attr("dy", ".32em")
		.attr("text-anchor", "end")
		.text(function(d, i) { return functions[i].substring(0, 20); });


	var column = svg.selectAll(".column")
		.data(times)
    	.enter().append("g")
      		.attr("class", "column")
      		.attr("transform", function(d, i) { return "translate(" + ((i + 1) * txt.xsize) + ")rotate(-90)"; });

  	column.append("line")
    	.attr("x1", -width);

	column.append("text")
		.attr("x", 5)
      	.attr("y", -txt.ysize / 2)
      	.attr("dy", ".32em")
      	.attr("text-anchor", "start")
      	.text(function(d, i) { return times[i]; });

    var filesandfunctions = [];

	table.forEach(function(line) {

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
		    .on("mouseover", function() { d3.select(this).style("stroke", "#4A0D3F").style("stroke-width", 3);  })
		    .on("mouseout", function() { d3.select(this).style("stroke", "none"); })
		filesandfunctions[filesandfunctions.length] = ["data/" + line.num + ".json", line.func, line.num];
	});
	function_details(filesandfunctions);
});
}; // function

invalidate();

