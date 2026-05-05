import * as Plot from "@observablehq/plot";
import {JSDOM} from "jsdom";

const data = [
    {x:0, y:1},
    {x:1, y:2},
    {x:2, y:3},
    {x:3, y:4},
    {x:4, y:8},
    {x:5, y:5},
    {x:6, y:6},
    {x:7, y:7},
    {x:8, y:8},
    {x:9, y:9},
    {x:10, y:10}
];

const plot = Plot.plot({
  document: new JSDOM("").window.document, // const {document} = new JSDOM("<!DOCTYPE html").window.document
  marks: [
    Plot.lineY(data, {x: "time (s)", y: "random value"})
  ]
});

process.stdout.write(plot.outerHTML); // return only plot.outHTML for angular imple.