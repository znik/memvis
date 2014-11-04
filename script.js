//
//
//

// 9 - intensity of working with each CL
// ?10 - false sharing candidates

var threads_num = 29;

var folder_1st = "data";
var main_lower = 1000;
var details_lower = 0;


var show_vars = function(filename) {
  d3.json(filename, function(error, data) {


  var margin = {top: 40, right: 40, bottom: 0, left: 40}; 

  var width = 1200 // / 4,
      height = 400 / 8;

  var center = {x: margin.left + width / 2, y: margin.top + height / 2};

 
  data = data.filter(function(d) {
    return d["refs"] > +details_lower;
  });

 if (data.length == 0)
    return;

  var max_refs = 0;
  data.forEach(function(d) {
    var p = +d["refs"];
    if (p > max_refs)
      max_refs = p;
  });

  var pane_height = height;
  var main = d3.select("body").select("#main");

  var svg = main.append("svg")
    .attr("x", 0)
    .attr("y", 0)
    .attr('class', 'detailed_view')
    .attr("height", pane_height + 30 + 10 * threads_num)
    .attr("width", width)
    .append("g");

  var pane = svg.append("rect")
    .attr('class', 'backpaper')
    .attr("fill", "none")
    .attr("x", 0)
    .attr("y", 0)
    .attr("height", pane_height)
    .attr("width", width - 2);

  data.forEach(function(d, i) {
    var draw_height = pane_height; // - margin.top - margin.bottom;
    var item_width = (width) / data.length;
    if (item_width > 40)
      item_width = 40;
    var x = item_width * i;
    var y = draw_height - draw_height * d["refs"] / max_refs;
    var col = 255 - 255 * (+d["refs"]) / max_refs;
    var color = d3.rgb(255, col, col);

      var xs_bar = svg.append("rect")
        .attr('class', 'xs_bar')
        .datum([x, y, d])
        .attr("fill", color)
        .attr("x", x)
        .attr("y", y)
        .attr("height", draw_height - y)
        .attr("width", item_width - 1)
        .on("click", function(d) {
          // FIXME !
          //main.selectAll("svg").remove();
          //console.log("Loading... " + folder_1st + parseInt(d[0]) + ".json");
          //show_accesses(folder_2nd + parseInt(d[0]/1024) + ".json");
        })
        .on("mouseover", function(d) {
          svg.append("rect")
            .attr('class', 'xs_highlight')
            .attr("fill", "none")
            .attr("x", d[0] - 1)
            .attr("y", d[1] - 1)
            .attr("stroke", "black")
            .attr("height", draw_height - d[1] + 2)
            .attr("width", item_width + 2);

            var r = svg.append("g")
              //.datum(d[3])
              .attr('class', 'hint')
              .attr("fill", "none")
              .attr("stroke", "black");

            var x_t = d[0] + 30, y_t = d[1] + 40;
            if (x_t > center.x)
              x_t -= 100;

            var t = r.append("text")
              .attr("x", x_t)
              .attr("y", y_t)
              .attr("dy", ".35em")
              //.text(d[2]["refs"]);
            

            var span = t.append("tspan");
            span.datum(d[2]).attr("x", 0).attr("y", pane_height + 10).html(function(d) {
              return "Address: " + d["addr"];
            })
            span = t.append("tspan");
            span.datum(d[2]).attr("x", 0).attr("y", pane_height + 20).html(function(d) {
              return "AllRefs: " + d["refs"];
            })


            for (var j = 0; j < threads_num; j++) {
              //var prop = "inf" + j;
              var span = t.append("tspan");
              span.datum([d[2], j]).attr("x", 0).attr("y", pane_height + 10 * (j+3)).html(function(d) {
                var str = d[0]["inf" + d[1]];
                //console.log("inf" + d[1], str);
                return d[1] + "(" + d[0]["ref" + d[1]] + "):" +
                  str.replace("<", "[").replace("<", "[").replace(">", "]").replace(">", "]"); });
            }
            //t.append("tspan").attr("x", 0).attr("y", pane_height+20).html(function(d) {
            //  return "count:" + d["count"];  });
        })               
        .on("mouseout", function(d) {
            svg.select(".xs_highlight").remove();
            svg.select(".hint").remove();
        });
  });
  });
};

var show_accesses = function(filename) {
  d3.json(filename, function(error, data) {

  var margin = {top: 30, right: 40, bottom: 30, left: 40};

  var width = 1200 - margin.left - margin.right,
      height = 400 / 4 - margin.top - margin.bottom;

  var center = {x: margin.left + width / 2, y: margin.top + height / 2};

  var max_refs = 0;
  data.forEach(function(d) {
      var p = +d["max"];
      if (p > max_refs)
        max_refs = p;
  });


//  var slider = d3.select("body").select("#slider");
//  slider.attr("min", min_refs).attr("max", max_refs);

  data = data.filter(function(d) {
    return d["max"] > +main_lower;
  });

  var pane_height = height;
	var main = d3.select("body").select("#main");
	var svg = main.append("svg")
    .attr("x", 0)
  	.attr("y", 0)
  	.attr("height", height + margin.top + margin.bottom)
    .attr("width", width)
    .append("g");

/*
  d3.select(".main_view")
    .selectAll("div")
    .data(data)
    .enter().append("div")
    //.style("background-color", function(d) { return d3.rgb(255 - 255 * d["max"] / max_refs, 255 - 255 * d["max"] / max_refs, 255); })
    .style("width", function(d) { return 50 + 50 * d["max"] / max_refs + "px"; })
    .text(function(d) { return d["max"]; })
    .on("click", function(d) {
        var subview = d3.select(this).append("div");
        show_vars(folder_1st + parseInt(d["num"]) + ".json", subview);
    });
*/

 	data.forEach(function(d, i) {
    var item_width = (width - margin.left - margin.right) / data.length;
    var x = item_width * i;

    var y = margin.top;
 
    var col = 255 - 255 * d["max"] / max_refs; //255 - 255 * (+d[fields[j]]) / max_refs;
    var color = d3.rgb(255, col, col);

    if (+d["max"] > 1000)
      show_vars(folder_1st + d["num"] + ".json");

	  var xs_bar = svg.append("rect")
      .attr('class', 'xs_bar')
      .datum([d["num"], margin.left + x, y, d, d["max"]])
      .attr("fill", color)
  		.attr("x", margin.left + x)
			.attr("y", y)
  		.attr("height", pane_height)
  		.attr("width", item_width - 1)
      .on("click", function(d) {
        main.selectAll(".detailed_view").remove();
        svg.select(".click_highlight").remove();
        svg.select(".click_hint").remove();
        svg.append("rect")
          .attr('class', 'click_highlight')
          .attr("fill", "none")
          .attr("x", d[1] - 1)
          .attr("y", d[2] - 1)
          .attr("stroke", "black")
          .attr("height", pane_height + 2)
          .attr("width", item_width + 2 - 1);

        var r = svg.append("g")
          .datum(d[3])
          .attr('class', 'click_hint')
          .attr("fill", "none")
          .attr("stroke", "black");

        var x_t = d[1], y_t = d[2] + 50;

        var t = r.append("text")
          .attr("x", x_t)
          .attr("y", y_t)
          .attr("dy", ".35em")
          .text("["+d[0]+"]"+d[4]);   


        console.log("Loading... " + folder_1st + parseInt(d[0]) + ".json");
        show_vars(folder_1st + parseInt(d[0]) + ".json");
      })
      .on("mouseover", function(d) {
        svg.append("rect")
          .attr('class', 'xs_highlight')
          .attr("fill", "none")
          .attr("x", d[1] - 1)
          .attr("y", d[2] - 1)
          .attr("stroke", "black")
          .attr("height", pane_height + 2)
          .attr("width", item_width + 2 - 1);

        var r = svg.append("g")
          .datum(d[3])
          .attr('class', 'hint')
          .attr("fill", "none")
          .attr("stroke", "black");

        var x_t = d[1], y_t = d[2] + 60;

        var t = r.append("text")
          .attr("x", x_t)
          .attr("y", y_t)
          .attr("dy", ".35em")
          .text("["+d[0]+"]"+d[4])            
        })               
        .on("mouseout", function(d) {
            svg.select(".xs_highlight").remove();
            svg.select(".hint").remove();
        });
	});

  });
};


var showVal = function(value) {
  console.log(value);
  var main = d3.select("body").select("#main");
  main.selectAll("svg").remove();
  main_lower = value;
  show_accesses(folder_1st + "main.json");
}

var showVal2 = function(value) {
  console.log(value);
  var main = d3.select("body").select("#main");
  main.selectAll("svg").remove();
  details_lower = value;
  show_accesses(folder_1st + "main.json");
}


show_accesses(folder_1st + "main.json");
